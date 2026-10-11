"""Focused configured PCI pipeline checks; no replacement PCI state machine.

All semantic protocol observations are conditional environment bookkeeping.
The original decoder and System provide the hardware equations and timing.
"""
from __future__ import annotations

from virtexe_dma import NAMES, sl
from virtexe_routing import data_support, expression_nodes


H = dict(NAMES, issue=sl(19, 8, 1, 'X'), rewind=sl(20, 7, 0, 'X'),
         issue_base=sl(6, 8, 1, 'Y'),
         a_state=sl(3, 16, 0, 'XQ'), k_state=sl(5, 15, 0, 'YQ'),
         c_state=sl(3, 7, 1, 'XQ'), start_state=sl(4, 9, 1, 'XQ'),
         e_state=sl(2, 14, 0, 'XQ'), irdy_raw='X0,Y15 IOI[3].I',
         gnt_raw='X0,Y16 IOI[2].I', v_hold=sl(5, 5, 0, 'Y'),
         address_disable=sl(4,9,0,'YQ'), stop_iq='X0,Y15 IOI[2].IQ',
         devsel_iq='X0,Y17 IOI[1].IQ', termination_k=sl(3,4,0,'X'),
         mode=sl(18, 10, 0, 'YQ'), target=sl(2, 21, 1, 'XQ'),
         j=sl(4, 13, 0, 'XQ'), h=sl(5, 8, 0, 'YQ'),
         q0=sl(18, 4, 1, 'YQ'), q1=sl(18, 4, 0, 'XQ'),
         rollback=sl(19, 7, 0, 'YQ'), direction=sl(17, 7, 1, 'Y'),
         toggle=sl(18, 4, 1, 'X'), end_edge=sl(15, 7, 1, 'X'),
         ended=sl(15, 7, 0, 'YQ'), active=sl(5, 5, 0, 'XQ'),
         termination=sl(8, 7, 0, 'YQ'), write_mode=sl(15, 7, 1, 'YQ'),
         write_accept=sl(19, 8, 0, 'XQ'),
         tc_ce_lo=sl(25, 26, 0, 'X'), tc_ce_hi=sl(25, 26, 1, 'X'),
         reload_hi=sl(25, 20, 1, 'YQ'),
         seg_ce_lo=sl(18, 17, 0, 'Y'), seg_ce_hi=sl(18, 17, 0, 'X'),
         available0=sl(22, 6, 0, 'XQ'), available1=sl(22, 7, 0, 'XQ'),
         available2=sl(22, 8, 0, 'XQ'), available3=sl(22, 8, 0, 'YQ'),
         available_nonzero=sl(22, 7, 1, 'X'), throttle=sl(22, 7, 1, 'YQ'),
         received_available=sl(37, 7, 1, 'X'), write=sl(37, 7, 1, 'Y'))

# Independently reduced predicates, exhaustively compared with original LUTs.
HANDSHAKE_RELATIONS = (
    ('j', ('target', 'a_state', 'k_state', 'c_state', 'v', 'start_state', 'trdy', 'i', 'irdy_raw', 'e_state'),
     lambda v: not v['target'] and (v['a_state'] and v['k_state'] or v['c_state'] and v['v'] or
                 v['start_state'] or not v['trdy'] and not v['i'] and v['v'] or
                 not v['irdy_raw'] and v['k_state'] and not v['e_state'])),
    ('v', ('start_state', 'gnt_raw', 'v_hold', 'v'),
     lambda v: v['start_state'] and not v['gnt_raw'] or v['v_hold'] and v['v']),
    ('termination', ('address_disable','termination_k','stop_iq','devsel_iq'),
     lambda v: v['address_disable'] and (v['termination_k'] or not v['stop_iq'] and v['devsel_iq'])),
    ('issue', ('mode', 'target', 'b', 'j', 'v', 'h'),
     lambda v: v['mode'] and not v['target'] and not v['b'] and v['j'] and v['v'] and v['h']),
    ('rewind', ('rollback', 'q0', 'q1'),
     lambda v: v['rollback'] and (v['q0'] or v['q1'])),
    ('direction', ('mode', 'issue_base', 'pci_accept', 'rewind'),
     lambda v: v['mode'] and not v['issue_base'] and (v['pci_accept'] or v['rewind'])),
    ('toggle', ('direction', 'issue', 'pci_accept', 'rewind'),
     lambda v: v['direction'] or v['issue'] and not v['pci_accept'] and not v['rewind']),
    ('q0', ('q0', 'toggle'), lambda v: v['q0'] != v['toggle']),
    ('q1', ('q1', 'q0', 'toggle', 'direction'),
     lambda v: v['q1'] != ((v['toggle'] and v['q0']) != v['direction'])),
    ('end_edge', ('ended', 'active'), lambda v: v['ended'] and not v['active']),
    ('rollback', ('rewind', 'end_edge', 'termination', 'read_mode', 'write_mode'),
     lambda v: v['rewind'] or v['end_edge'] and not v['termination'] and (v['read_mode'] or v['write_mode'])),
    ('write_accept', ('pci_accept', 'write_mode'), lambda v: v['pci_accept'] and v['write_mode']),
    ('tc_ce_lo', ('cl', 'reload', 'aux', 'write_accept'),
     lambda v: v['cl'] and v['reload'] or v['aux'] and v['write_accept']),
    ('tc_ce_hi', ('cl', 'reload_hi', 'aux', 'write_accept'),
     lambda v: v['cl'] and v['reload_hi'] or v['aux'] and v['write_accept']),
    ('seg_ce_lo', ('load_replica', 'pending', 'pci_accept'),
     lambda v: v['load_replica'] or v['pending'] or v['pci_accept']),
    ('seg_ce_hi', ('load_replica', 'pending', 'pci_accept'),
     lambda v: v['load_replica'] or v['pending'] or v['pci_accept']),
    ('available_nonzero', ('available0', 'available1', 'available2', 'available3'),
     lambda v: any(v.values())),
    ('throttle', ('available1', 'available2', 'available3'),
     lambda v: v['available3'] and (v['available1'] or v['available2'])),
    ('write', ('received_available', 'throttle'),
     lambda v: v['received_available'] and not v['throttle']),
)


