# One-time Codex continuation after an authorized reboot

`scripts/manage-codex-continuation.ps1` provides a fail-closed Windows
PowerShell 5.1 workflow for recording and reopening a specific Codex thread
after an independently authorized reboot. It never replays repository,
hardware, installation, PnP, IOCTL or reboot commands.

## Security and authorization model

- `Prepare` records the absolute worktree, branch, 40-character HEAD, host and
  user identity, task phase, completed steps, consumed authorizations, evidence
  file/tree hashes, next permitted action, fixed launcher and `codex://` URI.
- The manifest and companion hash are created without overwrite and receive an
  ACL for the current user, SYSTEM and Administrators.
- `Validate` rechecks the manifest, script hash, host, user, clean repository,
  HEAD, branch, launcher and every recorded evidence identity.
- `Register` requires an explicit acknowledgement and creates one interactive
  logon task under the intended user with limited run level, one action,
  `IgnoreNew` and a five-minute execution limit. It validates the registered
  action exactly.
- `Resume` uses a named mutex and a non-overwriting result record, validates all
  state again, opens only the pinned thread URI, records that no hardware action
  ran and removes the task. It rejects multiple existing Codex processes.
- `Cleanup` removes the named task idempotently.

The manifest contains no credentials, tokens, signing keys or arbitrary saved
command. Any changed HEAD, dirty/missing worktree, host mismatch, evidence
change, script change, missing launcher or prior resume fails closed.

## Supported limitation

Opening a `codex://threads/...` URI restores the user interface context but
does **not** start a Codex turn. The repository cannot safely inject unattended
user authority into a thread. A separately configured Codex thread heartbeat,
or one manual user message after logon, is required to resume reasoning. This
is the nearest reliable supported alternative to noninteractive continuation.

No Scheduled Task was registered during Batch 10. The 14-case dry suite uses
mock task storage and covers exact registration, validation, consumed-authority
recording, one-shot/partial completion, task cleanup, changed evidence/HEAD,
dirty or missing worktree, corrupt manifest, host mismatch, duplicate Codex
processes and missing executable.

## Operator boundary

Registration and any reboot require their own explicit authorization. Prepare
and validate the manifest before registration; after a failed or obsolete
attempt, run `Cleanup` and retain the result/evidence files for review. Never
reuse a manifest to infer authorization for an operation not listed as the
next permitted action.
