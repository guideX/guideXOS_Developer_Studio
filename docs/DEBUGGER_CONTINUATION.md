# Debugger Phase 4 — Breakpoint Continuation

Phase 4 implements real continuation from a hosted AMD64 software breakpoint.
The supported target remains the Windows-hosted, fixed-address ELF64 `ET_EXEC`
runtime used by Developer Studio.

The Server ABI extension is append-only. Existing command/status values and
request/snapshot offsets remain unchanged; Phase 4 appends the Continue command,
pending status, thread/stop-generation identity, register context, and
RFLAGS audit fields. The ABI layout test covers the resulting fixed sizes.

## Phase 9 variable invalidation

Continue invalidates the stopped Locals/Arguments generation. Values are rebuilt only after the next genuine paused stop.

## Phase 5 source stepping and Phase 6 Step Over

Phase 5 adds a separate user source-step command and pending mode. The private
breakpoint continuation step remains internal and is never reported as F11
completion. F11 uses real processor TF single stepping, DWARF-based same-line
suppression, and returns to `Paused / Step`; F5 can resume the stopped
source-step context. Phase 6 adds F10 using a separate bounded operation: it
decodes the current AMD64 instruction, binds a temporary logical owner at a
real call return address, executes the call without TF, and consumes the
matching internal return trap. Non-call instructions use the existing
 source-step engine. Phase 8 adds Step Out as a separate return-address operation.

## Stop context ownership

At a real `EXCEPTION_BREAKPOINT`, the Server owns the borrowed Windows
`CONTEXT` only during the vectored-exception callback. It copies the bounded
AMD64 register values into `DebugRegisterContext`/the development-debug
snapshot and never stores a `CONTEXT*`. A stopped context is valid only for
its exact session generation, process/runtime identity, thread ID, and stop
generation.

The hosted runtime records the raw RIP and separately normalizes the physical
breakpoint address. The current Windows proof delivered raw RIP equal to the
patched address (`delta = 0`); the handler also accepts and normalizes the
architecturally common `address + 1` form. Rewind is performed only for the
owned binding.

## Continue sequence

```text
PausedAtBreakpoint
  -> restore original byte and verify it
  -> validate stop identity and binding ownership
  -> record pending thread/binding/stop generation
  -> set RIP = breakpoint address in the live callback CONTEXT
  -> set RFLAGS = old RFLAGS | TF
  -> resume the trapped thread
  -> execute the original instruction exactly once
  -> receive the matching EXCEPTION_SINGLE_STEP
  -> clear TF with current RFLAGS & ~TF
  -> reinstall 0xCC when an enabled logical owner remains
  -> publish Running
```

The transient backend states are `PausedAtBreakpoint`,
`PreparingBreakpointResume`, and `SingleStepPending`. A pending internal
single-step is never reported as a user breakpoint hit and is not exposed as
Step Into.

## Ownership and failure behavior

Continue is rejected for a stale session, process, runtime, thread, stop
generation, breakpoint ID, binding, address, artifact identity, or missing
stopped register context. Failed byte restoration, context preparation, resume,
single-step routing, TF clearing, or breakpoint reinstallation does not claim
success. The target remains safely stopped when that is still possible.

Duplicate logical breakpoints share one physical binding. Reinstallation occurs
when any enabled logical owner remains at that exact address. If all owners are
disabled or removed while paused, the original instruction is single-stepped
without reinstalling `0xCC`.

Stop Debugging is serialized with the synchronous internal operation. It may
restore bindings, cancel the pending operation, release the trapped thread, and
tear down the session without performing user stepping.

## Register view and commands

## Phase 7 Call Stack

Call-stack state is valid only while the exact stopped context remains paused.
Continue clears the frame list before the target resumes; a later stop builds
a new identity-bound result. The call stack uses the target-memory boundary,
not instruction memory or host pointer access. See
[DEBUGGER_CALL_STACK.md](DEBUGGER_CALL_STACK.md).

While paused, Developer Studio shows read-only `RIP`, `RSP`, `RBP`, and
`RFLAGS`. The values are cleared when the real single-step completion publishes
Running or when the process exits.

Supported commands:

* Start Debugging — hosted-runtime proven.
* Continue/F5 — hosted-runtime proven from an owned breakpoint or source-step stop.
* Stop Debugging — hosted-runtime proven.
* Pause — disabled.
* Step Into/F11 — hosted-runtime proven through the separate user source-step path.
* Step Over/F10 — hosted-runtime proven through the bounded call-aware path.
* Step Out/Shift+F11 — hosted-runtime proven when frame #0 has a validated caller.

Step Out is documented in [DEBUGGER_STEP_OUT.md](DEBUGGER_STEP_OUT.md). The
next milestone is bounded locals and function arguments; Pause remains disabled.
Continue invalidates the stopped Locals/Arguments generation. Values are rebuilt only after the next genuine paused stop.
