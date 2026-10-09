"""Synthetic tests only. No firmware bytes or firmware-derived fixtures."""
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "fpga"))
from virtexe_routing import Router, LogicAnalyzer, classify_pip, decode_bel, lut_support


def pip(source, kind="pass", expected=True, inv=False):
    return {"source": [0, 0, source], "inv": False, "config": [{"kind": kind,
            "bits": [{"frame": 0, "bit": 0, "inv": inv}], "expected": [expected]}]}


class FakeArchitecture:
    def __init__(self, graph, terminals=None):
        self.graph, self.terminals = graph, terminals or {}

    def query(self, x, y, wire):
        return {"root": [x, y, wire], "tree": [[x, y, wire]],
                "pips": self.graph.get(wire, []), "terminals": self.terminals.get(wire, [])}


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


if __name__ == "__main__":
    unittest.main()
