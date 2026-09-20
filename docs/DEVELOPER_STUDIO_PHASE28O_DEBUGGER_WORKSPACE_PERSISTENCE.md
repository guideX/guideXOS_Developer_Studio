# Developer Studio Phase 28O — Debugger Workspace Persistence

Status: Outcome A validation closure complete. Phase 28P has not started.

## Provenance

Standalone repository: `D:\dev\guideXOS_Developer_Studio`

- Branch: `phase28o-debugger-workspace-persistence`
- Starting implementation commit: `f5a90d9fdc5a28e720e7a19df35b2c9d066edf15` — `developer studio: persist debugger workspace`
- Validation closure commit: `e39c371675c60fed14df594248342def40c9c9b6` — `developer studio: stabilize debugger workspace validation`
- `main` remains `33c37e56df6dd70e0963b2caca824e100f5e3d7e`; it was not changed.

Server repository: `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO`

- Branch: `v0.5_DEVELOPER_STUDIO`
- Starting implementation commit: `7ac5a0a09a6c7aa593ed4ee534a46b1a03cfa8ae` — `developer studio: persist debugger workspace`
- Validation/package commit: `337d38800e3455618a5cb920a0cc7974970a4e45` — `developer studio: validate debugger workspace persistence`
- Upstream: `origin/v0.5_DEVELOPER_STUDIO`; final divergence is ahead 1, behind 0.
- Nothing was fetched, merged, rebased, amended, reset, rewritten, or pushed.

## Scope and storage contract

Phase 28O persists project-scoped source breakpoint policies and Watch expression
order across a real Developer Studio close/relaunch cycle. It rematerializes
manager-owned runtime state for every Debug generation.

Primary storage is `<project-root>/guidexos.debugger.json`; recovery storage is
`<project-root>/guidexos.debugger.json.bak`. Schema version is 1, with bounded
capacity of 8 breakpoints and 8 Watches. Runtime IDs, addresses, bytes,
installed state, generation/session identity, raw hit counts, results, selected
frame, execution markers, Locals/Arguments, Output, and stepping state are not
persisted.

The save path serializes the complete bounded workspace, writes the backup first
and then the primary, and reports short-write/callback failures. Load validates
the project identity and root-scoped path, accepts the primary or valid backup,
and never imports partial runtime state. Bare-metal FAT long filenames are
covered, including the JSON file and `.bak` companion.

## Previous incomplete-QEMU boundary

The first focused continuation boot preserved at
`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-caa2964ac28b44baac4efc249b0f4099`
reached:

```text
DEVELOPER_STUDIO_PHASE28O_SAVE_PASS
DEVELOPER_STUDIO_PHASE28O_CLOSE_PASS
DEVELOPER_STUDIO_PHASE28O_RELAUNCH_PASS
```

The exact first missing marker was:

```text
DEVELOPER_STUDIO_PHASE28O_RESTORE_PASS
```

At that boundary the first Developer Studio instance had deliberately closed,
the relaunch had begun, and no Debug session was active. The harness had selected
the restore stage from a racy one-shot persistence-file check, so it could wait
for a relaunch that had already entered another lifecycle state. Later evidence
reached restore, materialization, breakpoint hit, Watch reevaluation, and
breakpoint-state markers, then exposed additional sequencing boundaries in the
old Phase 28M logpoint/step flow. One later run stopped at
`DEVELOPER_STUDIO_PHASE28M_SYMBOL_ERROR_MALFORMED_DWARF`; that was a fixture
debug-info artifact before mapper completion, not a persistence defect.

## Root cause and fix

Classification: harness/diagnostic automation sequencing plus timing; not a
production persistence or debugger state-machine defect.

No persistence architecture was reimplemented. The explicit Phase 28O
diagnostic flow was made deterministic by:

- waiting on the actual persistence-file predicate across event-loop turns;
- keeping the diagnostic relaunch on the same application lifecycle stack
  instead of recursively calling `gx_main`;
- latching Phase 28O diagnostic mode through relaunch and invalidating only the
  Watch runtime view at the close/reopen boundary;
