"""Synthetic and private PCI initiator/descriptor evidence regressions."""
import hashlib
import itertools
import json
import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "fpga"))
from virtexe_dma import (IIMTC_GATES, NAMES, RELATIONS, SGTA_GATES, ad_origin,
                        capture_map, check_local, focused_evidence, phase_observation, sl,
                        master_address_map, pointer_map, pointer_next, same_sampling_contract,
                        ad_bus_drivers, payload_map, POINTER_REPLICAS, LOAD_REPLICAS)
from virtexe_mmio import functional_expression
from virtexe_routing import COMMIT, evaluate_data, register_contract, register_equivalence
from virtexe_dma_handshake import (H, HANDSHAKE_RELATIONS, SCENARIOS, local_checks,
                                  owned_phase, pending_next, projection,
                                  handshake_queries, word_observer, receive_availability_checks)


class SyntheticHandshake(unittest.TestCase):
    def phase(self, **kwargs):
        values = dict(owned=True, data_phase=True, output_enabled=True, frame=False,
                      irdy=False, trdy=False, devsel=False, stop=True,
                      transferred_before=False, claimed_before=True)
        values.update(kwargs)
        return owned_phase(**values)

    def test_external_trdy_without_owned_enabled_data_phase_is_not_acceptance(self):
        for key in ('owned', 'data_phase', 'output_enabled'):
            self.assertFalse(self.phase(**{key:False})['accepted'])

    def test_wait_final_and_disconnect_with_data(self):
        self.assertTrue(self.phase(frame=True)['final_accepted'])
        self.assertFalse(self.phase(frame=True,trdy=True)['final_accepted'])
        self.assertTrue(self.phase(stop=False)['disconnect_with_data'])
        self.assertTrue(self.phase(irdy=True)['wait'])

    def test_retry_disconnect_and_abort_require_distinct_history(self):
        self.assertTrue(self.phase(stop=False,trdy=True)['retry'])
        self.assertFalse(self.phase(stop=False,trdy=True,transferred_before=True)['retry'])
        self.assertTrue(self.phase(stop=False,trdy=True,transferred_before=True)['disconnect_without_data'])
        self.assertTrue(self.phase(stop=False,trdy=True,devsel=True)['target_abort'])
        self.assertFalse(self.phase(stop=False,trdy=True,devsel=True,claimed_before=False)['target_abort'])
        self.assertFalse(self.phase(devsel=True,trdy=True,claimed_before=False)['master_abort'])
        self.assertTrue(self.phase(devsel=True,trdy=True,claimed_before=False,response_window_expired=True)['master_abort'])

    def test_pending_counter_simultaneous_events_are_not_naive_subtraction(self):
        self.assertEqual(pending_next(2,True,True,True,True),2)
        self.assertEqual(pending_next(2,True,False,True,True),1)
        self.assertEqual(pending_next(0,True,False,True,False),3)
        self.assertEqual(pending_next(1,False,False,False,True),1)

    def test_rollback_drains_at_most_three_words_under_stated_mode(self):
        for start in range(4):
            q=start
            for _ in range(3):
                q=pending_next(q,True,False,False,q!=0)
            self.assertEqual(q,0)

    def test_word_observer_reports_unmatched_and_duplicate_events_without_constraints(self):
        result=word_observer([{'write':(0,'a')},{'issue':0},{'rewind':True},
                              {'issue':0},{'accept':True},{'issue':0},{'accept':True}])
        self.assertEqual(result['accepted'],['a','a'])
        self.assertIn('duplicate acceptance',result['violations'])
        self.assertIn('rewind without issued token',word_observer([{'rewind':True}])['violations'])
        self.assertIn('overwrite of pending token',word_observer([
            {'write':(0,'a')},{'issue':0},{'write':(16,'b')}])['violations'])

    def test_projection_preserves_actual_contract_and_names_external_cuts(self):
        p={'logic':{H['q0']:{'kind':'register','data':{'source':'external'}},
                    'external':{'kind':'register','data':{'constant':1}},
                    'comb':{'kind':'combinational','data':{'source':'external'}}},'memories':{}}
        result=projection(p,['q0'])
        self.assertIs(result['logic'][H['q0']],p['logic'][H['q0']])
        self.assertNotIn('external',result['logic'])

    def test_public_handshake_matrix_keeps_unknown_safety_and_all_scenarios(self):
        p=Path(__file__).resolve().parents[2]/'docs'/'pci-mmio-register-evidence.json'
        m=json.loads(p.read_text(encoding='utf-8'))['pci_dma']['handshake_retirement']
        self.assertEqual(m['local_relations'],len(HANDSHAKE_RELATIONS))
        self.assertEqual(len(m['scenarios']),len(SCENARIOS))
        self.assertEqual(m['owned_acceptance'],'Unknown')
        self.assertEqual(m['irq_after_every_accepted_word'],'Unknown')


