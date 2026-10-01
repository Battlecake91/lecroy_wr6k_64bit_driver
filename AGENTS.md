# AGENTS.md

This file is the compact operating guide for the repository.
It contains only information required to work correctly from the current project state.
It is not a project history, changelog, investigation log, or handoff archive.

## 1. Repository and communication

- Repository: `Battlecake91/lecroy_wr6k_64bit_driver`
- Chat with the owner is in German.
- Repository documentation, code, comments, commit messages, and public-facing text are in English.
- The repository is public. Do not commit proprietary LeCroy binaries, private traces, license data, raw Dallas EEPROM images, secrets, or other non-public artifacts.
- New durable technical findings must be recorded in the appropriate canonical repository documentation.

## 2. Project goal

Develop a safe, maintainable Windows x64 replacement driver for the LeCroy WR6k acquisition hardware.

Long-term release target:
- normal installation without Windows test-signing mode;
- HLK-ready driver quality;
- eventual WHQL/WHCP-certified Microsoft-signed production driver.

Do not describe certification as already achieved.

## 3. Current verified state

Keep this section limited to the latest verified state. Replace obsolete values instead of appending history.

- Real hardware: LeCroy WR6k PCI device, VEN_1570 / DEV_0005.
- XStream on x64 has demonstrated real waveform acquisition and normal oscilloscope operation for the already-tested acquisition controls.
- Last established real-PCI safe ABI baseline: 9/9 PASS.
- Last established hardware regression milestone: 11/11 PASS.
- A hardened native source implementation of legacy `SetOneRegister` IOCTL `0x0022303C` exists, but its newest source revision has not yet been confirmed here as Windows-built, loaded, or hardware-verified.
- The original 43-entry legacy register list and the query/set record ABI are understood well enough for the current native implementation.
- Dallas WRITE `0x00223088` and serial FPGA/GPIO writer `0xCFDC2130` remain deliberately absent from the native driver.

Detailed and rapidly changing test status belongs in `docs/status.md`, not in this file.

## 4. Durable technical invariants

Only cross-cutting facts that future work must know belong here.

- The legacy register list contains 43 known native entries.
- Legacy register-list query and setter records are not interchangeable:
  - query record DWORD `+0x101` contains the physical register offset;
  - setter request DWORD `+0x101` contains the zero-based register-list index;
  - setter DWORD `+0x106` contains the requested value.
- Native register access must resolve only known BAR/offset mappings and validate lengths, indices, and ranges before touching hardware.
- The original x86 driver's unchecked behaviors are evidence, not implementation requirements. The x64 replacement should harden them.
- Private/vendor binaries may be analyzed locally but must never be committed.

## 5. Safety constraints

These constraints remain until explicitly superseded by better evidence and an owner decision.

- Do not perform arbitrary MMIO/register writes on the working scope.
- Do not use historical or guessed register values for live write tests. If a same-value write is required, read the current value first and write back exactly that value through a validated path.
- Do not issue Dallas EEPROM writes/deletes against the licensed production device.
- Do not expose or commit Dallas/license contents.
- Do not blindly implement or invoke `0xCFDC2130`; it shares GPIO state and is associated with serial FPGA programming.
- Do not inject nonzero synthetic IRQ masks merely for coverage.
- Do not force hazardous original IOCTLs merely to reach numerical coverage targets.
- Prefer static analysis, malformed-input rejection tests, software-only contracts, and reversible hardware tests before invasive experiments.

## 6. Active and deferred work

This section contains only work that is currently relevant across workstreams.

Current project-wide decision:
- Further driver feature work is temporarily paused while repository documentation and context handling are simplified.

Deliberately deferred:
- Dallas WRITE support until disposable/recoverable hardware is available.
- `0xCFDC2130` implementation until GPIO ownership, sequencing, and FPGA-programming semantics are sufficiently understood.
- Final WHQL/WHCP work until functional development and local HLK readiness justify the certification cost.

Topic-specific unfinished work must live in a topic-specific handoff under `docs/handoffs/`, not here.

## 7. Documentation model

Each piece of information should have one canonical home.

- `README.md`
  - Public project introduction only.
  - Explain what the project is, its goals, broad capabilities, build/use entry points, and links to deeper documentation.
  - Do not put LLM/agent instructions, investigation history, detailed test chronology, or internal handoff material in README.

- `AGENTS.md`
  - Compact global operating rules and only the current cross-cutting facts needed by future chats/agents.
  - No historical milestones or completed investigation narratives.

- `docs/status.md`
  - Current verified build, test, hardware, and implementation status.
  - Replace obsolete status rather than accumulating chronology.

- `docs/TODO.md`
  - Open work only.
  - Remove completed items instead of keeping checked-off history.