- skipping the unrelated legacy Phase 28M step-navigation gate once Phase 28O
  has completed its pane assertions;
- accepting the restored LOG template only after runtime output is observed and
  the target remains owned by the real breakpoint path rather than the LOG row;
- polling the diagnostic state machine at a bounded 50 ms event-loop cadence;
- emitting state-specific timeout evidence with project, Debug state, session
  generation, workspace/runtime counts, output count, and selected frame; and
- using the real Developer Studio Stop/controller path, then waiting for stale
  state clear, terminal state, second-generation materialization, and cleanup
  before emitting `DEVELOPER_STUDIO_PHASE28O_PASS`.

These changes are guarded by the Phase 28O diagnostic sentinel. Normal
Developer Studio persistence semantics and normal UX were not changed.

## Final package identity and audit

The same final AMD64 package was used for all three final boots:

| Package | Size | SHA-256 |
|---|---:|---|
| AMD64 `Apps/DeveloperStudio/bin/amd64/developerstudio.elf` | 907672 | `F8B230A7E06F0CE4A488209F89042D13F8579CC0BFA7660C7D81FCDB8A38C1BA` |
| ARM64 `Apps/DeveloperStudio/bin/arm64/developerstudio.elf` | 1067808 | `8008D9A85C7645D73E0664A1977CBA72CA97C930AD18A3D358D40D88397F4455` |

The AMD64 package is ELF64 little-endian AMD64 `ET_EXEC`, entry
`0x500a6510`; the ARM64 package is ELF64 little-endian ARM64 `ET_EXEC`, entry
`0x500c4318`. Both have no section metadata and no debug sections. The package
contains exactly `app.json` and the two architecture binaries.

`tests/validate-developer-studio-package.ps1` passed with
`package_content_audit=PASS`. The canonical ESP package copy was aligned to the
same hashes and the Phase 28O sentinel; the obsolete Phase 28M sentinel was
removed.

## Three fresh QEMU boots

Final evidence root:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-92907319c2ae4e7db4c3acbce1eb6c89`

The runner command was:

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-compiler-bootstrap.ps1 -Phase28OOnly -BootCount 3 -TimeoutSeconds 600
```

All three boots were fresh and isolated, required no manual interaction, and
used the same AMD64 package hash above. Each boot had zero missing required
markers.

- Boot 1: `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-92907319c2ae4e7db4c3acbce1eb6c89\esp-boot1`, serial `...\boot1.serial.log` — PASS.
- Boot 2: `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-92907319c2ae4e7db4c3acbce1eb6c89\esp-boot2`, serial `...\boot2.serial.log` — PASS.
- Boot 3: `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-92907319c2ae4e7db4c3acbce1eb6c89\esp-boot3`, serial `...\boot3.serial.log` — PASS.

The exact final O lifecycle sequence, with the established M/N markers between
these phases, was:

```text
DEVELOPER_STUDIO_PHASE28O_SAVE_PASS
DEVELOPER_STUDIO_PHASE28O_CLOSE_PASS
DEVELOPER_STUDIO_PHASE28O_RELAUNCH_PASS
DEVELOPER_STUDIO_PHASE28O_RESTORE_PASS
DEVELOPER_STUDIO_PHASE28O_NO_RUNTIME_ID_PASS
DEVELOPER_STUDIO_PHASE28O_DEBUG_START_PASS
DEVELOPER_STUDIO_PHASE28O_MATERIALIZE_PASS
DEVELOPER_STUDIO_PHASE28O_HIT_COUNT_ZERO_PASS
DEVELOPER_STUDIO_PHASE28O_UNRESOLVED_PASS
DEVELOPER_STUDIO_PHASE28O_FRESH_ID_PASS
DEVELOPER_STUDIO_PHASE28O_BREAKPOINT_HIT_PASS
DEVELOPER_STUDIO_PHASE28O_WATCH_REEVALUATE_PASS
DEVELOPER_STUDIO_PHASE28O_LOG_OUTPUT_PASS
DEVELOPER_STUDIO_PHASE28O_RESET_PASS
DEVELOPER_STUDIO_PHASE28O_TERMINAL_PASS
DEVELOPER_STUDIO_PHASE28O_SECOND_GENERATION_PASS
DEVELOPER_STUDIO_PHASE28O_CLEANUP_PASS
DEVELOPER_STUDIO_PHASE28O_PASS
```

