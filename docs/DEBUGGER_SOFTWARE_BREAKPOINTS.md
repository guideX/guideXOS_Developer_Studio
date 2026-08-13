# Debugger Phase 4: Hosted Software Breakpoints and Continuation

Phase 7 extends the exact paused-stop identity with validated target stack
bounds and bounded stack reads for the Call Stack tab. It does not change the
software-breakpoint ownership or continuation protocol. See
[DEBUGGER_CALL_STACK.md](DEBUGGER_CALL_STACK.md).

Phase 3 adds the first hosted Native ELF software-breakpoint path. It is intentionally
limited to the current AMD64/ELF64 fixed-address `ET_EXEC` runtime and is not a general
purpose process debugger.

## Phase 9 interaction

Locals/Arguments inspection is rebuilt only after a real owned breakpoint stop; breakpoint source text is never treated as a value source.

## Phase 5 interaction

The Phase 4 internal breakpoint-recovery single step is distinct from the
user-visible source-step operation. F11 may begin at a software breakpoint by
using the same byte-restore/RIP/TF primitive for the first instruction, then
reinstalling the INT3 before deciding whether the next real single-step reaches
a new DWARF source location. The breakpoint remains bound while stepping and a
later real breakpoint still wins over Step Into.

## Phase 6 internal return owners

Step Over reuses this physical binding manager rather than introducing a second
`INT3` subsystem. A temporary return breakpoint is a distinct high-range
logical owner, while a user breakpoint remains a normal user owner. If both
owners target the same address, one physical binding retains one original byte
and both logical owners are reported. Removing the temporary owner leaves the
user owner and `0xCC` installed; removing the final owner restores the original
instruction.

The internal owner is removed on a real return trap, user-breakpoint
interruption, stale/failing command, process exit, Stop Debugging, and runtime
teardown. Internal return traps are accepted only for the exact active
Step-Over session, process, runtime, thread, stop generation, binding, and
return address.

## Hosted execution model

The Server loads the validated Native ELF image into an `ExecutableMemoryBlock` at its
preferred fixed address. The current loader uses load bias zero, so a DWARF target
address is the numerical address used in the backing mapping. The debugger still keeps
the two concepts separate: the public binding identity is a target address, while the
Server owns the private host backing mapping used to patch that address.

Each hosted application has a guideXOS `ProcessTable` process ID, a Native App runtime
ID, and one or more host threads. These are not assumed to be a Windows process ID.
The development-run service publishes the exact deployment process ID and later the
matching runtime ID before debug commands are accepted. Trap records also include the
faulting host thread ID when the Windows exception context provides it.

## Launch gate

Debug-controlled launch sets an execution gate before `BeginHostCallDispatch` and
before the Native ELF entry point is called. The runtime registers the image and its
exact process/runtime identity, then publishes the prepared process. Developer Studio
binds mapped breakpoints and sends `ReleaseExecution`. This avoids a startup sleep or
race near the entry point. Ordinary Run Project launches do not use the gate.

## Binding and state meanings

The generic debugger keeps the following meanings distinct:

* **Pending**: a source breakpoint has no usable executable mapping yet.
* **Mapped**: DWARF produced one or more target addresses. The deterministic primary
  address is the only address patched in this milestone; the complete bounded list is
  retained for diagnostics.
* **Verified**: the hosted backend accepted the patch and verified `0xCC` in target
  memory. The UI reports this as Bound / Verified.
* **Hit**: the last observed owned trap matched the binding. This is separate from the
  persistent Verified binding state.

For AMD64, the physical instruction is `INT3` (`0xCC`). Before patching, the Server
validates the exact session/process/runtime and artifact hash, checks that the address
is within an executable ELF `PT_LOAD` range and the current backing mapping, reads and
retains the original byte, temporarily makes the containing page writable, writes
`0xCC`, flushes the instruction cache, restores the original protection, and verifies
the byte. Host pointers never cross the debugger ABI.

Multiple logical source breakpoints at one address share one physical binding. The
binding stores bounded logical owners and one original byte. A future removal API must
remove only the logical owner; the byte is restored only when the last owner is gone.
Phase 4 preserves duplicate ownership through continuation. The logical
controller tells the backend whether at least one enabled owner remains; the
physical binding is rebound only in that case.

## Trap handling

