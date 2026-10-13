"""Configured completion/INTA local checks, not DMA retirement certificates."""
from __future__ import annotations

import itertools

from virtexe_dma import NAMES, capture_map, sl
from virtexe_mmio import functional_expression
from virtexe_routing import COMMIT, evaluate_data, register_contract


MASK_GATE = sl(12, 13, 0, 'X')
MASKS = (sl(16, 7, 0, 'YQ'), sl(16, 7, 0, 'XQ'),
         sl(12, 8, 0, 'YQ'), sl(12, 8, 0, 'XQ'),
         sl(13, 8, 1, 'YQ'), sl(13, 8, 1, 'XQ'))
SOURCES = (NAMES['irq'], sl(18, 8, 1, 'YQ'), sl(18, 11, 0, 'YQ'),
           sl(18, 11, 0, 'XQ'), sl(15, 8, 1, 'YQ'), sl(16, 8, 0, 'YQ'))
INHIBIT = sl(7, 10, 1, 'XQ')
PAD_O, PAD_T = ('X0,Y12 IOI[3].PAD_' + p for p in ('O', 'T'))
PAD_LOGIC = (PAD_O, PAD_T, sl(11, 8, 1, 'Y'), sl(12, 8, 1, 'X'),
             sl(13, 8, 0, 'Y'), sl(16, 7, 1, 'Y'))
PCI_CLOCK = {'source': 'X24,Y29 BUFGCE[1].O', 'inv': False}


def interrupt_checks(report):
    """Exhaust all 8192 assignments of six sources/masks and the inhibit FF."""
    if report.get('project_combine_commit') != COMMIT:
        raise ValueError('Project Combine pin mismatch')
    if report['roots'].get('INTA#') != {'O': PAD_O, 'T': PAD_T}:
        raise ValueError('documented INTA# root missing; use include_interrupt_output')
    capture = capture_map(report, [MASK_GATE])
    if {i: d['signal'] for i, d in capture.items()} != dict(enumerate(MASKS)):
        raise ValueError('INTEN capture map differs')
    names = (*SOURCES, *MASKS, INHIBIT)
    for bits in itertools.product((False, True), repeat=len(names)):
        cuts = dict(zip(names, bits))
        expected = cuts[INHIBIT] or not any(cuts[s] and cuts[m] for s, m in zip(SOURCES, MASKS))
        if evaluate_data(report['logic'][PAD_O]['data'], report['logic'], cuts):
            raise ValueError('INTA# is not configured low/Hi-Z')
        if evaluate_data(report['logic'][PAD_T]['data'], report['logic'], cuts) != expected:
            raise ValueError('INTA# disable equation differs')
    return {'status': 'Verified', 'rows': 8192,
            'scope': 'configured combinational local cuts, not external line observation',
            'equation': 'T=G4 || !OR(source[i] && INTEN[i]); PAD_O=0',
            'generation_identity': 'none in this source/output cone; system provenance Unknown'}


def completion_contracts(report):
    """Compact contracts only; do not export the transitive private network."""
    names = {k: NAMES[k] for k in ('segment_event', 'terminal', 'done', 'irq', 'reload',
                                 'cl', 'pending', 'load_replica', 'next_phase')}
    names.update({f'inten{i}': n for i, n in enumerate(MASKS)})
    names['inhibit'] = INHIBIT
    result = {}
    for key, name in names.items():
        c = register_contract(name, report['logic'][name])
        if c['status'] != 'modeled_async_ff':
            raise ValueError('unsupported completion contract: ' + key)
        result[key] = dict(signal=name, **{k: functional_expression(c[k]) for k in
                                        ('data', 'clock', 'ce', 'sr', 'init', 'flags', 'status')})
    # Validate the two inventory qualifiers at their actual local LUT cuts.
    qualifiers = (
        ('next_phase', (NAMES['next_phase'], NAMES['segment_event'], NAMES['phase'], NAMES['accept']),
         lambda n, f, p, a: n and not f or p and a),
        ('load_replica', (NAMES['zero'], sl(20, 19, 1, 'Y'), sl(19, 21, 0, 'Y'), NAMES['terminal']),
         lambda z, u, v, l: l and not z and not (u and v)),
    )
    for key, cuts, expected in qualifiers:
        for bits in itertools.product((False, True), repeat=4):
            actual = evaluate_data(result[key]['data'], report['logic'], dict(zip(cuts, bits)))
            if actual != bool(expected(*bits)):
                raise ValueError('completion qualifier differs: ' + key)
    return result