The guest then emitted the existing M/N application and cleanup PASS markers.
The terminal sequence is emitted only after the real Stop/controller path has
cleared the session: Debug state is Exited, the controller is inactive, the UI
session generation is zero, and call-stack/variables/output authority is clear.

## Runtime identity, policy, and Watch proof

The existing in-guest `DEVELOPER_STUDIO_PHASE28K_ADD_PASS` records provide the
numeric runtime identity and address proof:

```text
Generation A: id=0000000100000001 address=0x0000000050001014
Generation B: id=0000000200000001 address=0x0000000050001014
```

Therefore runtime ID A differs from runtime ID B while the machine address is
allowed to remain identical. The persisted workspace contains the source-level
specification, not either runtime identity or address. The O no-runtime-ID
marker proves the restored workspace starts without runtime authority; the
materialization and fresh-ID markers prove live manager rematerialization.

The five configured rows, breakpoint policy, condition, hit threshold, disabled
state, LOG template, and two Watch strings restore correctly. The first Debug
pause produces a real breakpoint hit and reevaluates the restored Watches. The
raw hit-count-zero marker is emitted for both fresh generations before execution
continues. The LOG row emits runtime output and does not claim the user pause.
The selected frame, execution marker, and Output authority are regenerated and
are not restored as persisted state.

## Host and contract validation

- CTest: 30/30 focused tests passed, including workspace persistence, debugger,
  source stepping, stack, variables, structured variables, Watches, conditional
  breakpoints, editor, ABI/model coverage, and related editor models.
- Normal package-build host tests passed for AMD64 and ARM64, including run,
  project/search, completion/signature/include graph/relationships/ownership/
  types, debugger, source step, stack, variables, Watches, conditional policy,
  and debugger editor tests.
- Corrected filesystem contract runner passed:
  `scripts/run-native-filesystem-contract-test.ps1`; it includes
  `kernel/core/native_elf/native_elf_debug_watches.cpp`.
- PowerShell parser checks passed for the Phase 28O required-marker runner, the
  server QEMU runner, and the focused validation runner.
- `git diff --check` passed for committed source, scripts, and package changes.
- The broader representative hosted debugger smoke was also attempted after
  CTest; it failed before its initial render gate with server exit code
  `-1073741819` and zero lifecycle markers. That is an unrelated hosted-server
  access-violation boundary before Phase 28O and is retained as a limitation,
  not used as evidence against the successful guest Phase 28O proof.

## Phase 28L and preserved evidence

The current Phase 28L rerun passed with evidence at:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-bc9e59b69101447cb23fafadfd0096b4`

The earlier Phase 28L failure was isolated to fixture compilation before
Developer Studio launch. The current rerun reached the policy, FIFO output,
hit-count, coexistence, GUI completion, cleanup, generation reset, second-run,
and `DEVELOPER_STUDIO_PHASE28L_PASS` markers. It is not an active blocker.

All earlier Phase 28O QEMU evidence directories were preserved. The standalone
untracked build/evidence directories were preserved; no cleanup was performed.

## Outcome

Outcome A is justified. Developer Studio remembers source-level breakpoint
policies and Watch expressions across close/reopen, rematerializes fresh
debugger state for every Debug generation, and proves that behavior
deterministically across three clean real guideXOS boots.

No physical mouse was used. QEMU detected its emulated PS/2 mouse, but no manual
mouse or keyboard interaction was required. The guest diagnostic and real app
controller owned the lifecycle; the host only launched, observed, and asserted
markers. Standalone `main` remains unchanged, all evidence directories remain
preserved, and nothing was pushed.