class SyntheticDma(unittest.TestCase):
    def test_sampling_replicas_reject_unknown_modes_and_different_initialization(self):
        mode={k:{'value':v} for k,v in dict(FF_LATCH=0,FF_SR_ENABLE=1,FF_SR_SYNC=0,FF_REV_ENABLE=0,FFY_INIT=0).items()}
        def node(initial):
            return {'kind':'register','data':{'source':'conditional_bus'},
                    'controls':{'CE':{'constant':1},'CLK':{'source':'clock'},'SR':{'source':'reset'}},
                    'mode':dict(mode,FFY_INIT={'value':initial})}
        p={'logic':{'left.YQ':node(0),'right.YQ':node(0)}}
        same_sampling_contract(p,'left.YQ','right.YQ')
        p['logic']['right.YQ']=node(1)
        with self.assertRaises(ValueError):
            same_sampling_contract(p,'left.YQ','right.YQ')
        p['logic']['right.YQ']['mode']={}
        with self.assertRaises(ValueError):
            same_sampling_contract(p,'left.YQ','right.YQ')

    def test_payload_mapping_rejects_missing_producer(self):
        p={'roots':{'AD[0]':{'O':'out'}},'logic':{'out':{'kind':'register','data':{'constant':0}}}}
        with self.assertRaises(ValueError):
            payload_map(p)

    def test_pointer_load_step_priority_alignment_and_wrap(self):
        self.assertEqual(pointer_next(0xFFFFFFFC, 0, 0, False, False, True), 4)
        self.assertEqual(pointer_next(12, 0x103, 0x207, True, True, False), 0x100)
        self.assertEqual(pointer_next(12, 0x103, 0x207, False, True, False), 0x204)
        self.assertEqual(pointer_next(12, 0x103, 0x207, True, True, True), 20)
        self.assertEqual(pointer_next(12, 0, 0, False, False, False), 12)

    def test_public_matrix_preserves_safety_and_query_scope(self):
        path = Path(__file__).resolve().parents[2] / "docs" / "pci-mmio-register-evidence.json"
        matrix = json.loads(path.read_text(encoding="utf-8"))
        dma = matrix["pci_dma"]
        self.assertEqual(set(matrix["release_obligations"].values()), {"Unknown"})
        self.assertEqual(matrix["stable_until_explicit_restart"], "Unknown")
        self.assertEqual(dma["local_equations"]["count"], len(RELATIONS))
        self.assertEqual(dma["completion_source"]["retirement_implication"], "Unknown")
        q = dma["bounded_queries"]
        self.assertEqual(q["status"], "Inconclusive")
        self.assertEqual(q["bound"], 1)
        self.assertEqual([r["number"] for r in q["results"]], list(range(1, 11)))
        self.assertEqual(sum(r["solver"] == "sat" for r in q["results"]), q["sat"])
        self.assertEqual(sum(r["solver"] == "unsat" for r in q["results"]), q["unsat"])
        self.assertEqual(q["results"][4]["solver"], "unsat")
        self.assertIn("not eventual retry retirement", q["results"][4]["query"])

    def test_final_frame_does_not_mean_final_data_accepted(self):
        for trdy in (False, True):
            r = phase_observation(True, False, trdy, True, False, False, True)
            self.assertEqual(r["final_accepted"], not trdy)
            self.assertFalse(r["bus_idle_observed"])

    def test_retry_and_disconnect_are_distinguished_by_history(self):
        for first, trdy in itertools.product((False, True), repeat=2):
            r = phase_observation(False, False, trdy, False, False, first, True)
            self.assertEqual(r["retry"], first and trdy)
            self.assertEqual(r["disconnect"], not first or not trdy)
            self.assertFalse(r["target_abort"])

    def test_abort_requires_claim_history_and_is_not_retry(self):
        for claimed in (False, True):
            r = phase_observation(False, False, True, False, True, True, claimed)
            self.assertEqual(r["target_abort"], claimed)
            self.assertFalse(r["retry"])

    def test_capture_origin_does_not_assume_one_hot_bus(self):
        p = {"roots": {"AD[3]": {"IQ": "iq"}}, "logic": {
            "bus": {"kind": "combinational", "data": {"op": "bus", "drivers": [{"driver": "d"}, {"driver": "other"}]}},
            "d": {"kind": "tristate_driver", "data": {"source": "iq"}, "disable": {"source": "gate"}},
            "other": {"kind": "tristate_driver", "data": {"unknown": "other producer"}, "disable": {"constant": 0}},
            "iq": {"kind": "register"}, "gate": {"kind": "register"}}}
        r = ad_origin(p, {"source": "bus"})
        self.assertEqual(r["bit"], 3)
        self.assertEqual(r["disable"], {"source": "gate"})

    def test_missing_or_ambiguous_origin_fails_closed(self):
        for drivers in ([], [{"driver": "a"}, {"driver": "b"}]):
            p = {"roots": {"AD[0]": {"IQ": "iq"}}, "logic": {
                "bus": {"kind": "combinational", "data": {"op": "bus", "drivers": drivers}},
                "a": {"kind": "tristate_driver", "data": {"source": "iq"}, "disable": {"constant": 0}},
                "b": {"kind": "tristate_driver", "data": {"source": "iq"}, "disable": {"constant": 0}},
                "iq": {"kind": "register"}}}
            with self.assertRaises(ValueError):
                ad_origin(p, {"source": "bus"})

    def test_wrong_architecture_pin_cannot_pass(self):
        with self.assertRaises(ValueError):
            focused_evidence({"project_combine_commit": "wrong", "logic": {}})

    def test_irq_lut_matches_independent_sticky_clear_equation(self):
        variables = ("data0", "done", "clear", "irq")
        p = {"logic": {NAMES["irq"]: {"data": {"op": "lut4", "init": 0xDFCC,
            "inputs": {str(i): {"source": NAMES[n]} for i, n in enumerate(variables)}}}}}
        p["logic"].update({NAMES[n]: {"kind": "register"} for n in variables if n != "irq"})
        p["logic"][NAMES["irq"]]["kind"] = "register"
        self.assertEqual(check_local(p, "irq", variables, RELATIONS[0][2])["rows"], 16)
        p["logic"][NAMES["irq"]]["data"]["init"] ^= 1
        with self.assertRaises(ValueError):
            check_local(p, "irq", variables, RELATIONS[0][2])