The hosted runtime installs one Windows vectored exception handler. It handles only an
`EXCEPTION_BREAKPOINT` whose current guideXOS process has an active debug runtime, whose
address matches an installed physical binding, and whose execution gate is open.
Unrelated processes, addresses, sessions, and teardown-time exceptions are not claimed.

The focused Windows VEH harness records `RIP` at the patched `INT3` address in this
hosted runtime. The handler also accepts the one-byte-after form so the backend remains
robust to host exception-context differences, but it always reports the installed
binding address as the normalized target. It does not infer the target from a polling
marker. The faulting target thread waits on a resume event, so Developer Studio does
not report Paused while that thread continues asynchronously. Other runtime threads
are not claimed to be paused.

An owned trap produces `Running -> Paused` with stop reason `Breakpoint`, process/runtime
identity, thread ID, normalized target address, binding ID, raw instruction pointer,
and a copied fixed-width register context. The Phase 2 reverse DWARF mapper resolves
the normalized address back to the source location for the editor execution marker
and Debug Session panel.

## Restoration and teardown

The original byte is retained before every patch. Stop/cancel, runtime teardown, and
debugger restore commands restore all active physical bindings while the image mapping
still exists, then release the blocked target thread. Restoration is verified through
the same protected write/cache-flush path. Restore failures are reported as bounded
backend errors; an old artifact is never patched after a rebuild or stale-map event.

## Continuation

For an exact owned stop, Continue validates session generation, process/runtime ID,
thread ID, stop generation, breakpoint ID, physical binding, target address, and
artifact identity before changing target state. The sequence is:

```text
EXCEPTION_BREAKPOINT
  -> restore original byte and verify it
  -> mark the exact thread/binding/stop generation pending
  -> set RIP to the breakpoint address and RFLAGS.TF inside the live VEH CONTEXT
  -> resume the blocked exception callback
  -> execute the restored instruction once
  -> receive the matching EXCEPTION_SINGLE_STEP
  -> clear TF while preserving all other flags
  -> reinstall 0xCC when an enabled logical owner remains
  -> publish Running
```

The handler never retains a `CONTEXT*`; it copies the stopped registers into
bounded state and mutates the borrowed context only during the callback. A
single-step from another process, thread, session, generation, or without a
pending internal operation is left unclaimed. The internal single-step is not
exposed as Step Into.

## Scope and evidence boundary

Supported target: AMD64, little-endian ELF64, fixed-address `ET_EXEC`, load bias zero,
Windows-hosted Native ELF execution. Bare-metal breakpoint handling and arbitrary
memory read/write remain deferred. Phase 7 adds a bounded stopped-context call stack
and Phase 8 adds the documented hosted F11/F10/Shift+F11 paths.

The model tests cover binding state transitions, retained/installed-byte metadata,
duplicate logical ownership, partial-bind rollback, stale/incorrect trap routing,
RIP-versus-target normalization, no-fake-stop behavior, and natural exit. The
authoritative fixture proof establishes the bounded hosted Level B source-breakpoint
path: it records the actual artifact identity, original byte `0xC7`, installed `0xCC`,
exact process/runtime identity, a real `EXCEPTION_BREAKPOINT`, reverse mapping to
`src/main.cpp:20`, UI source navigation, execution marking, and clean stop/restore
teardown. These fields come from the running hosted target and the real Developer
Studio UI path, not from a fake backend or predetermined callback.

The evidence boundary remains narrow: the target is the current Windows-hosted
AMD64 little-endian ELF64 fixed-address `ET_EXEC` runtime with load bias zero.
Pause, arbitrary memory access, and multi-process/attach workflows remain unsupported.
The Phase 7 Call Stack is available only after an exact owned stopped context. A
normal F5 with no active debugger remains ordinary Run.

Step Out uses the same physical binding table as user breakpoints and Step Over.
If a user breakpoint already owns the caller return address, the logical
StepOut owner is added without a second patch. User intent wins at the combined
trap, so the persistent breakpoint remains `Paused / Breakpoint` after the
temporary owner is removed.
Locals/Arguments inspection is only rebuilt after a real owned breakpoint stop; breakpoint source text is never treated as a value source.
Structured children are also stop snapshots. They are cleared with the parent
view and can be expanded only while the same paused artifact and stop
generation remain current.