- `docs/handoffs/<topic>.md`
  - Temporary, topic-specific transfer state for parallel workstreams.
  - There is no global handoff file.
  - Only read a handoff when the owner explicitly references it or the active task clearly requires that exact workstream.
  - Never scan all handoffs automatically.
  - Never append old handoffs as history.
  - Remove a handoff once its state is absorbed or the workstream is complete.

- Canonical topic/reference documentation
  - Holds durable technical detail, evidence summaries, ABIs, architecture, and reverse-engineering conclusions.
  - Handoffs and AGENTS should link to these documents rather than duplicate their contents.

- Git history
  - Is the history.
  - Do not preserve obsolete states in current documentation merely because they were once important.

## 8. AGENTS.md maintenance rules

Treat this file as a bounded current-state document.

- Target size: at most about 150 lines.
- Hard limit: 250 lines. If adding material would exceed this, consolidate or move detail into canonical documentation.
- Remove completed, superseded, or purely historical material once its durable result exists elsewhere or no longer affects future work.
- Do not create dated chronological update blocks.
- Use dates only when the date itself is technically relevant.
- Do not duplicate detailed test results, trace excerpts, disassembly, full source functions, or long register tables here.
- Before adding a fact, ask: does every future workstream need this without opening another document? If not, put it in the relevant canonical document and link it if necessary.
- When a current value changes, replace it. Do not append the previous value.

## 9. Chat workflow and context ownership

Keep the implementation chat alive as long as practical and protect its context from analysis-heavy work.

- The active implementation chat is the single writer for the current coding workstream.
- Its primary responsibilities are implementation, integration, documentation updates, test interpretation, and maintaining continuity of its own changes.
- It should avoid performing deep reverse engineering, broad trace analysis, large binary inspection, or exploratory research when that work can be delegated to a separate analysis chat.
- When analysis is needed, the implementation chat should formulate a narrow task describing:
  1. what must be determined;
  2. why the answer is needed;
  3. relevant files, symbols, traces, or binaries;
  4. safety constraints and things that must not be changed;
  5. the exact result format needed for implementation.
- A separate analysis chat should solve only that task and return a compact result: findings, evidence, affected symbols/files, uncertainties, and recommended implementation implications.
- Analysis chats are read-only by default.
- If a question can only be resolved efficiently by changing or instrumenting code, an analysis chat may use its own clearly named temporary analysis branch.
- Code on an analysis branch is experimental evidence, not the production implementation. It may contain instrumentation, tests, proof-of-concept patches, or alternative implementations needed to validate a hypothesis.
- Analysis chats must not merge their experimental branch into `main` or treat it as automatically merge-ready.
- The active implementation chat receives the compact findings plus, when useful, the analysis branch name and relevant commit SHAs. It decides what code should be reused and performs the final production integration.
- An analysis result involving experimental code should state what the experiment proved, which files/symbols were changed, what may be reusable, what remains uncertain, and why the branch should not be merged blindly.
- Do not duplicate the full analysis transcript into the implementation chat or repository. Transfer conclusions and the minimum supporting evidence.
- Multiple analysis chats may work in parallel on independent questions. Avoid multiple chats editing the same implementation area concurrently except on explicitly isolated analysis branches.

Before modifying a subsystem, read its canonical subsystem documentation if one exists. Prefer one document per subsystem or functional domain, not one document per individual C function.

## 10. Repository access and context rules

Minimize context use without sacrificing correctness.

At the start of a normal chat:
1. Read `AGENTS.md`.
2. Read only the topic-specific handoff explicitly named by the owner, if one is supplied.
3. Read no other documentation automatically.

For repository inspection:
1. Prefer filename, symbol, or exact-text search.
2. Fetch only the relevant line range or section.
3. Fetch an entire large file only when its structure as a whole is genuinely required.
4. Do not reread unchanged large files during the same chat unless necessary.
5. Do not load investigation/reference documents merely because they are linked from another document.
6. For large source files, locate the relevant function/symbol first and inspect only its surrounding range.
7. Avoid copying full source functions, raw traces, dumps, or large disassemblies into documentation when a concise conclusion plus a source reference is sufficient.

Repository structure should be improved for software/documentation clarity, not artificially fragmented solely to reduce LLM context.

## 11. Reference documents

These are reference material, not mandatory startup reading.

- `docs/original-register-list-and-write-abi.md`
  - Canonical reference for the original 43-register list and legacy query/set record ABI.
- `docs/ioctl-map.md`
  - IOCTL inventory and implementation mapping.
- `docs/regression-testing.md`
  - Regression strategy, commands, and test contracts.
- `docs/runtime-trace.md`
  - Historical/runtime trace evidence. Read only when trace evidence is relevant.
- `docs/dallas-license-memory-test-plan.md`
  - Dallas/license safety, analysis, backup, and test planning.
- `docs/driver-signing-and-funding.md`
  - Driver signing, HLK/WHQL/WHCP planning.
