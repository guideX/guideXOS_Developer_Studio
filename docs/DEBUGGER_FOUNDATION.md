# Debugger Foundation — Phase 1

Developer Studio now has a bounded, UI-independent debugger foundation. The
hosted backend is intentionally a supervision backend: it launches the
existing temporary Native ELF development deployment, records its runtime
identity, observes running/exit state, and requests an owned-window stop.
This is Level A — Session supervision only. It is not a full debugger.

## Scope and proven level

The Phase 1 session model supports `Idle`, `Launching`, `Running`, `Paused`,
`Stopping`, `Exited`, and `Failed`, with validated transitions, bounded event
history, and a monotonically advancing session generation. A stale callback or
snapshot from an earlier generation is rejected before it can mutate the new
session.

The hosted Native ELF backend publishes these capabilities:

- Launch: supported through the existing Build Project and hosted Development
  Run path.
- Stop: supported through the owner-bound `development_run_request_close`
  path and subsequent normal polling/cleanup.
- Pause, Continue, Step Into, Step Over, Step Out: unavailable.
- Source and instruction breakpoints: unavailable in the backend; editor
  breakpoints remain `Pending`.
- Registers, memory, threads, call stack, expressions, and source-location
  resolution: unavailable.

The UI keeps ordinary Run on F5. F9 toggles the current editor line because it
was not occupied by an existing Developer Studio shortcut. Debug commands are
available from the Debug menu. Unsupported commands remain visibly unavailable
and do not create fake paused or breakpoint-hit states.

## Runtime capability audit

The existing Run path is:

```text
Developer Studio Build Project
  -> artifact validation and SHA-256 capture
  -> Server owner-bound temporary App Model registration
  -> DesktopService::LaunchDevelopmentApp
  -> Native ELF loader/runtime
  -> process/window polling and normal owned cleanup
```

The Server implementation in `development_run_service.cpp` uses bounded
in-memory deployment slots and generation-bearing handles. It validates the
project, manifest, artifact containment, hash, ELF64 AMD64 `ET_EXEC` shape,
ABI/entry-point requirements, and temporary App Model ownership. The snapshot
already exposes a stable deployment handle, host process-table PID, Native
runtime ID, window counts, exit code, and cleanup state.

The Native ELF execution model is hosted inside the Server runtime's existing
process-table/thread execution architecture. The current implementation does
not provide a debugger-facing process suspension API, resume API, register
context API, target-memory read/write API, trap/exception event stream, or
instruction-pointer correction path. `RequestClose` closes windows owned by
the deployment; it is not arbitrary host-process killing. The process table
does provide status and termination internals, but the Debugger Foundation
uses only the existing owner-bound Development Run close contract.

The ELF loader maps validated `PT_LOAD` segments into the existing Native ELF
runtime image. Ordinary Developer Studio builds remain freestanding C++11,
static `ld.lld`, `x86_64-unknown-elf`, `ET_EXEC`, entry point `gx_main`. Debug
sessions now request a separate `DebugSymbols` build configuration that adds
Clang DWARF line information at `-O0`, with repository-prefix normalization.
The package retains a valid ELF symbol table when the toolchain emits one, but
the existing symbol index still stores names and source locations rather than
instruction addresses.

These facts make source-to-address guessing and INT3 patching unsafe in this
phase. No Server ABI change was required.

## Architecture

The production model is in `src/developer_studio_debugger.*`. It defines:

- `DebugController` and the bounded session state machine;
- `DebugBackend` and `DebugCapabilities`;
- `DebugTarget`, which adapts the existing Project/Build/Run identity without
  duplicating the project configuration;
- `DebugSourceLocation` with project-relative path, line/column, explicit
  address validity, mapping state, and generations;
- `DebugBreakpoint`, with bounded ID, normalized project-relative path, line,
  enabled state, backend binding, session generation, and reason; and
- `DebugEvent`, with bounded event history and generation identity.

`src/developer_studio_debugger_hosted.*` adapts the existing
`HostedDevelopmentRunService`/`RunController` path. It does not bypass App
Model registration or launch a second executor. The deterministic test
backend exists only in `tests/debugger_test.cpp` and is not linked into the
production ELF.

## Breakpoints

Breakpoints are session-only in Phase 1. A canonical project-relative path and
line deduplicate entries; separator direction is normalized, traversal and
absolute paths are rejected, and the active project root is enforced. F9 and
the editor gutter use the same model. F9 toggling disables an existing entry
and re-enables it on the next toggle; the Breakpoints panel also supports
explicit deletion.

The visible states are `Pending`, `Mapped`, `Verified`, `Rejected`, `Disabled`,
and `Stale`. A click never makes a breakpoint Verified. With a debug artifact,
`Mapped` means the source line resolved to linked addresses; it does not mean
the hosted backend installed a runtime breakpoint. Hosted source breakpoints
remain `Pending` when mapping is unavailable. The `Verified` state is reserved
for a backend binding result. A breakpoint becomes Stale when its built project
generation, source document generation, or artifact identity no longer
matches the recorded generation. Unsaved edits remain dirty and are never
silently rebound to an active artifact.

The Breakpoints panel supports selection, source navigation through the
existing `WorkspaceControllerOpenDocumentAtLocation` path, enable/disable,
and deletion. The Debug Session panel shows backend, target, state, PID/runtime
identity when present, architecture, capabilities, and last status/event.

## Debug commands and lifecycle

`Debug -> Start Debugging` validates the active Native GUI project, saves dirty
documents only through the existing explicit build policy, builds through the
existing Build Project service, adapts the successful artifact into a
`DebugTarget`, and starts the hosted backend. The state sequence proven by the
model is `Idle -> Launching -> Running -> Exited`; a launch/build failure is
reported as `Failed`.

`Stop Debugging` requests the existing owner-bound close operation and waits
for the backend to report normal exit/cleanup. `Pause` and `Continue` are
disabled because the backend advertises neither capability. Step commands are
deferred and are not advertised as implemented.

Ordinary Run remains a separate command and continues to use the existing
Run controller. Closing a project or Developer Studio while a debug session
is active is blocked behind the bounded Stop Debugging confirmation. Closing a
document does not erase breakpoint storage. A project/source-generation
mismatch is surfaced as Stale rather than silently rebound.

## Phase 2 source mapping and next milestone

Phase 2 implements bounded DWARF/source-line mapping in
`src/developer_studio_debug_symbols.*`. It reads the exact hashed ELF artifact
through the existing filesystem API, supports the DWARF 4/5 line-table forms
emitted by the current Clang build, normalizes paths under the project root,
and exposes forward and reverse line/address lookup. The detailed contract,
limits, error states, and validation evidence are in
`docs/DEBUGGER_SOURCE_MAPPING.md`.

This evidence does not change the Phase 1 runtime capability level: the
hosted backend still supports launch and owner-bound stop only. A later
software-breakpoint/trap milestone must first add and validate a Server runtime
debugger ABI. Pause/register support cannot be claimed from the current
runtime audit.

Deferred work includes instruction breakpoints, trap delivery, Continue after a
real stop, stepping, DWARF expressions, locals, stack unwinding, watches,
memory/register editing, conditional/data breakpoints, attach/remote/multiple
process support, GDB/LLDB/DAP integration, managed/.NET debugging, and
bare-metal trap implementation.
