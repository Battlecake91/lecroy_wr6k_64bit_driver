"""Synthetic tests only. No firmware bytes or firmware-derived fixtures."""
import sys
import unittest
import itertools
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "fpga"))
from virtexe_routing import (COMMIT, Router, LogicAnalyzer, classify_pip, compact_report,
                            bus_component, data_support, decode_bel, evaluate_data, lut_support, resolve_bus,
                            cone_summary, memory_step, register_contract, register_step, state_table, verify_checkout,
                            local_equivalence, reconstruction_summary, register_equivalence, validate_pci_relations)


def pip(source, kind="pass", expected=True, inv=False):
    return {"source": [0, 0, source], "inv": False, "config": [{"kind": kind,
            "bits": [{"frame": 0, "bit": 0, "inv": inv}], "expected": [expected]}]}


class FakeArchitecture:
    def __init__(self, graph, terminals=None):
        self.graph, self.terminals = graph, terminals or {}

    def query(self, x, y, wire):
        return {"root": [x, y, wire], "tree": [[x, y, wire]],
                "pips": self.graph.get(wire, []), "terminals": self.terminals.get(wire, [])}


class CarryRouter:
    def __init__(self, cy0="CONST_0", self_="F", selg="G", init="BX", yb="GCY"):
        self.profile = {"CY0": cy0, "CYSELF": self_, "CYSELG": selg, "CYINIT": init, "YBMUX": yb}

    def trace(self, wire):
        return {"driver_status": "resolved", "constants": [], "inversion": False,
                "terminals": [{"x": wire[0], "y": wire[1], "bel": "INPUT", "pin": wire[2]}]}

    def bel(self, x, y, name):
        if name == "INPUT":
            return {"attributes": {}, "inputs": {}}
        profile = self.profile if y == 2 else {"CYINIT": "BX", "CYSELF": "CONST_1", "CYSELG": "CONST_1"}
        attrs = {k: {"selections": [v]} for k, v in dict(FXMUX="FXOR", GYMUX="GXOR", **profile).items()}
        attrs.update({"F": {"value": 0xAAAA}, "G": {"value": 0xAAAA}})
        for letter in "FG":
            for suffix in ("_RAM_ENABLE", "_SHIFT_ENABLE"):
                attrs[letter + suffix] = {"value": 0}
        return {"attributes": attrs, "inputs": {p: {"wire": [x, y, p], "inv": False}
                for p in ("F1", "F2", "G1", "G2", "BX", "BY")},
                "dedicated": {"CIN": {"x": 0, "y": 1, "bel": "SLICE[0]", "pin": "COUT"}}}


def synthetic_bus_row():
    columns = []
    for x in (0, 2, 3, 5, 6):
        columns.append({"x": x, "bus": {"attributes": {n: {"value": 0} for n in ("JOINER", "JOINER_E")}},
                        "tbufs": [{"y": 7, "bel": f"TBUF[{i}]", "attributes": {
                            n: {"value": 0} for n in ("OUT_A", "OUT_B")}} for i in range(2)]})
    fixed = []
    for a, b in zip((0, 2, 3, 5), (2, 3, 5, 6)):
        fixed.extend([[[a, lane], [b, lane + 1]] for lane in range(3)])
        fixed.append([[a, 4], [b, 0]])
    gates = [{"a": [x, 3], "b": [x, 4], "owner": owner, "attribute": attr}
             for x, owner, attr in ((0, 0, "JOINER"), (2, 0, "JOINER_E"),
                                   (3, 2, "JOINER_E"), (5, 3, "JOINER_E"))]
    return {"columns": columns, "fixed": fixed, "joiners": gates, "evidence": "synthetic"}