def local_checks(report):
    from virtexe_routing import evaluate_data
    results = {}
    for name, variables, expected in HANDSHAKE_RELATIONS:
        for assignment in range(1 << len(variables)):
            values = {v: bool(assignment & (1 << i)) for i, v in enumerate(variables)}
            actual = evaluate_data(report['logic'][H[name]]['data'], report['logic'],
                                   {H[k]: v for k, v in values.items()})
            if actual != bool(expected(values)):
                raise ValueError(f'{name}: assignment {assignment} differs')
        results[name] = dict(signal=H[name], rows=1 << len(variables), status='Verified',
                             scope='local cuts, not initialized PCI reachability')
    return results


def pending_next(q, mode, issue, accepted, rewind):
    """Actual two-bit arithmetic, NOT a presumed outstanding-write count."""
    direction = mode and not issue and (accepted or rewind)
    toggle = direction or issue and not accepted and not rewind
    return (q + ((-1 if direction else 1) if toggle else 0)) & 3


def receive_availability_checks(report):
    """Four pairwise comparators feeding W; no CDC or wrap interpretation."""
    from virtexe_routing import evaluate_data
    gates = (sl(39,6,0,'Y'),sl(38,7,0,'X'),sl(39,7,0,'Y'),sl(39,9,0,'Y'))
    result = []
    for n in gates:
        data = report['logic'][n]['data']
        pairs = ((0,2),(1,3)) if n == gates[2] else ((0,3),(1,2))
        pins = [data['inputs'][str(i)]['source'] for i in range(4)]
        for assignment in range(16):
            values = {pin:bool(assignment & (1<<i)) for i,pin in enumerate(pins)}
            expected = any(values[pins[a]] != values[pins[b]] for a,b in pairs)
            if evaluate_data(data,report['logic'],values) != expected:
                raise ValueError('receive pointer comparator differs')
        result.extend((pins[a],pins[b]) for a,b in pairs)
    data = report['logic'][H['received_available']]['data']
    for assignment in range(16):
        values={n:bool(assignment & (1<<i)) for i,n in enumerate(gates)}
        if evaluate_data(data,report['logic'],values) != any(values.values()):
            raise ValueError('receive availability reduction differs')
    return dict(status='Verified',pairs=result,scope='eight paired FF inequalities; no extra wrap bit or CDC proof')


def owned_phase(*, owned, data_phase, output_enabled, frame, irdy, trdy,
                devsel, stop, transferred_before, claimed_before,
                response_window_expired=False):
    """PCI protocol observer, not a hardware state variable or constraint.

    Inputs use electrical levels (False means asserted). Ownership and phase
    history must come from an external contract, not current GNT# alone.
    PCI_CE is deliberately not assigned a value by this observer.
    """
    phase = owned and data_phase and output_enabled
    transfer = phase and not irdy and not trdy
    terminated = phase and not irdy and (not stop or (frame and not trdy))
    return dict(accepted=transfer, final_accepted=transfer and frame,
                address_phase=owned and output_enabled and not data_phase and not frame,
                wait=phase and (irdy or trdy),
                retry=phase and not stop and trdy and not devsel and not transferred_before,
                disconnect_with_data=phase and not stop and not trdy and not devsel,
                disconnect_without_data=phase and not stop and trdy and not devsel and transferred_before,
                target_abort=phase and claimed_before and not stop and devsel and trdy,
                master_abort=phase and response_window_expired and not claimed_before and devsel and trdy and stop,
                termination=terminated)


