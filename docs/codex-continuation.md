# One-time Codex continuation after an authorized reboot

`scripts/manage-codex-continuation.ps1` provides a fail-closed Windows
PowerShell 5.1 workflow for resuming one exact Codex session after a separately
authorized reboot. It uses the supported noninteractive CLI surface:

```text
codex exec --sandbox workspace-write --cd <worktree> --json \
  --output-last-message <new-file> resume --strict-config <exact-uuid> -
```

The fixed continuation prompt is read from standard input. `--last`, UI URI
launching, undocumented app-server protocols, history-database edits and
dangerous sandbox bypasses are forbidden.

## Recorded identity and authorization

`Prepare` creates schema-2 manifest and hash files without overwrite. It pins:

- absolute worktree, clean branch and 40-character HEAD;
- host, user and hashed evidence identities;
- exact session UUID, CLI path, SHA-256, version and valid OpenAI signature;
- a bounded prompt file and its SHA-256;
- workspace-write sandbox, timeout, completed work and consumed authorizations;
- the next permitted action, which cannot grant broader authority.

The CLI path is application-versioned. An update changes its identity and
therefore requires a new manifest. The manifest contains no credentials,
tokens, API keys, signing keys or private material.

## One-shot execution

`Register` requires explicit acknowledgement and creates a limited,
interactive-logon Scheduled Task. `Resume` revalidates all state, takes a named
mutex, exclusively creates `attempt-started.json`, then disables the task
before starting the CLI. Existing Codex Desktop app-server and exec-server
processes are normal and are not used as a single-instance predicate.

The child is bounded and synchronous. Its JSONL stdout, stderr and final
message are separate, non-overwriting files. A successful exit produces a
hashed result record and removes the task. Failure, timeout or early exception
retains the disabled task and available evidence; another logon cannot retry.
Only the recorded child may be terminated on timeout.

No real task was registered and no real session was resumed in Batch 11. The
17-case mock suite covers exact UUID selection, absence of `--last`, clean
state validation, task registration and cleanup, executable/prompt/evidence
drift, one-shot success, nonzero child exit, timeout and persistent failure
evidence. A live reboot/resume test requires separate authorization.

## Operator boundary

Preparation does not authorize task registration or reboot. Registration and
the reboot each require explicit owner authorization. The continuation prompt
may resume only already authorized work; it cannot authorize signing,
installation, PnP, hardware access or any other new operation.
