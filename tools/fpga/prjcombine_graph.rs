// Public architecture adapter for Project Combine 234343d. No firmware input.
use prjcombine_entity::{EntityBundleItemIndex, EntityId};
use prjcombine_interconnect::{db::*, grid::*};
use prjcombine_types::bsdata::{PolTileBit, TileBit};
use prjcombine_virtex::{db::Database, defs::{bslots, wire_from_mux}, expanded::ExpandedDevice};
use prjcombine_xilinx_bitstream::BitRect;
use serde_json::{Value, json};
use std::io::{self, BufRead, Write};

fn pin_name((name, index): (&str, EntityBundleItemIndex)) -> String {
    match index {
        EntityBundleItemIndex::Single => name.to_string(),
        EntityBundleItemIndex::Array { index, .. } => format!("{name}[{index}]"),
    }
}

fn wire(db: &IntDb, w: WireCoord) -> Value {
    json!([w.col.to_idx(), w.row.to_idx(), db.wires.key(w.slot)])
}
fn bit(ed: &ExpandedDevice, t: TileCoord, b: TileBit, inv: bool) -> Value {
    match ed.tile_bits(t)[b.rect] {
        BitRect::Main(_, f, width, base, height, flip) => {
            assert!(b.frame.to_idx() < width && b.bit.to_idx() < height);
            let offset = if flip {
                height - 1 - b.bit.to_idx()
            } else {
                b.bit.to_idx()
            };
            json!({"frame": f + b.frame.to_idx(), "bit": base + offset, "inv": inv})
        }
        _ => json!({"unresolved": format!("{b:?}"), "inv": inv}),
    }
}
fn pbit(ed: &ExpandedDevice, t: TileCoord, b: PolTileBit) -> Value {
    bit(ed, t, b.bit, b.inv)
}
fn tw(ed: &ExpandedDevice, t: TileCoord, w: TileWireCoord) -> Value {
    ed.resolve_tile_wire(t, w)
        .map(|w| wire(ed.db, w))
        .unwrap_or(Value::Null)
}
fn bel(ed: &ExpandedDevice, t: TileCoord, bs: BelSlotId, b: &Bel) -> Value {
    let BelKind::Class(bc) = ed.db.bel_slots[bs].kind else {
        panic!("untyped bel")
    };
    let cls = &ed.db.bel_classes[bc];
    let mut inputs = serde_json::Map::new();
    for (id, &input) in &b.inputs {
        let (inv, control) = match input {
            BelInput::Fixed(p) => (p.inv, Value::Null),
            BelInput::Invertible(_, pb) => (false, pbit(ed, t, pb)),
        };
        inputs.insert(
            pin_name(cls.inputs.key(id)),
            json!({"wire": tw(ed,t,input.wire()), "inv": inv, "inversion_bit": control}),
        );
    }
    let mut outputs = serde_json::Map::new();
    for (id, wires) in &b.outputs {
        outputs.insert(
            pin_name(cls.outputs.key(id)),
            json!(wires.iter().map(|&w| tw(ed, t, w)).collect::<Vec<_>>()),
        );
    }
    let mut attrs = serde_json::Map::new();
    for (id, a) in &b.attributes {
        let value = match a {
            BelAttribute::BitVec(bits) => {
                json!({"bits": bits.iter().map(|&b| pbit(ed,t,b)).collect::<Vec<_>>()})
            }
            BelAttribute::Enum(e) => {
                let BelAttributeType::Enum(ec) = cls.attributes[id].typ else {
                    panic!("enum type")
                };
                let values: serde_json::Map<String, Value> = e
                    .values
                    .iter()
                    .map(|(id, v)| {
                        (
                            ed.db.enum_classes[ec].values[id].clone(),
                            json!(v.iter().collect::<Vec<_>>()),
                        )
                    })
                    .collect();
                json!({"bits": e.bits.iter().map(|&b| bit(ed,t,b,false)).collect::<Vec<_>>(), "values": values})
            }
        };
        attrs.insert(cls.attributes.key(id).clone(), value);
    }
    let mut dedicated = json!({});
    if ed.db.bel_slots.key(bs).starts_with("SLICE[") {
        // Pinned rdverify/virtex::verify_slice: CIN uses the same slice's
        // preceding-row COUT, only when that BEL actually exists in the grid.
        dedicated["CIN"] = ed
            .bel_delta(t.cell, 0, -1, bs)
            .map(|prev| {
                json!({"x":prev.col.to_idx(), "y":prev.row.to_idx(),
                   "bel":ed.db.bel_slots.key(prev.slot), "pin":"COUT"})
            })
            .unwrap_or(Value::Null);
        dedicated["evidence"] = json!("re/xilinx/rdverify/virtex/src/lib.rs::verify_slice");
    } else if bs == bslots::PCILOGIC {
        // verify_pcilogic connects hidden ready inputs to PCIIOB.PCI taps.
        // Their electrical/timing behavior is not an alias for IOI.I or IQ.
        for (name, row, index) in [
            ("IRDY", ed.chip.row_mid(), 3),
            ("TRDY", ed.chip.row_mid() - 1, 1),
        ] {
            dedicated[name] = json!({"x":t.col.to_idx(), "y":row.to_idx(),
                "bel":format!("IOI[{index}]"), "pin":"PCI"});
        }
        dedicated["evidence"] = json!("re/xilinx/rdverify/virtex/src/lib.rs::verify_pcilogic; verify_iob");
    }
    json!({"x":t.col.to_idx(), "y":t.row.to_idx(), "bel":ed.db.bel_slots.key(bs), "class":ed.db.tile_classes.key(ed[t].class), "inputs":inputs, "outputs":outputs, "attributes":attrs, "dedicated":dedicated})
}
fn pip_config(ed: &ExpandedDevice, p: &TilePip) -> Value {
    let t = p.tile;
    let mut matches = vec![];
    for info in ed.db[ed[t].class].bels.values() {
        let BelInfo::SwitchBox(sb) = info else {
            continue;
        };
        for item in &sb.items {
            let dst = p.tile_wire_out;
            let src = PolTileWireCoord {
                tw: p.tile_wire_in,
                inv: p.inv,
            };
            let v = match item {
                SwitchBoxItem::Mux(m) if m.dst == dst && m.src.contains_key(&src) => {
                    let mut value = json!({"kind":"mux", "bits":m.bits.iter().map(|&b| bit(ed,t,b,false)).collect::<Vec<_>>(), "expected":m.src[&src].iter().collect::<Vec<_>>(), "off":m.bits_off.as_ref().map(|v| v.iter().collect::<Vec<_>>())});
                    // HEX *_MUX is a tile-owned pre-buffer helper, not the shared
                    // multi-root conductor. Overlapping IO/BRAM tile cells can
                    // share its abstract coordinate without sharing its driver.
                    let name = ed.db.wires.key(dst.wire);
                    if name.starts_with("HEX_") && name.contains("_MUX[") {
                        let conductor = wire_from_mux(dst.wire).expect("HEX mux conductor");
                        let buffers = sb.items.iter().filter_map(|item| match item {
                            SwitchBoxItem::ProgBuf(b) if b.src.tw == dst
                                && b.dst.cell == dst.cell && b.dst.wire == conductor =>
                                Some(json!({"kind":"buffer", "bits":[pbit(ed,t,b.bit)],
                                            "expected":[true]})),
                            _ => None,
                        }).collect::<Vec<_>>();
                        value["owner_buffers"] = json!(buffers);
                    }
                    value
                }
                SwitchBoxItem::PermaBuf(b) if b.dst == dst && b.src == src => {
                    json!({"kind":"permanent", "bits":[], "expected":[]})
                }
                SwitchBoxItem::ProgBuf(b) if b.dst == dst && b.src == src => {
                    json!({"kind":"buffer", "bits":[pbit(ed,t,b.bit)], "expected":[true]})
                }
                SwitchBoxItem::Pass(b) if b.dst == dst && b.src == src.tw => {
                    json!({"kind":"pass", "bits":[pbit(ed,t,b.bit)], "expected":[true]})
                }
                SwitchBoxItem::BiPass(b)
                    if (b.a == dst && b.b == src.tw) || (b.b == dst && b.a == src.tw) =>
                {
                    json!({"kind":"bipass", "bits":[pbit(ed,t,b.bit)], "expected":[true]})
                }
                SwitchBoxItem::ProgInv(b) if b.dst == dst && b.src == src.tw => {
                    json!({"kind":"inverter", "bits":[pbit(ed,t,b.bit)], "expected":[p.inv]})
                }
                _ => continue,
            };
            matches.push(v);
        }
    }
    json!({"source":wire(ed.db,p.wire_in), "source_raw":wire(ed.db,p.wire_in_raw), "destination_raw":wire(ed.db,p.wire_out_raw), "tile":[t.col.to_idx(),t.row.to_idx(),ed.db.tile_classes.key(ed[t].class)], "inv":p.inv, "config":matches})
}
fn bus_row(ed: &ExpandedDevice, y: usize) -> Value {
    let mut columns = vec![];
    let mut fixed = vec![];
    let mut joiners = vec![];
    for x in 0..ed.chip.columns {
        let cell = CellCoord {
            die: DieId::from_idx(0),
            col: ColId::from_idx(x),
            row: RowId::from_idx(y),
        };
        let slot = if x == 0 || x == ed.chip.columns - 1 {
            bslots::TBUS_WE
        } else {
            bslots::TBUS
        };
        if !ed.has_bel(cell.bel(slot)) {
            continue;
        }
        let tile = ed.bel_tile(cell.bel(slot));
        let BelInfo::Bel(b) = &ed.db[ed[tile].class].bels[slot] else {
            unreachable!()
        };
        let tbufs = bslots::TBUF
            .into_iter()
            .map(|bs| {
                let tile = ed.bel_tile(cell.bel(bs));
                let BelInfo::Bel(b) = &ed.db[ed[tile].class].bels[bs] else {
                    unreachable!()
                };
                bel(ed, tile, bs, b)
            })
            .collect::<Vec<_>>();
        columns.push(json!({"x":x, "bus":bel(ed,tile,slot,b), "tbufs":tbufs}));
        // Exact dedicated nets from rdverify/virtex::{verify_tbus,verify_tbus_we}.
        if x < ed.chip.columns - 1 {
            let next = if x == 0 || ed.chip.cols_bram.contains(&(cell.col + 1)) {
                x + 2
            } else {
                x + 1
            };
            for lane in 0..3 {
                fixed.push(json!([[x, lane], [next, lane + 1]]));
            }
            fixed.push(json!([[x, 4], [next, 0]])); // 4 denotes BUS3_E.
        }
        if x == 0 {
            joiners.push(json!({"a":[x,3],"b":[x,4],"owner":x,"attribute":"JOINER"}));
        } else if x < ed.chip.columns - 1 {
            // Fuzzer ClbTbusRight: JOINER_E is owned by the preceding bus tile.
            let owner = columns[columns.len() - 2]["x"].as_u64().unwrap();
            joiners.push(json!({"a":[x,3],"b":[x,4],"owner":owner,"attribute":"JOINER_E"}));
        }
    }
    json!({"columns":columns,"fixed":fixed,"joiners":joiners,
           "evidence":"re/xilinx/rdverify/virtex/src/lib.rs::verify_tbus; re/xilinx/ise-hammer/src/virtex/tbus.rs::ClbTbusRight"})
}
fn query(ed: &ExpandedDevice, q: &Value) -> Value {
    let x = q["x"].as_u64().unwrap() as usize;
    let y = q["y"].as_u64().unwrap() as usize;
    assert!(x < ed.chip.columns && y < ed.chip.rows);
    if q.get("bus_row").and_then(Value::as_bool) == Some(true) {
        return bus_row(ed, y);
    }
    let cell = CellCoord {
        die: DieId::from_idx(0),
        col: ColId::from_idx(x),
        row: RowId::from_idx(y),
    };
    if let Some(name) = q["bel"].as_str() {
        let bs = ed.db.bel_slots.get(name).expect("unknown bel").0;
        let t = ed.bel_tile(cell.bel(bs));
        let BelInfo::Bel(b) = &ed.db[ed[t].class].bels[bs] else {
            panic!("not typed bel")
        };
        return bel(ed, t, bs, b);
    }
    let raw = cell.wire(ed.db.get_wire(q["wire"].as_str().unwrap()));
    let Some(root) = ed.resolve_wire(raw) else {
        return json!({"root":null,"pips":[],"terminals":[]});
    };
    let mut tree = ed.wire_tree(root);
    tree.sort();
    tree.dedup();
    let mut terminals = vec![];
    for &w in &tree {
        for &(t, _) in &ed[w.cell].tile_index {
            for (bs, info) in &ed.db[ed[t].class].bels {
                if let BelInfo::Bel(b) = info {
                    let BelKind::Class(bc) = ed.db.bel_slots[bs].kind else {
                        continue;
                    };
                    for (id, wires) in &b.outputs {
                        if wires
                            .iter()
                            .any(|&tw| ed.resolve_tile_wire(t, tw) == Some(root))
                        {
                            terminals.push(json!({"x":t.col.to_idx(),"y":t.row.to_idx(),"bel":ed.db.bel_slots.key(bs),"pin":pin_name(ed.db.bel_classes[bc].outputs.key(id))}));
                        }
                    }
                }
            }
        }
    }
    terminals.sort_by_key(|v| v.to_string());
    terminals.dedup();
    // wire_pips_bwd expands MultiRoot trees itself, but not Regional wires.
    // Regional taps can be placed away from the canonical region root.
    let pips = if matches!(ed.db[root.slot], WireKind::Regional(_)) {
        tree.iter()
            .flat_map(|&w| ed.wire_pips_bwd(w))
            .collect::<Vec<_>>()
    } else {
        ed.wire_pips_bwd(root)
    };
    json!({"root":wire(ed.db,root), "tree":tree.iter().map(|&w| wire(ed.db,w)).collect::<Vec<_>>(), "pips":pips.iter().map(|p| pip_config(ed,p)).collect::<Vec<_>>(), "terminals":terminals, "wire_kind":format!("{:?}",ed.db[root.slot])})
}
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let path = std::env::args().nth(1).expect("virtex.zstd path");
    let db = Database::from_file(path)?;
    let dev = db.devices.iter().find(|d| d.name == "xc2s200e").unwrap();
    let ed = db.chips[dev.chip].expand_grid(&dev.disabled, &db.int);
    for line in io::stdin().lock().lines() {
        let q: Value = serde_json::from_str(&line?)?;
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| query(&ed, &q)))
            .unwrap_or_else(|_| json!({"error":"unsupported or invalid architecture query"}));
        println!("{result}");
        io::stdout().flush()?;
    }
    Ok(())
}