def cone_summary(report, root):
    """Private full transitive dependency inventory; no extracted network export."""
    todo, seen, registers, boundaries, pads = [root], set(), set(), set(), set()
    while todo:
        n = todo.pop()
        if n in seen:
            continue
        seen.add(n)
        node = report['logic'].get(n, {})
        kind = node.get('kind')
        if kind == 'register':
            registers.add(n)
        elif kind == 'pad_input':
            pads.add(n)
        elif kind not in ('combinational', 'tristate_driver', 'memory_output'):
            boundaries.add(n)
        for e in expression_nodes({k: node[k] for k in ('data', 'controls', 'disable') if k in node}):
            if 'source' in e:
                todo.append(e['source'])
            elif 'unknown' in e:
                boundaries.add(str(e['unknown']))
    return dict(nodes=len(seen), registers=sorted(registers), pads=sorted(pads),
                boundaries=sorted(boundaries),
                immediate_state_cuts=sorted(data_support(report['logic'][root]['data'], report['logic'])))


def projection(report, states, outputs=(), memories=()):
    """Keep original state contracts and all combinational logic; cut other FFs.

    Named cut registers become fresh symbolic sources, never invented latches.
    This is an overapproximation, not a legal-launch reachability model.
    """
    wanted = {H.get(n, n) for n in (*states, *outputs)}
    logic = {n: d for n, d in report['logic'].items()
             if d.get('kind') in ('combinational', 'tristate_driver', 'pad_input', 'architecture_boundary')
             or n in wanted}
    return dict(report, logic=logic, memories={n: report['memories'][n] for n in memories})


SCENARIOS = (
    'issue without accepted phase', 'accepted phase without issue',
    'issue then rewind', 'rewind without preceding issue',
    'repeated rollback after retry-shaped response', 'partial transfer then disconnect-shaped response',
    'STOP asserted during final word', 'segment zero while raw handshake incomplete',
    'IRQ with nonzero prefetch state', 'IIMCL clear during pending rollback',
    'read pointer wraps while raw handshake incomplete', 'restart with retained staging RAM',
)


def word_observer(events, size=16):
    """Verification-only token ledger under an explicit output-pipeline contract.

    Events supply already identified word identities, not guessed pad values.
    This function never feeds constraints into System. Violations are failures
    of that supplied event interpretation, not demonstrated FPGA defects.
    """
    ram, pending, accepted, violations = {}, [], [], []
    for event in events:
        if 'write' in event:
            address, token = event['write']
            address %= size
            if address in ram and ram[address] in pending:
                violations.append('overwrite of pending token')
            ram[address] = token
        if 'issue' in event:
            token = ram.get(event['issue'] % size)
            if token is None:
                violations.append('issue without valid token')
            pending.append(token)
        if event.get('accept'):
            if not pending:
                violations.append('accept without issued token')
            else:
                token = pending.pop(0)
                if token in accepted:
                    violations.append('duplicate acceptance')
                accepted.append(token)
        if event.get('rewind'):
            if not pending:
                violations.append('rewind without issued token')
            else:
                pending.pop()
    return dict(pending=pending, accepted=accepted, violations=violations,
                role='verification observer only; no hardware constraints')


