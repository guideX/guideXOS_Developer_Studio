# Debugger Foundation — Phase 7

## Phase 5 status

Hosted Native ELF software-breakpoint plumbing is implemented for the current AMD64
fixed-address runtime, including debug-controlled launch gating, exact
process/runtime/thread identity, bounded `INT3` binding, real breakpoint stops,
copied register context, and correct breakpoint continuation through a real
processor single-step. The Phase 4 runtime proof established Level B + Continue
for this bounded source-breakpoint path; Phase 5 adds Level B + Step Into. This
is not a claim of general-purpose debugger support. Phase 6 adds Level B Step
Over through real call-aware execution and temporary return breakpoints. Phase
7 adds a bounded AMD64 frame-pointer call stack with target-owned stack reads;
it does not add CFI unwinding; Phase 8 adds the separate Step Out operation.

Developer Studio now has a bounded, UI-independent debugger foundation. The
hosted backend launches the existing temporary Native ELF development
deployment, records its runtime identity, observes running/exit state, requests
an owned-window stop, and performs the separately bounded AMD64 breakpoint
continuation path. The supervision-only subset is Level A. Phase 3B adds a
separately bounded Level B path for verified software breakpoints and owned
breakpoint pauses; Phase 4 adds Level B + Continue; Phase 5 adds the separate
DWARF/TF source-step operation.

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
- Pause: unavailable.
- Continue: supported from a real owned paused software-breakpoint stop and a
  completed source-step stop.
- Step Into: supported through real AMD64 TF single stepping and DWARF source
  mapping.
- Step Over: supported through bounded AMD64 call decoding, real call execution,
  and an internal return `INT3` owner.
- Step Out: supported through the current frame's validated raw caller return
  address and a separate internal return `INT3` owner.
- Call Stack: supported from a genuinely paused AMD64 stop through bounded RBP
  links, ELF function symbols, and DWARF source enrichment. The result is
  partial when the frame chain is unsafe or unavailable.
- Source and instruction breakpoints: source breakpoints are supported through
  DWARF mapping and the hosted software-breakpoint binder; instruction-level
  editing remains outside the UI contract.
- Registers are captured in a bounded read-only stopped context and the session
  panel shows `RIP`, `RSP`, `RBP`, and `RFLAGS` while paused. Memory, thread
  enumeration, expressions, and register editing remain unavailable.

The UI keeps ordinary Run on F5. F9 toggles the current editor line because it
was not occupied by an existing Developer Studio shortcut. Debug commands are
available from the Debug menu. Unsupported commands remain visibly unavailable
and do not create fake paused or breakpoint-hit states.

Phase 5 adds `F11` Step Into. It is a bounded source-step operation driven by
real processor TF single-step events and the existing DWARF mapper; it is not a
line-number or editor-caret simulation. See [DEBUGGER_STEPPING.md](DEBUGGER_STEPPING.md).

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

These facts made source-to-address guessing and INT3 patching unsafe in the
original foundation. Phase 2 source mapping did not require an ABI change;
Phase 4 adds only append-only debug request/snapshot fields for the bounded
AMD64 register context and continuation command, preserving the existing
numeric identities and offsets.

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
for the backend to report normal exit/cleanup. `Continue` is enabled only when
the exact stopped context, process/runtime/thread, session generation, stop
generation, and physical binding are valid. F5 continues such a stop; outside
debugging it retains ordinary Run behavior. Pause and all user-visible Step
commands remain disabled.

Ordinary Run remains a separate command and continues to use the existing
Run controller. Closing a project or Developer Studio while a debug session
is active uses the targeted owned-window close event; the hosted app requests
debug stop directly and completes teardown before releasing its window.
Keyboard/modal confirmation remains available for non-targeted application
close paths. Closing a document does not erase breakpoint storage. A
project/source-generation mismatch is surfaced as Stale rather than silently
rebound.

## Phase 19 deterministic hosted shutdown

The hosted close contract is focus-independent. `gui.close <windowId>` targets
Developer Studio's canonical owned window and enters the normal app lifecycle;
it does not kill the process or route a confirmation key through whichever
window has focus. The authoritative bounded stages are:

`debug_shutdown_request` -> `debug_stop=requested` ->
`debug_target_teardown` -> `debug_session_teardown` ->
`debug_window_release` -> `debug_shutdown_complete`.

The guaranteed partial ordering is request before stop, stop before target and
session teardown, teardown before window release, and window release before
shutdown complete. The Server records a monotonic `shutdownStage` and
`shutdownStageCode` in `nativeapp.processes`; the final release and complete
markers are emitted by `NativeAppRuntime::Cleanup` after owned windows are
released. `nativeapp.debuglog [count]` exposes at most 64 recent entries from
the existing bounded lifecycle ring.

The hosted debugger smoke uses `WAITSHUTDOWN <seconds>` to poll live Server
output and the durable process-table stage. It reports the expected and
actual stage, debugger/session/target identity, ownership state, and recent
markers on timeout. Successful runs print concise PASS lines. A failed run
writes the bounded `logs/developer-studio-debugger-shutdown-trace[-N].log`
artifact containing Phase 18 session, stop, step, binding, transition, and
shutdown diagnostics. Production runs do not enable or print this trace.

