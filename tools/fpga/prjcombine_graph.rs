// Public architecture adapter for Project Combine 234343d. No firmware input.
use prjcombine_entity::{EntityBundleItemIndex, EntityId};
use prjcombine_interconnect::{db::*, grid::*};
use prjcombine_types::bsdata::{PolTileBit, TileBit};
use prjcombine_virtex::{db::Database, expanded::ExpandedDevice};
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
    json!({"x":t.col.to_idx(), "y":t.row.to_idx(), "bel":ed.db.bel_slots.key(bs), "class":ed.db.tile_classes.key(ed[t].class), "inputs":inputs, "outputs":outputs, "attributes":attrs})
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
                    json!({"kind":"mux", "bits":m.bits.iter().map(|&b| bit(ed,t,b,false)).collect::<Vec<_>>(), "expected":m.src[&src].iter().collect::<Vec<_>>(), "off":m.bits_off.as_ref().map(|v| v.iter().collect::<Vec<_>>())})
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
fn query(ed: &ExpandedDevice, q: &Value) -> Value {
    let x = q["x"].as_u64().unwrap() as usize;
    let y = q["y"].as_u64().unwrap() as usize;
    assert!(x < ed.chip.columns && y < ed.chip.rows);
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
    json!({"root":wire(ed.db,root), "tree":tree.iter().map(|&w| wire(ed.db,w)).collect::<Vec<_>>(), "pips":ed.wire_pips_bwd(root).iter().map(|p| pip_config(ed,p)).collect::<Vec<_>>(), "terminals":terminals, "wire_kind":format!("{:?}",ed.db[root.slot])})
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