def handshake_queries(report, timeout_ms=30000):
    """Twelve actual-register cone diagnostics with explicit limitations."""
    from virtexe_symbolic import System, clock_key, z3
    pci = {'source': 'X24,Y29 BUFGCE[1].O', 'inv': False}
    rst = report['roots']['RST#']['I']
    core = ('q0', 'q1', 'rollback', 'pci_accept', 'write_accept', 'done', 'irq')
    results = []
    for number, title in enumerate(SCENARIOS, 1):
        bound = 4 if number in (4, 5) else 2
        states = list(core)
        if number in (10, 12):
            states += ['cl']
        read = [sl(29, 8, 1, 'YQ'), sl(29, 7, 1, 'YQ'), sl(29, 7, 0, 'YQ'), sl(29, 10, 1, 'YQ')]
        if number == 11:
            states += read
        segment = [sl(20, y, 0, pin) for y in range(10, 26) for pin in ('XQ', 'YQ')]
        if number == 8:
            states += segment
        memories = ()
        if number == 12:
            from virtexe_dma import payload_map
            memories = tuple(d['memory'] for d in payload_map(report).values())
        local = projection(report, states, memories=memories)
        s = System(local, bound=bound, powerup=False, timeout_ms=timeout_ms,
                   schedule=[{clock_key(pci)}] * bound)
        q = lambda n, t: s.signal(H.get(n, n), t)
        for t in range(bound + 1):
            s.solver.add(q(rst, t))
        # Stable explicit cuts only for the rollback-drain experiments.
        if number in (3, 4, 5):
            for t in range(bound + 1):
                s.solver.add(q('mode', t), z3.Not(q('pci_accept', t)))
            s.solver.add(z3.Not(q('q0', 0)), z3.Not(q('q1', 0)), z3.Not(q('rollback', 0)))
        if number == 1:
            predicate = z3.And(q('issue', 0), q('trdy', 0), z3.Not(q('pci_accept', 0)))
        elif number == 2:
            predicate = z3.And(z3.Not(q('issue', 0)), q('pci_accept', 0))
        elif number == 3:
            predicate = z3.And(q('issue', 0), z3.Not(q('issue', 1)), q('rewind', 1))
        elif number == 4:
            s.solver.add(*[z3.Not(q('issue', t)) for t in range(bound)])
            predicate = z3.Or(*[q('rewind', t) for t in range(bound + 1)])
        elif number == 5:
            s.solver.add(*[q('issue', t) == (t < 2) for t in range(bound)])
            predicate = z3.And(q('rewind', 2), q('rewind', 3), z3.Not(q('q0', 4)), z3.Not(q('q1', 4)))
        elif number == 6:
            predicate = z3.And(q('pci_accept', 0), z3.Not(q('issue', 0)),
                               z3.Not(q('stop', 1)), q('trdy', 1), q('rewind', 1))
        elif number == 7:
            predicate = z3.And(q('b', 0), z3.Not(q('stop', 0)), z3.Not(q('trdy', 0)),
                               q('segment_zero', 0), q('pci_accept', 1))
        elif number == 8:
            s.solver.add(*[q(n, 0) == (i == 0) for i, n in enumerate(segment)],
                         z3.Not(q('pending', 0)))
            for n in (sl(22, 19, 1, 'YQ'), *[sl(x,y,n,p) for x,y,n,p in
                       ((22,19,1,'XQ'),(22,20,0,'YQ'),(22,20,1,'YQ'),(23,20,0,'YQ'))]):
                s.solver.add(z3.Not(q(n, 0)))
            predicate = z3.And(q('trdy', 0), q('i', 0), q('pci_accept', 0),
                               *[z3.Not(q(n, 1)) for n in segment])
        elif number == 9:
            s.solver.add(z3.Not(q('irq', 0)), z3.Not(q('done', 0)))
            predicate = z3.And(q('terminal', 0), q('zero', 0), q('irq', 2),
                               z3.Or(q('q0', 2), q('q1', 2)))
        elif number == 10:
            predicate = z3.And(q('cl', 0), z3.Not(q('cl', 1)),
                               q(sl(17, 13, 1, 'Y'), 0), z3.Not(q('data0', 0)), q('rewind', 1))
        elif number == 11:
            s.solver.add(*[q(n, 0) for n in read], z3.Not(q('rewind', 0)))
            # Exact shared-D bit0 replica, conditional equal runtime history.
            s.solver.add(q(sl(29, 8, 0, 'YQ'), 0) == q(read[0], 0))
            predicate = z3.And(q('issue', 0), q('trdy', 0), *[z3.Not(q(n, 1)) for n in read])
        else:
            for t in range(bound):
                s.solver.add(z3.Not(q('write', t)))
            predicate = z3.And(z3.Not(q('cl', 0)), q('cl', 1),
                               q(sl(17, 13, 1, 'Y'), 0), q('data0', 0),
                               *[s.memory(n, 0) == s.memory(n, 2) for n in memories])
        base = s.solver.check()
        s.solver.add(predicate)
        answer = s.solver.check()
        item = dict(number=number, query=title, base_consistency=str(base), solver=str(answer),
                    bound=bound, status='Inconclusive', unbounded_proof=False,
                    initial_state='zero local q/rollback only' if number in (3, 4, 5) else 'arbitrary runtime',
                    assumptions=['RST# deasserted at every boundary', 'PCI edges only; no remote edge',
                                 'external named FF cuts symbolic; no legal launch or PCI history claim',
                                 'PCI_CE and dedicated PCIIOB behavior unresolved'],
                    observer_constraints=False, unknown_count=len(s.unknowns))
        if number in (3, 4, 5):
            item['assumptions'] += ['mode=1, A=0 throughout; issue schedule stated by query']
        if answer == z3.sat:
            model = s.solver.model()
            item['cut_trace'] = [{n: z3.is_true(model.eval(q(n, t), model_completion=True))
                                  for n in ('issue', 'rewind', 'q0', 'q1', 'pci_accept', 'irq', 'cl')}
                                 for t in range(bound + 1)]
        elif answer == z3.unknown:
            item['reason'] = s.solver.reason_unknown()
        results.append(item)
        print(f'Handshake query {number}: base={base}, result={answer}', flush=True)
    return results
