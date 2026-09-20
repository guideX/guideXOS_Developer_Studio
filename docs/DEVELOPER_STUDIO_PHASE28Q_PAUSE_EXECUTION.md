# Developer Studio Phase 28Q — Pause Execution

Phase 28Q adds a real Pause / Break operation for the one active native
Developer Studio target. Pause is a bounded debugger request; it does not
cancel the run, install a guessed source breakpoint, or fabricate a paused UI
state.

## Architecture audited

The Phase 28P server already has one cooperative AMD64 execution owner. The
target runs on the NativeElf scheduler stack and returns to the owner through
`native_elf_scheduler_yield()` from the existing native-window event loop.
There is no timer-preemptive debugger boundary in this path, and the existing
timer hook does not provide a safe arbitrary-context capture. Phase 28Q uses
that real scheduler yield as its capture boundary.

At the boundary, an AMD64 assembly shim saves all target GPRs, the target RIP,
CS, RFLAGS, and the logical post-yield RSP into the existing
`NativeElfDebugTrap::BreakpointContext`. The owner then publishes the normal
debug snapshot while the target remains suspended in the scheduler switch.

## Request and state machine

The debugger ABI adds the append-only command
`GX_DEVELOPMENT_DEBUG_PAUSE = 29`, status
`GX_DEVELOPMENT_DEBUG_STATUS_PAUSE_REQUESTED = 7`, and reason
`GX_DEVELOPMENT_DEBUG_PAUSE_REASON_USER_PAUSE = 8`.

The request is authenticated against the active handle, session generation,
runtime generation, optional process identity, artifact hash, and thread
identity. The request is consumed only by the matching active NativeElf
operation. Duplicate requests return a bounded `PAUSE_REQUESTED` response;
requesting Pause after capture returns the existing user-pause snapshot. A
request during terminal execution is rejected. Active Step / Step Over / Step
Out ownership is not replaced by a competing Pause request.

The transitions are:

```text
Running --Pause--> PauseRequested --next safe yield--> Paused(USER_PAUSE)
Paused(USER_PAUSE) --Continue--> Running
Paused(USER_PAUSE) --Cancel--> existing cancellation lifecycle
```

Continue clears only the manual-pause stop state and pumps the same saved
target context back through the existing scheduler. Persistent source
breakpoint patches remain owned by the breakpoint manager. Stop / Cancel still
uses the existing teardown path and remains distinct from Pause.

## Paused context and inspection

The manual stop publishes the normal register context, stop generation, stack
bounds, instruction pointer, and optional source mapping. It also publishes
the persistent breakpoint table without assigning a current breakpoint owner.
Call Stack, variable inspection, expression evaluation, target-memory reads,
and the existing controller data-tip path accept the captured context using
the same stop identity rules as other debugger pauses. An unmapped RIP remains
unmapped instead of being presented as user source.

## Developer Studio UI

The hosted backend exposes `canPause` and sends the new command through the
existing NativeElf debug bridge. The integrated debugger menu and Session pane
show `Pause (F6)` only while the controller is Running and no request is
pending. F6 invokes the real controller pause handler. Continue is enabled for
`PausedAtUserPause`; a second Pause is rejected as `AlreadyPaused`, and a
rapid duplicate request is rejected as `PauseAlreadyRequested`.

The controller clears its pending request on the authenticated user-pause
snapshot and clears the stopped context on Continue, so execution markers and
runtime-only data-tip state follow the existing paused/resumed behavior.

## ABI and compatibility

No legacy request or snapshot fields were changed. The existing sizes remain:

| Item | Size / prefix |
| --- | ---: |
| Legacy debugger request prefix | 104 bytes |
| Current debugger request | 160 bytes |
| Debugger snapshot | 27,384 bytes |
| Register context | 192 bytes |
| Compiler object ABI | 10 (unchanged) |

Only enum values were appended. GXSM, the NativeElf executable format, and the
existing host-call table are unchanged.

## Validation

The standalone model build and all 31 CTest model tests pass, including the
Phase 28Q controller test for Running → pending Pause, duplicate Pause,
USER_PAUSE capture, and Continue. The changed server NativeElf run service and
loader translation units also compile with the existing AMD64 freestanding
flags. A full server link is currently blocked by the checkout's pre-existing
missing `third_party/mbedtls` make dependency.

The Phase 28Q QEMU harness was invoked, but the required kernel build stopped
before boot on that same missing dependency. Therefore no QEMU pause result is
claimed. Once the server dependency is restored, the harness should verify
three fresh isolated boots, two Pause/Continue cycles, inspection, breakpoint
coexistence, deterministic final output, cleanup, and identical package
hashes.

The unrelated hosted smoke access violation `-1073741819` remains a separate
stabilization item and is not used as Phase 28Q evidence.

## Repository status

The standalone work is on `phase28q-debugger-pause`, branched from the final
Phase 28P commit. The server work remains local on
`v0.5_DEVELOPER_STUDIO`. No merge, push, reset, clean, amend, or history
rewrite is part of this phase.