@unittest.skipUnless(os.environ.get("WR6K_MMIO_REPORT"), "private PCI report not configured")
class PrivateDma(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p = json.loads(Path(os.environ["WR6K_MMIO_REPORT"]).read_text(encoding="utf-8"))
        if cls.p.get("project_combine_commit") != COMMIT:
            raise ValueError("architecture pin mismatch")
        if cls.p.get("image_sha256") != hashlib.sha256(Path(os.environ["WR6K_FPGA_BINARY_205"]).read_bytes()).hexdigest():
            raise ValueError("private image/report mismatch")
        if cls.p.get("frame_validation", {}).get("status") != "PASS":
            raise ValueError("private frame validation missing")

    def test_sgta_has_exact_bits_2_through_31(self):
        m = capture_map(self.p, SGTA_GATES)
        self.assertEqual(set(m), set(range(2, 32)))
        self.assertEqual(m[2]["signal"], sl(25, 20, 0, "YQ"))

    def test_complete_pointer_map_has_unique_30_bit_dependencies(self):
        m = pointer_map(self.p)
        self.assertEqual(set(m), set(range(2, 32)))
        self.assertEqual(len(set(m.values())), 30)
        self.assertEqual(m[2], sl(25, 21, 0, "YQ"))
        self.assertEqual(m[31], sl(25, 27, 0, "XQ"))

    def test_master_address_has_30_registers_and_two_zero_bits(self):
        m = master_address_map(self.p)
        for bit in (0, 1):
            self.assertEqual(functional_expression(self.p['logic'][m[bit]['signal']]['data']), {'constant': 0})
        self.assertEqual(len({m[b]['signal'] for b in range(2, 32)}), 30)
        self.assertTrue(all(self.p['logic'][m[b]['signal']]['kind'] == 'register' for b in range(2, 32)))

    def test_iimtc_has_all_32_bits(self):
        self.assertEqual(set(capture_map(self.p, IIMTC_GATES)), set(range(32)))

    def test_two_distinct_word_banks_have_complete_bit_mapping(self):
        a = capture_map(self.p, [NAMES["word_a_lo_ce"], NAMES["word_a_hi_ce"]], stages=1)
        b = capture_map(self.p, [NAMES["word_b_lo_ce"], NAMES["word_b_hi_ce"]], stages=1)
        self.assertEqual(set(a), set(range(32)))
        self.assertEqual(set(b), set(range(32)))
        self.assertFalse({d["signal"] for d in a.values()} & {d["signal"] for d in b.values()})
        for m in (a, b):
            self.assertTrue(all(d["disable"]["source"] == sl(3, 14, 0, "X") for d in m.values()))

    def test_busmaster_command_enable_is_configuration_bit_two(self):
        name = sl(5, 10, 1, "YQ")
        n = self.p["logic"][name]
        self.assertEqual(ad_origin(self.p, n["data"])["bit"], 2)
        self.assertEqual(n["controls"]["CE"]["source"], sl(8, 10, 0, "Y"))

    def test_all_ad_and_cbe_output_registers_retain_pci_ce_boundary(self):
        for name, root in self.p["roots"].items():
            if not name.startswith(("AD[", "CBE#[")):
                continue
            c = register_contract(root["O"], self.p["logic"][root["O"]])
            self.assertEqual(c["ce"]["source"], "X0,Y13 PCILOGIC.PCI_CE")

    def test_receive_buffer_has_independent_remote_and_pci_ports(self):
        for name in ("X46,Y9 BRAM", "X46,Y13 BRAM"):
            ports = self.p["memories"][name]["ports"]
            self.assertEqual(ports["A"]["clock"]["source"], "X24,Y0 BUFGCE[0].O")
            self.assertEqual(ports["B"]["clock"]["source"], "X24,Y29 BUFGCE[1].O")
            self.assertEqual(ports["A"]["write"]["source"], sl(43, 10, 1, "YQ"))
            self.assertEqual(functional_expression(ports["B"]["write"]), {"constant": 1, "inv": True})

    def test_descriptor_load_flag_replicas_have_matched_register_contracts(self):
        for n in LOAD_REPLICAS:
            register_equivalence(self.p, sl(22, 19, 1, "YQ"), n)

    def test_pointer_control_replicas_have_exact_shared_sampling_contracts(self):
        for left,right in POINTER_REPLICAS:
            same_sampling_contract(self.p,left,right)

    def test_all_payload_bits_map_through_distinct_staging_ram_to_receive_bram(self):
        m=payload_map(self.p)
        for bit,d in m.items():
            expected=f'X46,Y{13 if bit<16 else 9} BRAM.DOB[{bit%16}]'
            self.assertEqual(d['source'],{'source':expected,'inv':False})
            self.assertEqual(set(d['write_address']),{'0','1','2','3'})
            self.assertEqual(set(d['read_address']),{'0','1','2','3'})
            self.assertEqual(self.p['memories'][d['memory']]['clock']['source'],'X24,Y29 BUFGCE[1].O')


def add_relation(name, variables, predicate):
    def test(self):
        self.assertEqual(check_local(self.p, name, variables, predicate)["status"], "Verified")
    setattr(PrivateDma, "test_equation_" + name, test)


for relation in RELATIONS:
    add_relation(*relation)


@unittest.skipUnless(os.environ.get('WR6K_MMIO_REPORT'), 'private PCI report not configured')
class PrivateHandshake(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        PrivateDma.setUpClass()
        cls.p=PrivateDma.p
        cls.checks=local_checks(cls.p)

    def test_receive_availability_is_eight_paired_pointer_inequalities(self):
        r=receive_availability_checks(self.p)
        self.assertEqual(len(r['pairs']),8)
        self.assertEqual(r['status'],'Verified')
        self.assertEqual(len({n for pair in r['pairs'] for n in pair}),16)

    def test_prefetch_and_available_registers_reset_to_zero_on_pci_clock(self):
        for name in ('q0','q1','rollback',*[f'available{i}' for i in range(4)]):
            c=register_contract(H[name],self.p['logic'][H[name]])
            self.assertEqual(c['init'],0)
            self.assertEqual(c['clock']['source'],'X24,Y29 BUFGCE[1].O')
            self.assertEqual(functional_expression(c['ce']),{'constant':1,'inv':False})
            self.assertEqual(c['sr']['source'],self.p['roots']['RST#']['I'])


def add_handshake_relation(name):
    def test(self):
        self.assertEqual(self.checks[name]['status'],'Verified')
    setattr(PrivateHandshake,'test_handshake_equation_'+name,test)


for name,_,_ in HANDSHAKE_RELATIONS:
    add_handshake_relation(name)


@unittest.skipUnless(os.environ.get('WR6K_MMIO_REPORT') and os.environ.get('WR6K_FPGA_SYMBOLIC')=='1',
                     'private PCI SMT not configured')
class PrivateHandshakeSmt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        PrivateDma.setUpClass()
        cls.p=PrivateDma.p
        cls.results=handshake_queries(cls.p)

    def test_prefetch_two_bit_next_equation_with_simultaneous_events(self):
        from virtexe_symbolic import System,z3
        s=System(self.p,bound=0,powerup=False)
        q=lambda n:s.signal(H[n],0)
        word=lambda lo,hi:z3.If(lo,z3.BitVecVal(1,2),z3.BitVecVal(0,2))+z3.If(hi,z3.BitVecVal(2,2),z3.BitVecVal(0,2))
        old=word(q('q0'),q('q1'))
        actual=word(*[s.expr(self.p['logic'][H[n]]['data'],0) for n in ('q0','q1')])
        direction=z3.And(q('mode'),z3.Not(q('issue')),z3.Or(q('pci_accept'),q('rewind')))
        toggle=z3.Or(direction,z3.And(q('issue'),z3.Not(q('pci_accept')),z3.Not(q('rewind'))))
        expected=old+z3.If(toggle,z3.If(direction,z3.BitVecVal(3,2),z3.BitVecVal(1,2)),z3.BitVecVal(0,2))
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(actual!=expected)
        self.assertEqual(s.solver.check(),z3.unsat)

    def test_available_word_count_and_pointer_modulo_conservation(self):
        from virtexe_symbolic import System,z3
        s=System(self.p,bound=0,powerup=False)
        q=lambda n:s.signal(H[n],0)
        regs=[H[f'available{i}'] for i in range(4)]
        word=lambda values:sum((z3.If(v,z3.BitVecVal(1<<i,4),z3.BitVecVal(0,4)) for i,v in enumerate(values)),z3.BitVecVal(0,4))
        old=word([s.signal(n,0) for n in regs])
        actual=word([s.expr(self.p['logic'][n]['data'],0) for n in regs])
        unit=lambda n:z3.If(q(n),z3.BitVecVal(1,4),z3.BitVecVal(0,4))
        expected=old+unit('write')+unit('rewind')-unit('issue')
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(actual!=expected)
        self.assertEqual(s.solver.check(),z3.unsat)

    def test_reload_high_and_low_copies_have_equal_next_functions_and_contracts(self):
        from virtexe_symbolic import System,z3
        s=System(self.p,bound=0,powerup=False)
        a,b=[register_contract(H[n],self.p['logic'][H[n]]) for n in ('reload','reload_hi')]
        for key in ('init','flags','clock','ce','sr','rev','reset','set','status'):
            self.assertEqual(functional_expression(a.get(key)),functional_expression(b.get(key)))
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(s.expr(a['data'],0)!=s.expr(b['data'],0))
        self.assertEqual(s.solver.check(),z3.unsat)

    def test_issue_and_rewind_can_coexist_in_unreachable_or_runtime_cut(self):
        from virtexe_symbolic import System,z3
        s=System(self.p,bound=0,powerup=False)
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(s.signal(H['issue'],0),s.signal(H['rewind'],0))
        self.assertEqual(s.solver.check(),z3.sat)

    def test_mode_zero_can_retain_rollback_state_in_runtime_projection(self):
        from virtexe_symbolic import System,clock_key,z3
        local=projection(self.p,['q0','q1','rollback'])
        pci=clock_key({'source':'X24,Y29 BUFGCE[1].O','inv':False})
        s=System(local,bound=4,powerup=False,schedule=[{pci}]*4)
        q=lambda n,t:s.signal(H.get(n,n),t)
        for t in range(5):
            s.solver.add(q(self.p['roots']['RST#']['I'],t),z3.Not(q('issue',t)),
                         z3.Not(q('mode',t)),z3.Not(q('pci_accept',t)),z3.Not(q('end_edge',t)))
        s.solver.add(q('q0',0),z3.Not(q('q1',0)),q('rollback',0))
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(z3.And(*[q('rewind',t) for t in range(5)]))
        self.assertEqual(s.solver.check(),z3.sat)

    def test_pci_ce_zero_holds_all_36_output_bits_not_automatic_sampling(self):
        from virtexe_symbolic import System,clock_key,z3
        outputs=[r['O'] for n,r in self.p['roots'].items() if n.startswith(('AD[','CBE#['))]
        self.assertEqual(len(outputs),36)
        local=projection(self.p,[],outputs)
        pci=clock_key({'source':'X24,Y29 BUFGCE[1].O','inv':False})
        s=System(local,bound=1,powerup=False,schedule=[{pci}])
        for t in (0,1):
            s.solver.add(s.signal(self.p['roots']['RST#']['I'],t))
        s.solver.add(z3.Not(s.signal('X0,Y13 PCILOGIC.PCI_CE',0)))
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(z3.Or(*[s.signal(n,0)!=s.signal(n,1) for n in outputs]))
        self.assertEqual(s.solver.check(),z3.unsat)


def add_handshake_scenario(number):
    def test(self):
        result=self.results[number-1]
        self.assertEqual(result['base_consistency'],'sat')
        self.assertEqual(result['solver'],'unsat' if number==4 else 'sat')
        self.assertFalse(result['unbounded_proof'])
        self.assertFalse(result['observer_constraints'])
        self.assertGreater(result['unknown_count'],0)
    setattr(PrivateHandshakeSmt,f'test_handshake_scenario_{number:02d}',test)


for number in range(1,len(SCENARIOS)+1):
    add_handshake_scenario(number)


@unittest.skipUnless(os.environ.get("WR6K_MMIO_REPORT") and os.environ.get("WR6K_FPGA_SYMBOLIC") == "1",
                     "private PCI SMT not configured")
class PrivateDmaSmt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        PrivateDma.setUpClass()
        cls.p = PrivateDma.p

    def test_staging_write_pointers_advance_on_admission_and_read_pointer_can_rewind(self):
        from virtexe_symbolic import System,z3
        s=System(self.p,bound=0,powerup=False,timeout_ms=30000)
        q=lambda n:s.signal(n,0)
        def word(values):
            return sum((z3.If(v,z3.BitVecVal(1<<i,4),z3.BitVecVal(0,4)) for i,v in enumerate(values)),z3.BitVecVal(0,4))
        m=payload_map(self.p)
        canonical=[m[0]['write_address'][str(i)]['source'] for i in range(4)]
        for d in m.values():
            for i,n in enumerate(canonical):
                other=d['write_address'][str(i)]['source']
                if n!=other:
                    register_equivalence(self.p,n,other)
                    s.solver.add(q(n)==q(other))
        write=word([q(n) for n in canonical])
        write_d=word([s.expr(self.p['logic'][n]['data'],0) for n in canonical])
        admitted=s.expr(m[0]['write_enable'],0)
        read_names=[m[0]['read_address'][str(i)]['source'] for i in range(4)]
        for d in m.values():
            for i,n in enumerate(read_names):
                other=d['read_address'][str(i)]['source']
                if n!=other:
                    register_equivalence(self.p,n,other)
                    s.solver.add(q(n)==q(other))
        same_sampling_contract(self.p,read_names[0],sl(29,8,0,'YQ'))
        s.solver.add(q(read_names[0])==q(sl(29,8,0,'YQ')))
        read=word([q(n) for n in read_names])
        read_d=word([s.expr(self.p['logic'][n]['data'],0) for n in read_names])
        issue=q(sl(19,8,1,'X')); rewind=q(sl(20,7,0,'X'))
        expected_read=read+z3.If(issue,z3.BitVecVal(1,4),z3.BitVecVal(0,4))-z3.If(rewind,z3.BitVecVal(1,4),z3.BitVecVal(0,4))
        mismatches=[write_d!=write+z3.If(admitted,z3.BitVecVal(1,4),z3.BitVecVal(0,4)),read_d!=expected_read]
        mismatches.extend(s.expr(d['write_enable'],0)!=admitted for d in m.values())
        s.solver.add(q(self.p['roots']['RST#']['I']))
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(z3.Or(*mismatches))
        answer=s.solver.check()
        detail=''
        if answer==z3.sat:
            model=s.solver.model()
            detail=str({k:str(model.eval(v)) for k,v in dict(write=write,write_d=write_d,admitted=admitted,read=read,read_d=read_d,expected_read=expected_read,issue=issue,rewind=rewind).items()})
        self.assertEqual(answer,z3.unsat,detail)

    def test_master_data_phase_all_byte_enables_asserted_under_explicit_selection(self):
        from virtexe_symbolic import System,z3
        s=System(self.p,bound=0,powerup=False,timeout_ms=30000)
        q=lambda n:s.signal(n,0)
        s.solver.add(q(self.p['roots']['RST#']['I']),z3.Not(q(sl(2,21,1,'XQ'))),
                     z3.Not(q(sl(3,14,1,'XQ'))),q(sl(4,9,0,'YQ')),
                     z3.Not(q(sl(6,9,1,'YQ'))))
        bits=[s.expr(self.p['logic'][self.p['roots'][f'CBE#[{i}]']['O']]['data'],0) for i in range(4)]
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(z3.Or(*bits))
        self.assertEqual(s.solver.check(),z3.unsat)

    def test_all_ad_pre_output_data_equal_selected_address_or_payload_alternative(self):
        from virtexe_symbolic import System,z3
        for selection in (master_address_map(self.p),payload_map(self.p)):
            s=System(self.p,bound=0,powerup=False,timeout_ms=30000)
            q=lambda n:s.signal(n,0)
            s.solver.add(q(self.p['roots']['RST#']['I']),z3.Not(q(sl(2,21,1,'XQ'))),
                         z3.Not(q(sl(3,14,1,'XQ'))))
            mismatches=[]
            for bit,d in selection.items():
                for driver,node in ad_bus_drivers(self.p,bit).items():
                    s.solver.add(s.expr(node['disable'],0)==(driver!=d['driver']))
                actual=s.expr(self.p['logic'][self.p['roots'][f'AD[{bit}]']['O']]['data'],0)
                mismatches.append(actual!=q(d['signal']))
            self.assertEqual(s.solver.check(),z3.sat)
            s.solver.add(z3.Or(*mismatches))
            self.assertEqual(s.solver.check(),z3.unsat)

    def test_staging_memory_is_retained_when_write_admission_is_disabled(self):
        from virtexe_symbolic import System,clock_key,z3
        pci={'source':'X24,Y29 BUFGCE[1].O','inv':False}
        s=System(self.p,bound=1,powerup=False,timeout_ms=30000,schedule=[{clock_key(pci)}])
        s.solver.add(*[s.signal(self.p['roots']['RST#']['I'],t) for t in (0,1)])
        memories=payload_map(self.p)
        for d in memories.values():
            s.solver.add(z3.Not(s.expr(d['write_enable'],0)))
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(z3.Or(*[s.memory(d['memory'],0)!=s.memory(d['memory'],1) for d in memories.values()]))
        self.assertEqual(s.solver.check(),z3.unsat)

    def test_runtime_irq_can_rise_without_a_new_accepted_phase(self):
        from virtexe_symbolic import System,clock_key,z3
        pci={'source':'X24,Y29 BUFGCE[1].O','inv':False}
        # Keep actual FF contracts; external named cuts stay explicitly unknown.
        # The complete machine query can time out and gives no reachable trace.
        names=[NAMES[n] for n in ('done','irq','pci_accept')]+[self.p['roots']['RST#']['I']]
        local=dict(self.p,logic={n:self.p['logic'][n] for n in names},memories={})
        s=System(local,bound=2,powerup=False,timeout_ms=30000,schedule=[{clock_key(pci)}]*2)
        q=lambda n,t:s.signal(n,t)
        for t in range(3):
            s.solver.add(q(self.p['roots']['RST#']['I'],t),z3.Not(q(NAMES['pci_accept'],t)))
        s.solver.add(z3.Not(q(NAMES['done'],0)),z3.Not(q(NAMES['irq'],0)),
                     q(NAMES['terminal'],0),q(NAMES['zero'],0))
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(q(NAMES['irq'],2))
        # Retained terminal/descriptor state, not an initialized legal launch.
        # No transaction identity is assigned to this IRQ or to prior phases.
        self.assertEqual(s.solver.check(),z3.sat)

    def test_conditional_two_word_fetch_captures_count_b_then_address_a(self):
        from virtexe_symbolic import System,clock_key,z3
        pci={'source':'X24,Y29 BUFGCE[1].O','inv':False}
        s=System(self.p,bound=3,powerup=False,timeout_ms=30000,schedule=[{clock_key(pci)}]*3)
        q=lambda n,t:s.signal(NAMES[n],t)
        for t in range(4):
            s.solver.add(s.signal(self.p['roots']['RST#']['I'],t))
        s.solver.add(z3.Not(q('phase',0)),z3.Not(q('incoming_phase',0)))
        for t in range(3):
            s.solver.add(q('pending',t)==(t==0),q('accept',t)==(t>0))
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(z3.Or(q('word_a_lo_ce',1),z3.Not(q('word_b_lo_ce',1)),
                           z3.Not(q('word_a_lo_ce',2)),q('word_b_lo_ce',2)))
        self.assertEqual(s.solver.check(),z3.unsat)

    def test_word_a_zero_detector_exact_boolean_equivalence(self):
        from virtexe_symbolic import System, z3
        s = System(self.p, bound=0, powerup=False, timeout_ms=30000)
        m = capture_map(self.p, [NAMES["word_a_lo_ce"], NAMES["word_a_hi_ce"]], stages=1)
        zero = z3.And(*[z3.Not(s.signal(d["signal"], 0)) for d in m.values()])
        s.solver.add(s.signal(self.p["roots"]["RST#"]["I"], 0))
        self.assertEqual(s.solver.check(), z3.sat)
        s.solver.add(s.signal(NAMES["zero"], 0) != zero)
        self.assertEqual(s.solver.check(), z3.unsat)

    def test_complete_pointer_next_load_chain_or_eight_byte_step(self):
        from virtexe_symbolic import System, z3
        s = System(self.p, bound=0, powerup=False, timeout_ms=30000)
        q = lambda n: s.signal(n, 0)
        m = pointer_map(self.p)
        def word(values):
            return sum((z3.If(v, z3.BitVecVal(1 << i, 32), z3.BitVecVal(0, 32)) for i, v in values.items()), z3.BitVecVal(0, 32))
        old = word({i:q(n) for i,n in m.items()})
        data = word({i:z3.If(s.expr(self.p['logic'][n]['controls']['CE'],0), s.expr(self.p['logic'][n]['data'],0), q(n)) for i,n in m.items()})
        sgta = word({i:q(d['signal']) for i,d in capture_map(self.p,SGTA_GATES).items()})
        wa = word({i:q(d['signal']) for i,d in capture_map(self.p,[NAMES['word_a_lo_ce'],NAMES['word_a_hi_ce']],stages=1).items()})
        initial=z3.And(q(NAMES['cl']),q(NAMES['reload']))
        chain=q(sl(22,21,1,'Y'))
        step=z3.And(q(NAMES['segment_event']),q(NAMES['next_phase']))
        for left,right in POINTER_REPLICAS:
            same_sampling_contract(self.p,left,right)
            s.solver.add(q(left)==q(right))
        expected=z3.If(z3.And(z3.Or(initial,chain),z3.Not(step)),z3.If(initial,sgta,wa)&0xFFFFFFFC,
                       old+z3.If(step,z3.BitVecVal(8,32),z3.BitVecVal(0,32)))
        s.solver.add(q(self.p['roots']['RST#']['I']))
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(data!=expected)
        answer=s.solver.check()
        detail=''
        if answer==z3.sat:
            model=s.solver.model()
            detail=str({k:str(model.eval(v)) for k,v in dict(old=old,data=data,expected=expected,initial=initial,chain=chain,step=step,sgta=sgta,wa=wa).items()})
        self.assertEqual(answer,z3.unsat,detail)

    def test_master_address_load_pointer_or_word_a_else_accepted_dword_step(self):
        from virtexe_symbolic import System,z3
        s=System(self.p,bound=0,powerup=False,timeout_ms=30000)
        q=lambda n:s.signal(n,0)
        def word(values):
            return sum((z3.If(v,z3.BitVecVal(1<<i,32),z3.BitVecVal(0,32)) for i,v in values.items()),z3.BitVecVal(0,32))
        m=master_address_map(self.p)
        old=word({i:q(d['signal']) for i,d in m.items()})
        data=word({i:z3.If(s.expr(self.p['logic'][d['signal']].get('controls',{}).get('CE',{'constant':1}),0),s.expr(self.p['logic'][d['signal']]['data'],0),q(d['signal'])) for i,d in m.items()})
        ptr=word({i:q(n) for i,n in pointer_map(self.p).items()})
        wa=word({i:q(d['signal']) for i,d in capture_map(self.p,[NAMES['word_a_lo_ce'],NAMES['word_a_hi_ce']],stages=1).items()})
        load=q(sl(22,19,1,'YQ')); pending=q(NAMES['pending']); accept=q(NAMES['pci_accept'])
        for n in LOAD_REPLICAS:
            register_equivalence(self.p,sl(22,19,1,'YQ'),n)
            s.solver.add(q(n)==load)
        expected=z3.If(z3.Or(load,pending),z3.If(load,wa,ptr)&0xFFFFFFFC,
                       old+z3.If(accept,z3.BitVecVal(4,32),z3.BitVecVal(0,32)))
        s.solver.add(q(self.p['roots']['RST#']['I']))
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(data!=expected)
        answer=s.solver.check()
        detail=''
        if answer==z3.sat:
            model=s.solver.model()
            detail=str({k:str(model.eval(v)) for k,v in dict(old=old,data=data,expected=expected,load=load,pending=pending,accept=accept).items()})
        self.assertEqual(answer,z3.unsat,detail)

    def test_master_cbe_command_is_read_six_or_write_seven_under_explicit_selection(self):
        from virtexe_symbolic import System,z3
        s=System(self.p,bound=0,powerup=False,timeout_ms=30000)
        q=lambda n:s.signal(n,0)
        mode=q(sl(18,10,0,'YQ'))
        s.solver.add(q(self.p['roots']['RST#']['I']),
                     z3.Not(q(sl(2,21,1,'XQ'))),
                     z3.Not(q(sl(3,14,1,'XQ'))),
                     z3.Not(q(sl(4,9,0,'YQ'))),
                     z3.Not(q(sl(6,9,1,'YQ'))))
        bits=[s.expr(self.p['logic'][self.p['roots'][f'CBE#[{i}]']['O']]['data'],0) for i in range(4)]
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(z3.Or(bits[0]!=mode,z3.Not(bits[1]),z3.Not(bits[2]),bits[3]))
        self.assertEqual(s.solver.check(),z3.unsat)

    def test_chain_and_payload_load_use_whole_word_b_zero(self):
        from virtexe_symbolic import System,z3
        s=System(self.p,bound=0,powerup=False,timeout_ms=30000)
        q=lambda n:s.signal(n,0)
        wb=capture_map(self.p,[NAMES['word_b_lo_ce'],NAMES['word_b_hi_ce']],stages=1)
        zero=z3.And(*[z3.Not(q(d['signal'])) for d in wb.values()])
        terminal=q(NAMES['terminal']); za=q(NAMES['zero'])
        chain=z3.And(terminal,zero,z3.Not(za))
        load=z3.And(terminal,z3.Not(zero),z3.Not(za))
        s.solver.add(q(self.p['roots']['RST#']['I']))
        self.assertEqual(s.solver.check(),z3.sat)
        s.solver.add(z3.Or(q(sl(22,21,1,'Y'))!=chain,s.expr(self.p['logic'][sl(22,19,1,'YQ')]['data'],0)!=load))
        self.assertEqual(s.solver.check(),z3.unsat)

    def test_iimtc_counter_d_is_reload_or_decrement_modulo_32_bits(self):
        from virtexe_symbolic import System, z3
        s = System(self.p, bound=0, powerup=False, timeout_ms=30000)
        names = [sl(29, y, 1, pin) for y in range(13, 29) for pin in ("XQ", "YQ")]
        m = capture_map(self.p, IIMTC_GATES)
        def word(values):
            return sum((z3.If(v, z3.BitVecVal(1 << i, 32), z3.BitVecVal(0, 32)) for i, v in enumerate(values)), z3.BitVecVal(0, 32))
        old = word([s.signal(n, 0) for n in names])
        data = word([s.expr(self.p["logic"][n]["data"], 0) for n in names])
        load = word([s.signal(m[i]["signal"], 0) for i in range(32)])
        expected = z3.If(z3.And(s.signal(NAMES["cl"], 0), s.signal(NAMES["reload"], 0)), load, old - 1)
        s.solver.add(s.signal(self.p["roots"]["RST#"]["I"], 0))
        self.assertEqual(s.solver.check(), z3.sat)
        s.solver.add(data != expected)
        answer = s.solver.check()
        detail = ""
        if str(answer) == "sat":
            model = s.solver.model()
            detail = str({"old": model.eval(old), "actual": model.eval(data),
                          "expected": model.eval(expected)})
        self.assertEqual(answer, z3.unsat, detail)

    def test_descriptor_counter_d_loads_word_b_or_two_or_decrements(self):
        from virtexe_symbolic import System, z3
        s = System(self.p, bound=0, powerup=False, timeout_ms=30000)
        names = [sl(20, y, 0, pin) for y in range(10, 26) for pin in ("XQ", "YQ")]
        m = capture_map(self.p, [NAMES["word_b_lo_ce"], NAMES["word_b_hi_ce"]], stages=1)
        def word(values):
            return sum((z3.If(v, z3.BitVecVal(1 << i, 32), z3.BitVecVal(0, 32)) for i, v in enumerate(values)), z3.BitVecVal(0, 32))
        old = word([s.signal(n, 0) for n in names])
        data = word([s.expr(self.p["logic"][n]["data"], 0) for n in names])
        load = s.signal(sl(22, 19, 1, "YQ"), 0)
        for n in (sl(22, 19, 1, "XQ"), sl(22, 20, 0, "YQ"), sl(23, 20, 0, "YQ")):
            s.solver.add(s.signal(n, 0) == load)
        expected = z3.If(load, word([s.signal(m[i]["signal"], 0) for i in range(32)]),
                         z3.If(s.signal(NAMES["pending"], 0), z3.BitVecVal(2, 32), old - 1))
        s.solver.add(s.signal(self.p["roots"]["RST#"]["I"], 0))
        self.assertEqual(s.solver.check(), z3.sat)
        s.solver.add(data != expected)
        answer = s.solver.check()
        detail = ""
        if answer == z3.sat:
            model = s.solver.model()
            detail = str({"old": model.eval(old), "actual": model.eval(data), "expected": model.eval(expected),
                          "load": model.eval(load), "pending": model.eval(s.signal(NAMES["pending"], 0))})
        self.assertEqual(answer, z3.unsat, detail)


if __name__ == "__main__":
    unittest.main()