def completion_queries(report, timeout_ms=10000):
    """Small actual-FF projections with explicit free named-cut histories.

    No host/PCI stimuli, transaction generator, new idle state or token counter.
    UNSAT means only that a local conditional mismatch is impossible. SAT does
    not establish initialized reachability of an acquisition or a hardware bug.
    """
    from virtexe_symbolic import System, clock_key, z3
    interrupt_checks(report)
    completion_contracts(report)
    results = []
    aliases = dict(NAMES, mask0=MASKS[0], inhibit=INHIBIT, mask_gate=MASK_GATE,
                   cl_gate=sl(17, 13, 1, 'Y'), mask_data='X17,Y1 TBUS.OUT')

    def run(title, states, bound, setup, assertion, witness=False, reset=False, powerup=False):
        wanted = {aliases.get(n, n) for n in states}
        # Only these original FFs and the proven output/gate LUTs are modeled.
        # Omitted combinational/state sources are explicit fresh named cuts.
        wanted.update((*PAD_LOGIC, NAMES['segment_gate'], sl(25, 20, 1, 'Y'),
                       report['roots']['RST#']['I']))
        local = dict(report, logic={n: report['logic'][n] for n in wanted}, memories={})
        s = System(local, bound=bound, powerup=powerup, timeout_ms=timeout_ms,
                   schedule=[{clock_key(PCI_CLOCK)}] * bound)
        q = lambda n, t=0: s.signal(aliases.get(n, n), t)
        for t in range(bound + 1):
            s.solver.add(q(report['roots']['RST#']['I'], t) == (not reset))
        setup(s, q)
        base = s.solver.check()
        s.solver.add(assertion(s, q) if witness else z3.Not(assertion(s, q)))
        answer = s.solver.check()
        results.append(dict(query=title, base_consistency=str(base), solver=str(answer), bound=bound,
                            initial_state='configured INIT (local projection)' if powerup else 'arbitrary runtime',
                            scope='local actual-FF projection; not legal-launch or hardware safety proof',
                            kind='constructed witness' if witness else 'conditional mismatch',
                            assumptions=['explicit PCI edges only', 'RST# asserted' if reset else 'RST# deasserted',
                                         'omitted sources are independent named cuts; no fairness or transaction ownership'],
                            unknown_count=len(s.unknowns)))
        if answer == z3.unknown:
            results[-1]['solver_reason'] = s.solver.reason_unknown()

    def pipeline(s, q):
        s.solver.add(*[z3.Not(q(n)) for n in ('segment_event', 'terminal', 'done', 'irq')])
        for t in range(4):
            s.solver.add(q('segment_zero', t), z3.Not(q('load_replica', t)),
                         z3.Not(q('pending', t)), q('next_phase', t), q('zero', t),
                         z3.Not(q('cl', t)), z3.Not(q('clear', t)))
    run('terminal pipeline F1 L2 C3 IRQ4', ('segment_event', 'terminal', 'done', 'irq'), 4,
        pipeline, lambda s, q: z3.And(q('segment_event', 1), q('terminal', 2), q('done', 3),
                                      q('irq', 4), z3.Not(q('irq', 3))))

    def ack(c, bit):
        def setup(s, q):
            s.solver.add(q('done') == c, q('cl'), z3.Not(q('terminal')),
                         q('irq'), q('clear'), q('data0') == bit)
        return setup
    run('C set has priority over INTST ACK', ('done', 'irq'), 1, ack(True, True),
        lambda s, q: q('irq', 1))
    run('ACK clears IRQ only when C0 and data0=1', ('done', 'irq'), 1, ack(False, True),
        lambda s, q: z3.Not(q('irq', 1)))
    run('ACK data0=0 retains IRQ', ('done', 'irq'), 1, ack(False, False),
        lambda s, q: q('irq', 1))

    def mask(s, q):
        s.solver.add(q('done'), q('irq'), q('cl'), q('mask0'), q('mask_gate'),
                     z3.Not(q('mask_data')), z3.Not(q('terminal')))
        for t in (0, 1):
            s.solver.add(z3.Not(q('inhibit', t)), *[z3.Not(q(n, t)) for n in MASKS[1:]])
    run('INTEN0 masks pad contribution without clearing C or IRQ', ('done', 'irq', 'mask0'), 1,
        mask, lambda s, q: z3.And(q('done', 1), q('irq', 1), z3.Not(q('mask0', 1)),
                                  z3.Not(q(PAD_T)), q(PAD_T, 1)))

    def clear(s, q):
        s.solver.add(q('cl'), q('done'), q('irq'))
        for t in range(3):
            s.solver.add(q('cl_gate', t) == (t == 0), q('data0', t) == (t > 0),
                         q('clear', t), z3.Not(q('terminal', t)))
    run('IIMCL0 then ACK respects old-state pipeline', ('cl', 'done', 'irq', 'reload'), 3,
        clear, lambda s, q: z3.And(z3.Not(q('cl', 1)), q('done', 1), z3.Not(q('done', 2)),
                                   q('irq', 2), z3.Not(q('irq', 3)), q('reload', 2)))

    def restart(s, q):
        s.solver.add(q('cl'), q('done'), q('irq'), q('reload'))
        for t in range(2):
            s.solver.add(q('cl_gate', t), q('data0', t), q('clear', t),
                         z3.Not(q('terminal', t)))
    run('IIMCL1 does not clear retained C when cl1', ('cl', 'done', 'irq', 'reload'), 2,
        restart, lambda s, q: z3.And(q('done', 2), q('irq', 2), z3.Not(q('reload', 1))))

    def repeated(s, q):
        s.solver.add(z3.Not(q('done')), z3.Not(q('irq')))
        for t in range(5):
            s.solver.add(z3.Not(q('cl', t)), q('terminal', t) == (t in (0, 3)),
                         q('zero', t), q('clear', t), q('data0', t))
    run('two terminal pulses reassert source without a generation tag', ('done', 'irq'), 5,
        repeated, lambda s, q: z3.And(q('irq', 2), z3.Not(q('irq', 3)), q('irq', 5)))

    initial_states = ('segment_event', 'terminal', 'done', 'irq', 'cl', 'pending',
                      'load_replica', 'next_phase', 'reload', 'mask0', 'inhibit')
    initial_value = lambda s, q: z3.And(q('reload'), *[z3.Not(q(n)) for n in initial_states if n != 'reload'])
    run('async reset forces INIT even without a clock edge', initial_states, 0,
        lambda s, q: None, initial_value, reset=True)
    run('configured INIT vector in local projection', initial_states, 0,
        lambda s, q: None, initial_value, powerup=True)

    def masked(s, q):
        s.solver.add(z3.Not(q('mask0')))
    run('mask0 removes IRQ dependence while other sources remain arbitrary', (*MASKS, *SOURCES, INHIBIT), 0,
        masked, lambda s, q: z3.substitute(q(PAD_T), (q('irq'), z3.BoolVal(False))) ==
                             z3.substitute(q(PAD_T), (q('irq'), z3.BoolVal(True))))

    def all_masked(s, q):
        s.solver.add(*[z3.Not(q(n)) for n in MASKS])
    run('all six masks zero disables pad regardless of source state', (*MASKS, *SOURCES, INHIBIT), 0,
        all_masked, lambda s, q: q(PAD_T))

    def bookkeeping(s, q):
        from virtexe_dma import payload_map
        from virtexe_dma_handshake import H, RX_WRITE, RX_READ, RX_PUBLISHED, RX_SNAPSHOT
        staging = payload_map(report)[0]
        write0 = staging['write_address']['0']['source']
        read0 = staging['read_address']['0']['source']
        s.solver.add(z3.Not(q('done')), z3.Not(q('irq')))
        for t in range(2):
            s.solver.add(q('terminal', t), q('zero', t), z3.Not(q('cl', t)),
                         q('pending', t), q(H['q0'], t), q(H['available0'], t),
                         q(H['issue'], t), q(H['rewind'], t),
                         q(RX_WRITE[0], t), z3.Not(q(RX_READ[0], t)),
                         q(RX_SNAPSHOT[0], t), z3.Not(q(RX_PUBLISHED[0], t)),
                         q(write0, t), z3.Not(q(read0, t)))
    run('retained terminal can set IRQ with nonzero bookkeeping cuts', ('done', 'irq'), 2,
        bookkeeping, lambda s, q: z3.And(q('done', 1), q('irq', 2)), witness=True)
    return results