## Phase 2 source mapping and next milestone

Phase 2 implements bounded DWARF/source-line mapping in
`src/developer_studio_debug_symbols.*`. It reads the exact hashed ELF artifact
through the existing filesystem API, supports the DWARF 4/5 line-table forms
emitted by the current Clang build, normalizes paths under the project root,
and exposes forward and reverse line/address lookup. The detailed contract,
limits, error states, and validation evidence are in
`docs/DEBUGGER_SOURCE_MAPPING.md`.

Phase 3 extends this foundation with the separately documented hosted software
breakpoint and trap path. Phase 4 adds the separately documented restore-RIP-set-TF
single-step continuation path without exposing that internal primitive as user
stepping.

## Phase 3B end-to-end proof

The authoritative UI smoke uses the checked-in fixture at
`tests/fixtures/debugger-phase3b`. Developer Studio opens the fixture through its
real project dialog, moves the editor caret to `src/main.cpp:20`, arms F9, builds
with debug symbols, starts Ctrl+F5, and consumes the hosted trap. The fixture's
line 20 maps to `0x20001218` in the fixed `0x20000000` image.

The proof recorded the exact deployment handle, guideXOS process ID, Native App
runtime ID, and closed execution gate before binding. The Server validated the
artifact identity, installed `0xCC` over original byte `0xC7`, released the gate,
and delivered a real `EXCEPTION_BREAKPOINT`. Developer Studio accepted only the
owned binding, reverse-mapped the stop, opened the existing source document, and
published both the paused-breakpoint and editor execution-marker results.

The same smoke requests Stop Debugging through the product close confirmation,
restores the binding, exits the target, and closes Developer Studio cleanly. The
Server's active host callback context is thread-local so the Studio and debugged
Native ELF workers can remain live concurrently.

The Phase 4 UI variant of this smoke adds `-ContinueBreakpoint`: after the
paused marker it sends F5 and requires the hosted `Running` marker together
with the Server's real `EXCEPTION_SINGLE_STEP` and `rebound=true` diagnostics.
The direct runtime harness remains the authoritative repeated-hit and exact
instruction-semantics proof; the UI variant uses the checked-in fixture's
window loop so the post-Continue Running state can be observed without
depending on target exit timing.

The Phase 5 UI variant adds `-StepInto`: after the paused marker it sends F11
and requires `STEPPING`, `PAUSED_STEP`, the hosted user source-step acceptance,
and a real `EXCEPTION_SINGLE_STEP` diagnostic.

Phase 6 adds F10. A recognized `E8 rel32` or `FF /2` call receives a temporary
internal return breakpoint and executes the callee normally; the next source
location is selected from the real return trap and DWARF reverse mapping. A
non-call instruction falls back to the existing real source-step engine. The
operation is bounded and cleans up its temporary logical owner on completion,
interruption, stale identity, failure, process exit, and teardown. See
[DEBUGGER_STEP_OVER.md](DEBUGGER_STEP_OVER.md).

Phase 7 adds the bounded Call Stack tab and frame-pointer unwinder described in
[DEBUGGER_CALL_STACK.md](DEBUGGER_CALL_STACK.md). It reads only the stopped
target stack through the append-only debug boundary and clears the result on
resume or stale identity.

Deferred work includes additional instruction-breakpoint workflows, general
DWARF expressions beyond the bounded Watch grammar, CFI/`eh_frame` unwinding,
memory/register editing, conditional/data breakpoints, attach/remote/multiple
process support, GDB/LLDB/DAP integration, managed/.NET debugging, and
bare-metal trap implementation.

## Phase 8 Step Out

Phase 8 adds real Step Out / Shift+F11 for the bounded hosted AMD64 path. It
uses the current physical frame, validates the caller return address from the
fresh Call Stack, arms a temporary internal return breakpoint, and waits for
the real return-address `EXCEPTION_BREAKPOINT`. See
[DEBUGGER_STEP_OUT.md](DEBUGGER_STEP_OUT.md). Pause, locals, arguments,
general expression evaluation beyond Phase 11, DWARF CFI unwinding, and optimized-code full debugging remain
unsupported. Phase 11 adds the separate bounded Watch expression path; it does
not change the structured Locals/Arguments contract.
## Phase 9 locals and arguments

The debugger now has a bounded read-only DWARF variable index for simple Native ELF debug builds. Locals and formal arguments are evaluated from the exact stopped register/frame-base/target-memory state. See [DEBUGGER_LOCALS_ARGUMENTS.md](DEBUGGER_LOCALS_ARGUMENTS.md). Phase 11 adds bounded Watch expressions on top of this same value model; arbitrary C++ expression evaluation and full optimized-variable debugging remain outside the contract.

Phase 10 adds bounded lazy structured-value nodes for DWARF-backed members,
arrays, nested aggregates, and explicit pointer expansion. See
[DEBUGGER_STRUCTURED_VARIABLES.md](DEBUGGER_STRUCTURED_VARIABLES.md). Hosted
structured UI proof remains distinct from model/runtime proof.
