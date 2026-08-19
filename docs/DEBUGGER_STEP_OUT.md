# Debugger Phase 8 — Step Out

Step Out is the hosted AMD64 command `Shift+F11`. It executes the current
physical function until that invocation returns to its immediate caller:

```text
Paused
  -> fresh frame #0 and caller frame #1
  -> validated raw saved return address
  -> temporary InternalStepOutBreakpoint
  -> normal target execution
  -> RET
  -> real EXCEPTION_BREAKPOINT at the caller return address
  -> temporary owner removal
  -> caller source mapping and fresh Call Stack
  -> Paused / Step
```

Step Out never selects a Call Stack row as the execution context. A selected
caller frame is an inspection context only. The operation always uses frame #0
from the exact current session, process, thread, artifact, and stop generation.

## Phase 9 variable invalidation

Step Out invalidates Locals/Arguments until the real caller stop is published. The execution marker remains tied to frame #0 while a selected caller frame is inspected.
Structured values follow the same invalidation boundary and are never carried across the real return-address stop.

## Return-address policy

The frame-pointer unwinder exposes both values:

```text
rawReturnAddress = address where RET resumes execution
lookupAddress   = rawReturnAddress - 1
```

The temporary INT3 is installed at `rawReturnAddress`. The adjusted lookup
address is used only for caller function/source attribution, matching the
existing Phase 7 policy. No source-line arithmetic is used.

Before binding, the raw address must be non-zero, canonical AMD64, inside an
executable target segment, covered by the current artifact identity, and
readable through the target-memory contract. A caller outside the Native ELF
image, including the Windows host/runtime boundary, disables Step Out.

## Breakpoint ownership and interruption

Step Out uses the existing physical breakpoint manager. Its logical owner is a
separate high-range `InternalStepOutBreakpoint` ID. A user breakpoint and the
temporary owner can share one physical `0xCC`; removal restores the original
byte only after the last owner is gone. If a persistent user breakpoint owns
the exact return address, user intent wins and the resulting stop is
`Paused / Breakpoint`.

A user breakpoint hit inside the current function or a nested callee cancels
Step Out, removes only its temporary return owner, and leaves the user
breakpoint active. Process exit and Stop Debugging clear the operation; target
memory is not touched after the image is gone. A stale stop, wrong thread, or
invalid return trap is rejected with a bounded error.

## State and limits

The `DebugStepOutOperation` stores session, stop, process, runtime, thread,
starting register/source context, raw and lookup return addresses, the
temporary binding identity, and completion status. After completion the
register snapshot is the real stopped context with TF clear, the execution
marker is at the caller return position, and the Call Stack is rebuilt with a
new stop generation and frame #0 selected.

Step Out is supported for valid frame-pointer debug builds, including leaf,
recursive, and multiple-return functions when the saved caller address is
valid. It does not add Pause, locals, arguments, general expression
evaluation beyond the bounded Phase 11 Watch grammar, DWARF CFI unwinding, optimized-code full debugging, managed
debugging, or multi-thread execution control.

The Phase 8 fixture is `tests/fixtures/debugger-phase8`. Its primary proof is
`level3 -> level2 -> level1 -> gx_main`; repeated Shift+F11 moves outward one
physical frame at a time.
Step Out invalidates Locals/Arguments until the real caller stop is published. The execution marker remains tied to frame #0 while a selected caller frame is inspected.

## Hosted stop cleanup

The controller is the authoritative owner of the logical Step Out state. The
hosted target may keep the physical single-step trap suspended while the
controller has already published the caller stop, so a later poll can observe
the same internal trap again. When the controller is already paused at
`PausedAtStepOut`, that observation is idempotent and does not publish a second
stop or remove the temporary owner twice. Continue from `PausedAtStepOut` uses
the existing hosted resume path, which releases the held trap and clears the
step-stop context.

The transition validator and its error reporting remain unchanged. Inspection
refreshes (Call Stack, Locals, Arguments, and Watch) only rebuild stopped
context; they do not mutate execution state. A persistent user breakpoint at
the return address remains independently owned when the temporary Step Out
owner is removed.

## Phase 18 lifecycle observability

The hosted diagnostic log emits compact lifecycle markers only while a debug
session is active. The markers use the existing host-log path and do not poll
target memory or add a production output entry:

* `debug_session` records session generation, process/runtime/thread identity,
  state, stop reason, stop generation, selected frame, and source location.
* `debug_step` records the last operation (`StepInto`, `StepOver`, or
  `StepOut`), its stop-generation binding, active flag, Step Out raw return and
  lookup addresses, completion generation, temporary-owner state, and bounded
  cleanup count.
* `debug_binding` records the physical binding ID/address, logical owner count,
  user/internal owner counts, shared status, and installed state. An overlap
  therefore has one physical ID, two logical owners during Step Out, and one
  user owner after cleanup.
* `debug_transition` records the authoritative state transition sequence and
  the last rejected transition/status. Strict transition validation remains in
  force; duplicate observation is idempotent only for the already-published
  Step Out trap.

At an overlapping return-address stop, the internal Step Out completion is
recorded internally but the user-visible reason is `Breakpoint`. Continue then
uses the surviving user breakpoint owner, restores and re-inserts the same
physical trap, and later hits retain the user's logical breakpoint identity.
Continue is also valid from source-step, Step Over, and Step Out completion
stops; each operation's temporary owner is released before the next operation
is accepted.
