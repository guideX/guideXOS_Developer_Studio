# Debugger Phase 5 — Source-Level Step Into

Status: implemented in the debugger model and hosted AMD64 path. The feature
is intended for bounded debug-build Native ELF sessions (`-g -O0`) and does not
claim optimized-code debugging parity.

## User command

`F11` is Step Into only when an owned session is genuinely `Paused`, the
backend advertises `canStepInto`, the stopped register context is valid, and
the current RIP has a DWARF source location. F11 is ignored outside that state.
`F5` remains Continue/Run and `F9` remains source-breakpoint toggle.

```text
Paused -> F11 -> Stepping -> real EXCEPTION_SINGLE_STEP event(s)
        -> DWARF lookup / same-source suppression -> Paused / Step
```

Each operation records session and stop generations, process/thread identity,
starting and last addresses/source locations, and a bounded instruction count.
The current bound is `kDebugMaxSourceStepInstructions` (1024 instructions).

## Internal and user-visible single steps

Phase 4 breakpoint continuation remains a private
`InternalBreakpointResumeStep`: restore the original byte, rewind to the
canonical breakpoint address, set TF, execute one instruction, clear TF, and
reinstall INT3 before unrestricted Continue.

Phase 5 uses a separate `UserSourceStep` operation. F11 from an INT3 reuses the
same restore/reinstall primitive for its first instruction, but the target is
held at every real single-step trap until source stepping decides whether to
issue another instruction step or publish a Step stop. Internal recovery is
never reported as F11 completion.

## DWARF stopping policy

Address-to-source lookup uses the existing sequence-aware DWARF line mapper.
Ranges do not cross `DW_LNE_end_sequence`. Unmapped addresses are stepped
through without fabricating source locations. A visible stop occurs when the
canonical project-relative source path or line differs from the starting
location. Columns remain metadata and are ignored for first-pass equality;
`is_stmt` remains part of mapper row selection/tie-breaking rather than a new
stop rule.

Calls, branches, returns, and cross-file transfers follow actual processor
execution and DWARF truth. No `currentLine + 1` or editor-caret simulation is
used.

At completion the controller publishes `Paused / Step`, captures fresh
RIP/RSP/RBP/RFLAGS, and moves the existing execution marker through normal
source-document navigation without adding every debugger stop to ordinary
Back/Forward history. Breakpoint markers remain unchanged.

While `Stepping`, stopped registers are cleared. TF is clear at every visible
Paused state and before unrestricted Continue. A real breakpoint encountered
during stepping wins and cancels the source-step operation. Process exit clears
the operation and context. An unexpected event or step-bound exhaustion leaves
the target controlled and reports a bounded failure/limit reason.

The current implementation is single-thread focused: one operation stays on
one process and target thread. Inline-function semantics, optimized Native ELF
debugging, Step Over, and Step Out remain future work.

The source-step model test covers capability gating, same-line suppression,
different-line completion, register refresh, repeated stepping, generation
identity, and wrong-thread rejection. The native runtime harness proves real
AMD64 TF-driven `EXCEPTION_SINGLE_STEP` events from an INT3 stop and a source
step stop, including breakpoint rebinding and step resume.

The next milestone is **Debugger Phase 6 — Step Over**, building on this
single-step engine to execute an entire called function and stop at the next
source location in the current frame.
