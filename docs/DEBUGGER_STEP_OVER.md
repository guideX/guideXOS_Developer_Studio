# Debugger Phase 6 — Step Over / F10

Phase 7 can display the paused frame-pointer call stack after Step Over, but
does not change F10's call-aware execution path. The stack is cleared when
execution resumes and is not a Step Out implementation; Shift+F11 has its own
return-address operation. See
[DEBUGGER_CALL_STACK.md](DEBUGGER_CALL_STACK.md).

Status: implemented for the bounded hosted AMD64 Native ELF debug path. This
milestone adds real call-aware Step Over; it does not claim call-stack
unwinding, inline-frame semantics, optimized-code parity, or general-purpose
debugger support.

## Phase 9 variable invalidation

Step Over treats Locals/Arguments as stop-generation data; the panels do not retain the caller’s previous values while the callee executes.

## User command and state

F10 is accepted only for an owned, genuinely paused session with a valid
stopped register context, exact session/process/runtime/thread identity, and a
backend that advertises Step Over. F10 is ignored or rejected outside that
state. F5, F9, and F11 retain their existing meanings.

```text
Paused / Breakpoint or Step
  -> F10
  -> StepOverPending
  -> real call-aware execution
  -> Paused / Step, or a real user breakpoint stop
```

Every operation records the session generation, stop generation, process,
runtime, thread, starting address/source location, starting RSP/RBP, and
bounded call/instruction counters. A stopped context is never reused after a
generation or identity mismatch.

## Call decoding

The controller reads at most 16 bytes from the executable target address
through the existing debug ABI. When a software breakpoint overlays the first
byte, the backend returns the retained original byte rather than the `0xCC`
patch. The bounded AMD64 decoder recognizes:

* direct near calls, `E8 rel32`;
* indirect near calls, `FF /2`, with register and memory ModRM/SIB forms; and
* non-call instructions without guessing their length.

Truncated, malformed, unsupported, out-of-range, or non-executable reads do
not become fake calls. A recognized direct call computes its return address as
`instruction address + decoded instruction length`; the controller never
derives that address from a source line or editor caret.

## Real call path

For a recognized call, the controller allocates a high-range internal logical
breakpoint owner, binds it at the exact return address, and asks the hosted
runtime to execute the call. The runtime restores the original call byte,
resumes the trapped thread without TF, and keeps the temporary return binding
armed. The callee therefore executes normally. When the real return reaches
the temporary `INT3`, the runtime reports an internal trap with the exact
binding, address, and thread identity.

The controller removes only the temporary logical owner, preserving any user
breakpoint that shares the same physical binding. If a user breakpoint owns
the return address, that user stop wins. Otherwise the controller publishes a
`Paused / Step` stop at the next source location in the original frame, using
the existing DWARF reverse mapper. Same-source return stops are suppressed by
continuing through the bounded source-step path until the source changes.

## Non-call fallback and bounds

If the current instruction is not a recognized call, Step Over uses the real
source-step engine for one or more processor single steps. It stops at the
next different source line; it never implements F10 as `currentLine + 1`.
Branches, returns, and unmapped instructions follow actual execution.

The operation is bounded to 1,024 source-step instructions and 32 detected
calls. A limit, stale identity, unexpected thread, failed memory read, failed
temporary bind, failed return trap, or process exit interrupts the operation
and removes any temporary owner that was created.

## Ownership and cleanup

Internal owners use a separate high-range ID namespace from user breakpoint
IDs. The physical breakpoint manager remains the single owner of patched
bytes: multiple logical owners share one retained original byte, and the byte
is restored only when the last owner is removed. Temporary owners are removed
on return, user-breakpoint interruption, F5/F11 transition, Stop Debugging,
runtime teardown, and failure paths.

The runtime validates the exact session, process, runtime, thread, stop
generation, binding, and return address before accepting an internal trap.
Internal trap recovery is not presented as a user breakpoint hit. Step Out
uses a separate operation and internal purpose while reusing this physical
ownership manager; it is documented in [DEBUGGER_STEP_OUT.md](DEBUGGER_STEP_OUT.md).

## Evidence

The decoder/model test covers direct and indirect calls, non-calls,
truncation, unsupported encodings, executable-range boundaries, and original
byte overlays. The hosted Windows runtime harness executes a real `E8` call,
observes the callee exactly once, receives the real internal return trap,
removes the temporary owner, restores/rebinds the user breakpoint state, and
verifies teardown bytes.
Structured value nodes are owned by the selected stop generation and are not reused after F10.
Step Over treats Locals/Arguments as stop-generation data; the panels do not retain the caller’s previous values while the callee executes.