class RoutingTests(unittest.TestCase):
    def setUp(self):
        self.image = SimpleNamespace(frames=[[1, 0]])

    def test_enabled_disabled_and_inverted_bits(self):
        self.assertEqual(classify_pip(self.image, pip("a"))["status"], "active")
        self.assertEqual(classify_pip(self.image, pip("a", inv=True))["status"], "disabled")
        self.assertEqual(classify_pip(self.image, pip("a", expected=False))["status"], "disabled")

    def test_missing_or_duplicate_encoding_is_unknown(self):
        p = pip("a")
        p["config"] *= 2
        self.assertEqual(classify_pip(self.image, p)["status"], "unknown")
        p["config"] = []
        self.assertEqual(classify_pip(self.image, p)["status"], "unknown")

    def test_hex_mux_ownership_enabled_disabled_and_unknown(self):
        p = pip("selected", "mux")
        guard = pip("owner", "buffer")["config"][0]
        p["config"][0]["owner_buffers"] = [guard]
        self.assertEqual(classify_pip(self.image, p)["status"], "active")
        guard["bits"][0]["bit"] = 1
        self.assertEqual(classify_pip(self.image, p)["status"], "disabled")
        guard["bits"][0]["frame"] = 9999
        self.assertEqual(classify_pip(self.image, p)["status"], "unknown")
        for guards in ([], [guard, guard]):
            p["config"][0]["owner_buffers"] = guards
            self.assertEqual(classify_pip(self.image, p)["status"], "unknown")

    def test_overlapping_hex_muxes_do_not_select_convenient_driver(self):
        io, bram = pip("io", "mux"), pip("bram", "mux")
        for p, index in ((io, 0), (bram, 1)):
            p["config"][0].update(bits=[], expected=[])
            guard = pip("owner", "buffer")["config"][0]
            guard["bits"][0]["bit"] = index
            p["config"][0]["owner_buffers"] = [guard]
        terminal = lambda name: {"x": 0, "y": 0, "bel": name, "pin": "Q"}
        arch = FakeArchitecture({"shared_mux": [io, bram]},
                                {"io": [terminal("IO")], "bram": [terminal("BRAM")]})
        for bits, expected in (([1, 0], "IO"), ([0, 1], "BRAM")):
            result = Router(SimpleNamespace(frames=[bits]), arch).trace([0, 0, "shared_mux"])
            self.assertEqual(result["driver_status"], "resolved")
            self.assertEqual(result["terminals"][0]["bel"], expected)
        result = Router(SimpleNamespace(frames=[[1, 1]]), arch).trace([0, 0, "shared_mux"])
        self.assertEqual(result["driver_status"], "ambiguous")

    def test_unavailable_frame_is_unknown(self):
        p = pip("a")
        p["config"][0]["bits"][0]["frame"] = 2241
        self.assertEqual(classify_pip(self.image, p)["status"], "unknown")

    def test_only_active_mux_branch_is_traversed(self):
        arch = FakeArchitecture({"dst": [pip("PULLUP", "mux"), pip("disabled", "mux", False)]})
        result = Router(self.image, arch).trace([0, 0, "dst"])
        self.assertEqual(len(result["nodes"]), 2)
        self.assertEqual(result["signal"], [0, 0, "dst"])
        self.assertEqual(result["driver_status"], "resolved")

    def test_cycle_is_bounded_and_not_a_driver(self):
        arch = FakeArchitecture({"a": [pip("b")], "b": [pip("a")]})
        result = Router(self.image, arch).trace([0, 0, "a"])
        self.assertEqual(len(result["cycles"]), 1)
        self.assertEqual(len(result["nodes"]), 2)
        self.assertEqual(result["driver_status"], "unknown")

    def test_ambiguity_counts_distinct_producers(self):
        terms = {n: [{"x": 0, "y": 0, "bel": n, "pin": "Q"}] for n in ("a", "b")}
        arch = FakeArchitecture({"dst": [pip("a"), pip("b")]}, terms)
        result = Router(self.image, arch).trace([0, 0, "dst"])
        self.assertEqual(result["driver_status"], "ambiguous")

    def test_reconvergent_paths_are_one_driver(self):
        arch = FakeArchitecture({"dst": [pip("a"), pip("b")], "a": [pip("q")], "b": [pip("q")]},
                                {"q": [{"x": 0, "y": 0, "bel": "ff", "pin": "Q"}]})
        self.assertEqual(Router(self.image, arch).trace([0, 0, "dst"])["driver_status"], "resolved")

    def test_route_inversion_is_preserved(self):
        inverted = pip("PULLUP")
        inverted["inv"] = True
        router = Router(self.image, FakeArchitecture({"a": [inverted]}))
        self.assertTrue(router.trace([0, 0, "a"])["inversion"])
        pin = LogicAnalyzer(router).input({"wire": [0, 0, "a"], "inv": True})
        self.assertFalse(pin["inv"])

    def test_conflicting_polarities_are_ambiguous(self):
        inverted = pip("q")
        inverted["inv"] = True
        arch = FakeArchitecture({"dst": [pip("a"), pip("b")], "a": [pip("q")], "b": [inverted]},
                                {"q": [{"x": 0, "y": 0, "bel": "ff", "pin": "Q"}]})
        result = Router(self.image, arch).trace([0, 0, "dst"])
        self.assertEqual(result["driver_status"], "ambiguous")
        self.assertIsNone(result["inversion"])

    def test_limits_are_explicit(self):
        router = Router(self.image, FakeArchitecture({"a": [pip("b")], "b": [pip("PULLUP")]}))
        for kwargs in ({"max_nodes": 1}, {"max_depth": 0}):
            result = router.trace([0, 0, "a"], **kwargs)
            self.assertTrue(result["limits"])
            self.assertEqual(result["driver_status"], "unknown")

    def test_deep_graph_does_not_use_python_recursion(self):
        graph = {f"w{i}": [pip(f"w{i+1}")] for i in range(1100)}
        graph["w1100"] = [pip("PULLUP")]
        result = Router(self.image, FakeArchitecture(graph)).trace([0, 0, "w0"], max_nodes=1200, max_depth=1200)
        self.assertEqual(result["driver_status"], "resolved")
        self.assertEqual(len(result["nodes"]), 1102)

    def test_lut_support_excludes_unused_pins(self):
        self.assertEqual(lut_support(0xAAAA), [0])
        self.assertEqual(lut_support(0xFFFF), [])

    def test_feedback_network_stops_at_existing_register(self):
        class FakeRouter:
            def trace(self, signal):
                return {"driver_status": "resolved", "constants": [],
                        "terminals": [{"x": 0, "y": 0, "bel": "SLICE[0]", "pin": "XQ"}]}

            def bel(self, x, y, name):
                pin = {"wire": [0, 0, "q"], "inv": False}
                return {"attributes": {"DXMUX": {"selections": ["X"]},
                        "FXMUX": {"selections": ["F"]}, "F": {"value": 0xAAAA},
                        "F_RAM_ENABLE": {"value": 0}, "F_SHIFT_ENABLE": {"value": 0}},
                        "inputs": {p: pin for p in ("F1", "CLK", "CE", "SR")}}
        analyzer = LogicAnalyzer(FakeRouter())
        root = analyzer.output({"x": 0, "y": 0, "bel": "SLICE[0]", "pin": "XQ"})
        report = analyzer.report([root])
        self.assertEqual(len(report["logic"]), 1)
        self.assertEqual(report["logic"][root]["data"]["inputs"]["0"]["source"], root)

    def test_bel_lsb_order_and_polarity(self):
        bel = {"inputs": {"SR": {"wire": [0, 0, "reset"], "inv": False,
                                  "inversion_bit": {"frame": 0, "bit": 1, "inv": True}}},
               "attributes": {"LUT": {"bits": [{"frame": 0, "bit": 0}, {"frame": 0, "bit": 1}]},
                              "MUX": {"bits": [{"frame": 0, "bit": 0}], "values": {"A": [True], "B": [False]}}}}
        decoded = decode_bel(self.image, bel)
        self.assertEqual(decoded["attributes"]["LUT"]["value"], 1)
        self.assertEqual(decoded["attributes"]["MUX"]["selections"], ["A"])
        self.assertTrue(decoded["inputs"]["SR"]["inv"])

    def test_state_table_expands_combination_but_stops_at_registers(self):
        logic = {"q": {"kind": "register", "data": {"source": "c"}},
                 "p": {"kind": "pad_input"},
                 "c": {"kind": "combinational", "data": {"op": "mux",
                       "select": {"source": "p"}, "zero": {"source": "q", "inv": True},
                       "one": {"constant": 1}}}}
        table = state_table({"logic": logic}, "q")
        self.assertEqual(table["inputs"], ["p", "q"])
        self.assertEqual([r["d"] for r in table["rows"]], [1, 1, 0, 1])
        self.assertIn("apply CE, SR and clock separately", table["semantics"])

    def test_local_table_correlates_repeated_sources(self):
        data = {"op": "lut4", "init": 0x6666,
                "inputs": {"0": {"source": "a"}, "1": {"source": "a"}}}
        table = state_table({"logic": {"q": {"data": data}}}, "q", local=True)
        self.assertEqual(table["inputs"], ["a"])
        self.assertEqual([r["d"] for r in table["rows"]], [0, 0])

    def test_truth_table_limits_and_architecture_boundaries_are_unknown(self):
        logic = {"q": {"data": {"source": "a"}}, "a": {"kind": "architecture_boundary"}}
        self.assertEqual(state_table({"logic": logic}, "q")["status"], "unknown")
        self.assertEqual(state_table({"logic": logic}, "q", max_inputs=0, local=True)["status"], "unknown")
        self.assertEqual(state_table({"logic": logic}, "a")["status"], "unknown")
        with self.assertRaises(ValueError):
            evaluate_data({"unknown": "carry"}, {}, {})

    def test_combinational_cycles_fail_closed(self):
        logic = {"a": {"kind": "combinational", "data": {"source": "a"}}}
        with self.assertRaises(ValueError):
            evaluate_data({"source": "a"}, logic, {})
        with self.assertRaises(ValueError):
            data_support({"source": "a"}, logic)

    def test_compact_report_preserves_active_and_unknown_evidence(self):
        report = {"nodes": [{"pips": [{"status": status, "evidence": status}
                                      for status in ("active", "disabled", "unknown")]}]}
        node = compact_report(report)["nodes"][0]
        self.assertEqual(node["disabled_pip_count"], 1)
        self.assertEqual([p["evidence"] for p in node["pips"]], ["active", "unknown"])

    def test_disabled_pad_alias_is_not_an_active_driver(self):
        class PadArchitecture(FakeArchitecture):
            def query(self, x, y, wire=None, bel=None):
                if bel:
                    return {"attributes": {"IBUF_MODE": {"bits": [{"frame": 0, "bit": 0}],
                            "values": {"NONE": [True], "CMOS": [False]}}}, "inputs": {}}
                return super().query(x, y, wire)
        branches = [{"source": [0, 0, name], "config": [
                    {"kind": "permanent", "bits": [], "expected": []}]} for name in ("pad", "dll")]
        arch = PadArchitecture({"q": branches},
                               {"pad": [{"x": 0, "y": 0, "bel": "IOI[2]", "pin": "I"}],
                                "dll": [{"x": 0, "y": 0, "bel": "DLL", "pin": "LOCKED"}]})
        result = Router(self.image, arch).trace([0, 0, "q"])
        self.assertEqual(result["driver_status"], "resolved")
        self.assertEqual(result["terminals"][0]["bel"], "DLL")
        self.assertEqual(result["inactive_sources"], [[0, 0, "pad"]])
        self.assertEqual(Router(self.image, arch).trace([0, 0, "pad"])["driver_status"], "unknown")
        self.image.frames[0][0] = 0
        self.assertEqual(Router(self.image, arch).trace([0, 0, "q"])["driver_status"], "ambiguous")

    def test_checkout_must_be_clean_and_pinned(self):
        source = Path.cwd().resolve()
        for head, status in ((COMMIT, " M public/tablegen/src/emit.rs"), ("wrong", "")):
            with patch("virtexe_routing.subprocess.check_output", side_effect=[str(source), head, status]):
                with self.assertRaises(ValueError):
                    verify_checkout(source)
        with patch("virtexe_routing.subprocess.check_output", side_effect=[str(source), COMMIT, ""]):
            self.assertEqual(verify_checkout(source), source)

    def test_checkout_must_be_repository_root(self):
        source = Path.cwd().resolve()
        with patch("virtexe_routing.subprocess.check_output", return_value=str(source.parent)):
            with self.assertRaises(ValueError):
                verify_checkout(source)

    def test_carry_xor_modes_exhaustively(self):
        for cy0, sf, sg, init, yb in itertools.product(
                ("CONST_0", "CONST_1", "F1_G1", "PROD"), ("CONST_1", "F"),
                ("CONST_1", "G"), ("BX", "CIN"), ("GCY", "BY")):
            analyzer = LogicAnalyzer(CarryRouter(cy0, sf, sg, init, yb))
            roots = {pin: analyzer.output({"x": 0, "y": 2, "bel": "SLICE[0]", "pin": pin})
                     for pin in ("X", "Y", "XB", "YB", "COUT")}
            logic = analyzer.report(roots)["logic"]
            for f1, f2, g1, g2, bx, by, lower in itertools.product(range(2), repeat=7):
                ci = bx if init == "BX" else lower
                di_f = {"CONST_0": 0, "CONST_1": 1, "F1_G1": f1, "PROD": f1 & f2}[cy0]
                di_g = {"CONST_0": 0, "CONST_1": 1, "F1_G1": g1, "PROD": g1 & g2}[cy0]
                fc = ci if sf == "CONST_1" or f1 else di_f
                gc = fc if sg == "CONST_1" or g1 else di_g
                expected = {"X": f1 ^ ci, "Y": g1 ^ fc, "XB": fc,
                            "YB": gc if yb == "GCY" else by, "COUT": gc}
                values = {f"X0,Y2 INPUT.{pin}": value for pin, value in
                          zip(("F1", "F2", "G1", "G2", "BX", "BY"), (f1, f2, g1, g2, bx, by))}
                values["X0,Y1 INPUT.BX"] = lower
                for pin, result in expected.items():
                    self.assertEqual(evaluate_data({"source": roots[pin]}, logic, values), bool(result))

    def test_missing_cin_is_not_a_guessed_neighbor(self):
        router = CarryRouter(init="CIN")
        original = router.bel
        router.bel = lambda *args: dict(original(*args), dedicated={})
        analyzer = LogicAnalyzer(router)
        root = analyzer.output({"x": 0, "y": 2, "bel": "SLICE[0]", "pin": "CI"})
        report = analyzer.report([root])
        self.assertIn("unknown", report["logic"][root]["data"])

    def test_bus_rotation_joiner_owner_and_bram_skip(self):
        row = synthetic_bus_row()
        component = bus_component(row, (2, 2))
        self.assertEqual(component["members"], [(0, 1), (2, 2), (3, 3)])
        row["columns"][1]["bus"]["attributes"]["JOINER_E"]["value"] = 1
        component = bus_component(row, (2, 2))
        self.assertEqual(component["members"], [(0, 1), (2, 2), (3, 3), (3, 4), (5, 0), (6, 1)])
        self.assertNotIn((2, 3), component["members"])
        self.assertNotIn((4, 0), component["members"])
        self.assertFalse(any(g["a"][0] == 6 for g in component["joiners"]))

    def test_bus_taps_are_independent_and_same_driver_is_deduplicated(self):
        row = synthetic_bus_row()
        row["columns"][1]["tbufs"][0]["attributes"]["OUT_B"]["value"] = 1
        component = bus_component(row, (2, 2))
        self.assertEqual(len(component["drivers"]), 1)
        self.assertEqual(component["drivers"][0]["terminal"]["bel"], "TBUF[0]")
        self.assertEqual(component["drivers"][0]["taps"][0]["lane"], 2)
        # Synthetic fixed alias joins both selected taps without duplicating O.
        row["fixed"].append([[2, 0], [2, 2]])
        row["columns"][1]["tbufs"][0]["attributes"]["OUT_A"]["value"] = 1
        component = bus_component(row, (2, 2))
        self.assertEqual(len(component["drivers"]), 1)
        self.assertEqual(len(component["drivers"][0]["taps"]), 2)

    def test_unknown_joiner_and_tap_encodings_remain_unknown(self):
        row = synthetic_bus_row()
        row["columns"][1]["bus"]["attributes"]["JOINER_E"] = {"status": "unknown"}
        self.assertIn("unknown", bus_component(row, (2, 2)))
        row["columns"][1]["bus"]["attributes"]["JOINER_E"] = {"value": 0}
        row["columns"][1]["tbufs"][0]["attributes"]["OUT_B"] = {"status": "unknown"}
        self.assertIn("unknown", bus_component(row, (2, 2)))

    def test_bus_resolution_floating_contention_and_multiple_drivers(self):
        bus = {"op": "bus", "drivers": [{"driver": "a"}, {"driver": "b"}]}
        logic = {n: {"kind": "tristate_driver", "data": {"source": n + "i"},
                     "disable": {"source": n + "t"}} for n in ("a", "b")}
        for ai, bi, at, bt in itertools.product(range(2), repeat=4):
            values = {"ai": ai, "bi": bi, "at": at, "bt": bt}
            status = "floating" if at and bt else "driven" if at != bt else "contention" if ai != bi else "multiple_drivers_agree"
            result = resolve_bus(bus, logic, values)
            self.assertEqual(result["status"], status)
            if status in ("floating", "contention"):
                with self.assertRaises(ValueError):
                    evaluate_data(bus, logic, values)
            else:
                self.assertEqual(evaluate_data(bus, logic, values), bool(bi if at else ai))
        self.assertEqual(resolve_bus(bus, logic, {"at": 1})["status"], "unknown")
        logic["a"]["disable"] = {"constant": 1}
        logic["b"]["disable"] = {"constant": 1}
        self.assertEqual(resolve_bus(bus, logic, {})["status"], "floating")

    def test_register_reset_clock_and_ce_priority(self):
        signal = "X1,Y1 SLICE[0].XQ"
        node = {"kind": "register", "data": {"source": "d"},
                "controls": {p: {"source": p.lower()} for p in ("CE", "SR", "CLK")},
                "mode": {k: {"value": v} for k, v in {"FFX_INIT": 1, "FF_LATCH": 0,
                         "FF_SR_ENABLE": 1, "FF_SR_SYNC": 0, "FF_REV_ENABLE": 0}.items()}}
        report = {"logic": {signal: node}}
        for q, d, ce, sr, edge in itertools.product(range(2), repeat=5):
            result = register_step(report, signal, {signal: q, "d": d, "ce": ce, "sr": sr}, bool(edge))
            self.assertEqual(result["next"], 1 if sr else d if edge and ce else q)
        node["mode"]["FF_SR_SYNC"]["value"] = 1
        self.assertEqual(register_contract(signal, node)["status"], "modeled_sync_ff")
        for q, d, ce, sr, edge in itertools.product(range(2), repeat=5):
            result = register_step(report, signal, {signal: q, "d": d, "ce": ce, "sr": sr}, bool(edge))
            self.assertEqual(result["next"], (1 if sr else d if ce else q) if edge else q)
        node["mode"]["FF_SR_SYNC"]["value"] = 0
        node["mode"]["FF_LATCH"]["value"] = 1
        self.assertEqual(register_contract(signal, node)["status"], "unknown")

    def test_ce_hold_does_not_sample_a_floating_bus(self):
        signal = "X1,Y1 SLICE[0].YQ"
        node = {"kind": "register", "data": {"op": "bus", "drivers": []},
                "controls": {"CE": {"source": "ce"}, "SR": {"constant": 0}},
                "mode": {k: {"value": v} for k, v in {"FFY_INIT": 0, "FF_LATCH": 0,
                         "FF_SR_ENABLE": 1, "FF_SR_SYNC": 0, "FF_REV_ENABLE": 0}.items()}}
        report = {"logic": {signal: node}}
        self.assertEqual(register_step(report, signal, {signal: 1, "ce": 0}, True)["next"], 1)
        self.assertEqual(register_step(report, signal, {signal: 1, "ce": 1}, True)["status"], "unknown")

    def test_nested_operator_unknown_and_cycle_are_explicit(self):
        signal = "q"
        report = {"logic": {signal: {"kind": "combinational", "data": {
            "op": "xor", "args": [{"source": "q"}, {"unknown": "missing carry"}]}}}}
        summary = cone_summary(report, [signal])
        self.assertEqual(summary["status"], "incomplete")
        self.assertEqual(summary["nodes"], 1)
        self.assertEqual(summary["boundaries"][signal], ["missing carry"])
        self.assertEqual(state_table(report, signal, local=True)["status"], "unknown")

    def test_local_table_visits_operator_lists(self):
        data = {"op": "xor", "args": [{"source": "a"}, {"source": "b"}]}
        table = state_table({"logic": {"q": {"data": data}}}, "q", local=True)
        self.assertEqual(table["inputs"], ["a", "b"])
        self.assertEqual([r["d"] for r in table["rows"]], [0, 1, 1, 0])

    def test_bus_combinational_cycle_is_unknown_but_register_feedback_is_valid(self):
        bus = {"op": "bus", "drivers": [{"driver": "tbuf"}]}
        logic = {"bus": {"kind": "combinational", "data": bus}, "tbuf": {
            "kind": "tristate_driver", "disable": {"constant": 0}, "data": {"source": "bus"}}}
        self.assertEqual(resolve_bus(bus, logic, {})["status"], "unknown")
        with self.assertRaisesRegex(ValueError, "cycle"):
            data_support(bus, logic)
        logic["tbuf"]["data"] = {"source": "q"}
        logic["q"] = {"kind": "register", "data": bus}
        self.assertEqual(data_support(bus, logic), {"q"})
        self.assertEqual(resolve_bus(bus, logic, {"q": 1})["value"], 1)
        logic["tbuf"]["disable"] = {"constant": 1}
        logic["tbuf"]["data"] = {"unknown": "disabled producer"}
        self.assertEqual(data_support(bus, logic), set())

    def test_dual_port_ram_configuration_is_not_a_constant_lut(self):
        router = CarryRouter()
        original = router.bel
        def bel(x, y, name):
            b = original(x, y, name)
            if name == "INPUT":
                return b
            b["attributes"].update({k: {"value": v} for k, v in {
                "F_RAM_ENABLE": 1, "G_RAM_ENABLE": 1, "WA4_ENABLE": 0, "F": 0, "G": 0}.items()})
            b["attributes"].update({k: {"selections": [v]} for k, v in {
                "DIF_MUX": "BY", "FXMUX": "F", "GYMUX": "G"}.items()})
            b["inputs"].update({p: {"wire": [x, y, p]} for p in (
                "F3", "F4", "G3", "G4", "SR", "CLK")})
            return b
        router.bel = bel
        analyzer = LogicAnalyzer(router)
        roots = {p: analyzer.output({"x": 0, "y": 2, "bel": "SLICE[0]", "pin": p}) for p in ("X", "Y")}
        report = analyzer.report(roots)
        memory = next(iter(report["memories"]))
        self.assertEqual(len(report["memories"]), 1)
        self.assertEqual(report["memories"][memory]["initial"], 0)
        self.assertEqual(report["logic"][roots["X"]]["data"]["op"], "ram_read")
        with self.assertRaisesRegex(ValueError, "runtime memory"):
            evaluate_data({"source": roots["X"]}, report["logic"], {})
        for x, y in itertools.product(range(16), repeat=2):
            values = {memory: 0xA5A5}
            for letter, address in (("F", x), ("G", y)):
                values.update({f"X0,Y2 INPUT.{letter}{i + 1}": (address >> i) & 1 for i in range(4)})
            for pin, address in (("X", x), ("Y", y)):
                self.assertEqual(evaluate_data({"source": roots[pin]}, report["logic"], values), bool(0xA5A5 & (1 << address)))
        summary = cone_summary(report, list(roots.values()))
        self.assertIn(memory, summary["boundaries"])
        self.assertIn("X0,Y2 INPUT.SR", summary["boundaries"])
        b = bel(0, 2, "SLICE[0]")
        b["attributes"]["G"]["value"] = 1
        entry = {}
        analyzer.slice(entry, "X", b["attributes"], b["inputs"], {"x": 0, "y": 2, "bel": "SLICE[0]"}, {})
        self.assertIn("disagree", entry["data"]["unknown"])

    def test_memory_write_requires_runtime_state_and_selected_clock(self):
        contract = {"kind": "ram16x1d", "initial": 0, "enable": {"source": "we"},
                    "data": {"source": "d"}, "write_address": {str(i): {"source": f"a{i}"} for i in range(4)}}
        report = {"logic": {}, "memories": {"m": contract}}
        self.assertEqual(memory_step(report, "m", {}, True)["status"], "unknown")
        for initial, address, data, we, edge in itertools.product((0, 0xA5A5, 0xFFFF), range(16), range(2), range(2), range(2)):
            values = {"m": initial, "d": data, "we": we, **{f"a{i}": (address >> i) & 1 for i in range(4)}}
            expected = ((initial & ~(1 << address)) | (data << address)) if we and edge else initial
            self.assertEqual(memory_step(report, "m", values, bool(edge))["next"], expected)

    def test_missing_pci_state_network_cannot_pass_native_equation_checks(self):
        result = validate_pci_relations({"logic": {}})
        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(len(result["checks"]), 19)
        self.assertTrue(all(not c["pass"] and c["reason"] for c in result["checks"]))

    def test_local_alias_proof_checks_polarity_ordering_and_unknowns(self):
        left = {"op": "lut4", "init": 0xAAAA, "inputs": {"0": {"source": "a"}}}
        right = {"op": "lut4", "init": 0xCCCC, "inputs": {"1": {"source": "a"}}}
        self.assertEqual(local_equivalence(left, right, {})["rows"], 2)
        self.assertEqual(local_equivalence({"source": "alias"}, right,
                         {"alias": {"kind": "combinational", "data": left}})["rows"], 2)
        for invalid in (dict(right, inv=True), {"unknown": "absent encoding"},
                        {"op": "bus", "drivers": []}):
            with self.assertRaises(ValueError):
                local_equivalence(left, invalid, {})
        with self.assertRaisesRegex(ValueError, "cycle"):
            local_equivalence({"source": "a"}, right,
                              {"a": {"kind": "combinational", "data": {"source": "a"}}})

    def test_shadow_proof_requires_matching_register_controls_and_init(self):
        left, right = "X1,Y1 SLICE[0].XQ", "X0,Y1 IOI[1].PAD_O"
        controls = {p: {"source": p.lower()} for p in ("CE", "SR", "CLK")}
        flags = {"LATCH": 0, "SR_ENABLE": 1, "SR_SYNC": 0}
        a = {"kind": "register", "data": {"source": "d"}, "controls": controls,
             "mode": {"FFX_INIT": {"value": 1}, "FF_REV_ENABLE": {"value": 0},
                      **{"FF_" + k: {"value": v} for k, v in flags.items()}}}
        b = {"kind": "register", "data": {"source": "d"},
             "controls": {"OCE": controls["CE"], "SR": controls["SR"], "OCLK": controls["CLK"]},
             "mode": {"FFO_INIT": {"value": 1}, **{"FFO_" + k: {"value": v} for k, v in flags.items()}}}
        report = {"logic": {left: a, right: b}}
        self.assertEqual(set(register_equivalence(report, left, right)), {"data", "ce", "sr", "clock"})
        b["controls"]["OCE"] = {"source": "other_ce"}
        with self.assertRaises(ValueError):
            register_equivalence(report, left, right)
        b["controls"]["OCE"] = controls["CE"]
        b["mode"]["FFO_INIT"]["value"] = 0
        with self.assertRaises(ValueError):
            register_equivalence(report, left, right)

    def test_summary_does_not_turn_local_pass_into_complete_reconstruction(self):
        report = {"logic": {"b": {"kind": "combinational", "data": {"op": "bus"}}},
                  "routes": {"x": {"driver_status": "ambiguous"}}, "boundaries": [],
                  "local_relation_checks": {"status": "PASS"}}
        summary = reconstruction_summary(report)
        self.assertEqual(summary["status"], "incomplete")
        self.assertEqual(summary["routing_statuses"]["ambiguous"], 1)
        self.assertEqual(summary["conditional_buses"], 1)
        self.assertTrue(summary["dma_quiescence"].startswith("Unknown"))


if __name__ == "__main__":
    unittest.main()
