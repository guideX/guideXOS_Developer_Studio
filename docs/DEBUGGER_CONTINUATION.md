# Debugger Phase 4 — Breakpoint Continuation

Phase 4 implements real continuation from a hosted AMD64 software breakpoint.
The supported target remains the Windows-hosted, fixed-address ELF64 `ET_EXEC`
runtime used by Developer Studio.

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

While paused, Developer Studio shows read-only `RIP`, `RSP`, `RBP`, and
`RFLAGS`. The values are cleared when the real single-step completion publishes
Running or when the process exits.

Supported commands:

* Start Debugging — hosted-runtime proven.
* Continue/F5 — hosted-runtime proven from an owned breakpoint stop.
* Stop Debugging — hosted-runtime proven.
* Pause — disabled.
* Step Into, Step Over, Step Out — disabled.

The next milestone is **Debugger Phase 5 — user-visible Step Into and
source-level single stepping**.
