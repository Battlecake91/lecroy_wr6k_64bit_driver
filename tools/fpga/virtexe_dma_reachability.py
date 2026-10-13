"""Initialized PCI launch-prefix diagnostics, never a replacement initiator.

The register dependency closure preserves the original configured contracts.
External bus stimulus and protocol observations are separate from those FFs.
PCILOGIC remains symbolic: a SAT prefix is conditional, not hardware reachability.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from virtexe_dma_handshake import H
from virtexe_routing import COMMIT, expression_nodes


def closed_cone(report, roots):
    """Retain transitive state, controls and memory dependencies without FF cuts."""
    todo, seen, memories = list(roots), set(), set()
    while todo:
        name = todo.pop()
        if name in seen:
            continue
        seen.add(name)
        node = report['logic'].get(name, {})
        expressions = {k: node[k] for k in ('data', 'controls', 'disable', 'memory') if k in node}
        for item in expression_nodes(expressions):
            if 'source' in item:
                todo.append(item['source'])
            if 'driver' in item:
                todo.append(item['driver'])
            memory = item.get('memory')
            if memory in report.get('memories', {}) and memory not in memories:
                memories.add(memory)
                # Include both ports and every clock/enable/address/data control.
                for dep in expression_nodes(report['memories'][memory]):
                    if 'source' in dep:
                        todo.append(dep['source'])
    return dict(report, logic={n: report['logic'][n] for n in seen if n in report['logic']},
                memories={n: report['memories'][n] for n in memories})


def launch_stimulus(t):
    """External host writes: PCI command, SGTA, IIMTC, IIMCL; then no response.

    A fixed host schedule is a diagnostic candidate. Its accepted phases must
    be checked, not assumed. BAR0 starts at its configured zero value.
    """
    transactions = ((2, True, 4, 6), (10, False, 0x40, 0x100000),
                    (18, False, 0x44, 1), (26, False, 0x48, 1))
    for start, config, address, data in transactions:
        if start <= t <= start + 5:
            address_phase = t == start
            return dict(frame=not address_phase, irdy=address_phase,
                        ad=address if address_phase else data,
                        cbe=(0xB if config else 7) if address_phase else 0,
                        idsel=config and address_phase, host=True)
    return dict(frame=True, irdy=True, ad=0, cbe=15, idsel=False, host=False)


def initialized_prefix(report, bound=40, timeout_ms=60000, descriptor_response=False):
    """One power-up-origin constraint system; no joins of runtime witnesses."""
    from virtexe_symbolic import System, clock_key, z3
    if report.get('project_combine_commit') != COMMIT:
        raise ValueError('architecture pin mismatch')
    if report.get('frame_validation', {}).get('status') != 'PASS':
        raise ValueError('private frame validation missing')
    if bound < 34:
        raise ValueError('bound must include the host launch prefix')
    roots = report['roots']
    watched = [H[n] for n in ('cl', 'initial', 'pending', 'pci_accept', 'accept',
                              'irq', 'done', 'start_state', 'v', 'issue', 'rewind')]
    watched += ['X5,Y10 SLICE[1].YQ', 'X7,Y10 SLICE[0].YQ']
    pci_roots = [roots[n] for n in ('RST#', 'GNT#', 'IDSEL', 'REQ#', 'FRAME#',
                                   'IRDY#', 'TRDY#', 'STOP#', 'DEVSEL#')]
    pci_roots += [roots[f'{prefix}[{i}]'] for prefix, width in (('AD', 32), ('CBE#', 4))
                  for i in range(width)]
    watched += [root[p] for root in pci_roots
                for p in ('O', 'T', 'I', 'IQ') if p in root]
    local = closed_cone(report, watched)
    pci = {'source': 'X24,Y29 BUFGCE[1].O', 'inv': False}
    s = System(local, bound=bound, powerup=True, timeout_ms=timeout_ms,
               schedule=[{clock_key(pci)}] * bound)
    q = s.signal
    host_done = z3.BoolVal(False)
    target_active, target_read, target_ready = [z3.BoolVal(False) for _ in range(3)]
    target_address = z3.BitVecVal(0, 32)
    accepted_reads, accepted_writes = [], []
    def bus_word(prefix, pin, t, width):
        value = z3.BitVecVal(0, 32)
        for i in range(width):
            value = value | z3.If(q(roots[f'{prefix}[{i}]'][pin], t),
                                  z3.BitVecVal(1 << i, 32), z3.BitVecVal(0, 32))
        return value
    for t in range(bound + 1):
        stimulus = launch_stimulus(t)
        if stimulus['host'] and not stimulus['irdy']:
            host_irdy = host_done
        else:
            host_irdy = z3.BoolVal(stimulus['irdy'])
            host_done = z3.BoolVal(False)
        s.solver.add(q(roots['RST#']['I'], t) == (t != 0),
                     q(roots['GNT#']['I'], t) == (t < 33),
                     q(roots['IDSEL']['I'], t) == stimulus['idsel'])
        mapped = z3.If(target_read,
                       z3.Or(*[target_address == address for address in
                               (0x100000, 0x100004, 0x100008, 0x10000C)]),
                       target_address == 0x200000)
        response = z3.And(target_active, target_ready, mapped, descriptor_response, t >= 33)
        for name, external in (('FRAME#', z3.BoolVal(stimulus['frame'])), ('IRDY#', host_irdy),
                               ('TRDY#', z3.Not(response)), ('STOP#', True), ('DEVSEL#', z3.Not(response))):
            pad = roots[name]
            s.solver.add(q(pad['I'], t) == z3.If(q(pad['T'], t),
                                                external, q(pad['O'], t)))
            if stimulus['host'] and name in ('FRAME#', 'IRDY#'):
                s.solver.add(q(pad['T'], t))
        for prefix, width, value in (('AD', 32, stimulus['ad']), ('CBE#', 4, stimulus['cbe'])):
            for bit in range(width):
                pad = roots[f'{prefix}[{bit}]']
                external = z3.BoolVal(bool(value & (1 << bit)))
                if prefix == 'AD':
                    memory_word = z3.If(target_address == 0x100000, z3.BitVecVal(1, 32),
                                  z3.If(target_address == 0x100004, z3.BitVecVal(0x200000, 32),
                                        z3.BitVecVal(0, 32)))
                    read_response = z3.And(response, target_read)
                    external = z3.If(read_response,
                                     (memory_word & (1 << bit)) != 0, external)
                    # A responding read target and the FPGA may not both drive AD.
                    s.solver.add(z3.Implies(read_response, q(pad['T'], t)))
                s.solver.add(q(pad['I'], t) == z3.If(q(pad['T'], t),
                    external, q(pad['O'], t)))
                if stimulus['host']:
                    s.solver.add(q(pad['T'], t))
        if stimulus['host'] and not stimulus['irdy']:
            host_done = z3.Or(host_done, z3.Not(q(roots['TRDY#']['I'], t)))
            if not launch_stimulus(t + 1)['host']:
                # Admit only complete host transactions within this candidate
                # schedule. This restricts the environment, not target logic.
                s.solver.add(host_done)
        address_start = z3.And(t >= 34, z3.Not(q(roots['FRAME#']['T'], t)),
                              z3.Not(q(roots['FRAME#']['O'], t)),
                              q(roots['FRAME#']['I'], max(0, t - 1)))
        if descriptor_response:
            command = bus_word('CBE#', 'O', t, 4)
            s.solver.add(z3.Implies(address_start, z3.And(
                z3.Not(target_active), q(roots['IRDY#']['I'], max(0, t - 1)),
                z3.Not(q(roots['GNT#']['I'], max(0, t - 1))),
                z3.Or(command == 6, command == 7),
                *[z3.Not(q(roots[f'{p}[{i}]']['T'], t))
                  for p, width in (('AD', 32), ('CBE#', 4)) for i in range(width)])))
            s.solver.add(z3.Implies(response, z3.And(
                *[q(roots[name]['T'], t) for name in ('TRDY#', 'DEVSEL#', 'STOP#')],
                z3.Not(q(roots['FRAME#']['T'], t)), z3.Not(q(roots['IRDY#']['T'], t)),
                *[q(roots[f'AD[{i}]']['T'], t) == target_read for i in range(32)],
                *[z3.Not(q(roots[f'CBE#[{i}]']['T'], t)) for i in range(4)],
                bus_word('CBE#', 'O', t, 4) == 0)))
        transferred = z3.And(response, z3.Not(q(roots['IRDY#']['I'], t)),
                             z3.Not(q(roots['TRDY#']['I'], t)))
        accepted_reads.append(z3.And(transferred, target_read))
        accepted_writes.append(z3.And(transferred, z3.Not(target_read)))
        final = z3.And(transferred, q(roots['FRAME#']['I'], t))
        target_ready = z3.And(target_active, z3.Not(final))
        target_active = z3.Or(address_start, z3.And(target_active, z3.Not(final)))
        target_read = z3.If(address_start, bus_word('CBE#', 'O', t, 4) == 6, target_read)
        target_address = z3.If(address_start, bus_word('AD', 'O', t, 32),
                              target_address + z3.If(transferred, z3.BitVecVal(4, 32), z3.BitVecVal(0, 32)))
    results = dict(scope='single initialized configured-state prefix, conditional architecture',
                   bound=bound, registers=len(s.registers), memories=len(s.memories),
                   unknowns=sorted(s.unknowns.values()),
                   hardblock_assigned=False, runtime_state_assigned=False,
                   target_environment='simple memory responder' if descriptor_response else 'no target response',
                   all_release_obligations='Unknown', queries={})
    properties = {
        'base_consistency': z3.BoolVal(True),
        'configuration_and_launch': z3.And(q('X5,Y10 SLICE[1].YQ', 33), q(H['cl'], 33)),
        'launch_then_master_start': z3.And(q(H['cl'], 33),
            z3.Or(*[q(H['start_state'], t) for t in range(33, bound + 1)])),
        'irq_by_end' if descriptor_response else 'irq_during_stalled_fetch': z3.And(q(H['cl'], 33),
            z3.Or(*[q(H['irq'], t) for t in range(33, bound + 1)])),
    }
    address_phases = []
    for t in range(34, bound + 1):
        # External ownership observation: after the last host cycle no other
        # initiator drives the bus, and grant was offered on an idle bus.
        pads = [roots[f'{p}[{i}]'] for p, width in (('AD', 32), ('CBE#', 4))
                for i in range(width)]
        address_phases.append(z3.And(
            z3.Not(q(roots['FRAME#']['T'], t)), z3.Not(q(roots['FRAME#']['O'], t)),
            q(roots['FRAME#']['I'], t - 1), q(roots['IRDY#']['I'], t - 1),
            z3.Not(q(roots['GNT#']['I'], t - 1)),
            *[z3.Not(q(pad['T'], t)) for pad in pads],
            *[q(roots[f'AD[{i}]']['O'], t) == bool(0x100000 & (1 << i)) for i in range(32)],
            *[q(roots[f'CBE#[{i}]']['O'], t) == bool(6 & (1 << i)) for i in range(4)]))
    properties['owned_descriptor_address_candidate'] = z3.And(q(H['cl'], 33), z3.Or(*address_phases))
    if descriptor_response:
        from virtexe_dma import NAMES, capture_map
        a = capture_map(report, [NAMES['word_a_lo_ce'], NAMES['word_a_hi_ce']], stages=1)
        b = capture_map(report, [NAMES['word_b_lo_ce'], NAMES['word_b_hi_ce']], stages=1)
        captured = z3.Or(*[z3.And(
            *[q(b[i]['signal'], t) == bool(1 & (1 << i)) for i in range(32)],
            *[q(a[i]['signal'], t) == bool(0x200000 & (1 << i)) for i in range(32)])
            for t in range(34, bound + 1)])
        properties['owned_two_read_beats_and_descriptor_capture'] = z3.And(
            q(H['cl'], 33), z3.Or(*address_phases),
            z3.Sum(*[z3.If(p, 1, 0) for p in accepted_reads]) >= 2, captured)
        properties['same_execution_descriptor_capture_and_irq'] = z3.And(
            properties['owned_two_read_beats_and_descriptor_capture'],
            z3.Or(*[q(H['irq'], t) for t in range(33, bound + 1)]))
    for name, predicate in properties.items():
        s.solver.push()
        s.solver.add(predicate)
        answer = s.solver.check()
        result = dict(solver=str(answer), status='Conditional' if answer != z3.unknown else 'Unknown')
        if answer == z3.sat:
            model = s.solver.model()
            result['trace'] = [{n: z3.is_true(model.eval(q(H[n], t), model_completion=True))
                                for n in ('cl', 'pending', 'start_state', 'pci_accept', 'irq')}
                               for t in range(bound + 1)]
            # These are observations of this same model, never forced acknowledgements.
            result['host_acceptance_edges'] = [t for t in range(bound + 1)
                if launch_stimulus(t)['host']
                and z3.is_false(model.eval(q(roots['IRDY#']['I'], t), model_completion=True))
                and z3.is_false(model.eval(q(roots['TRDY#']['I'], t), model_completion=True))]
            result['descriptor_address_edges'] = [34 + i for i, p in enumerate(address_phases)
                                                  if z3.is_true(model.eval(p, model_completion=True))]
            result['post_launch_acceptance_edges'] = [t for t in range(33, bound + 1)
                if z3.is_true(model.eval(q(H['pci_accept'], t), model_completion=True))]
            result['target_read_acceptance_edges'] = [t for t, p in enumerate(accepted_reads)
                                                      if z3.is_true(model.eval(p, model_completion=True))]
            result['target_write_acceptance_edges'] = [t for t, p in enumerate(accepted_writes)
                                                       if z3.is_true(model.eval(p, model_completion=True))]
        elif answer == z3.unknown:
            result['reason'] = s.solver.reason_unknown()
        results['queries'][name] = result
        s.solver.pop()
        print(f'Initialized prefix {name}: {answer}', flush=True)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--bound', type=int, default=40)
    parser.add_argument('--timeout-ms', type=int, default=60000)
    parser.add_argument('--descriptor-response', action='store_true')
    args = parser.parse_args()
    if args.bound < 34:
        parser.error('bound must include the complete host launch prefix (at least 34)')
    repo = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    if output == args.report.resolve() or output.is_relative_to(repo):
        parser.error('private diagnostic output must be outside the repository and not the input')
    result = initialized_prefix(json.loads(args.report.read_text(encoding='utf-8')),
                                args.bound, args.timeout_ms, args.descriptor_response)
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
