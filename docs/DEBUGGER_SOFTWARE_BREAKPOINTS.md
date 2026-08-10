# Debugger Phase 3: Hosted Software Breakpoints

Phase 3 adds the first hosted Native ELF software-breakpoint path. It is intentionally
limited to the current AMD64/ELF64 fixed-address `ET_EXEC` runtime and is not a general
purpose process debugger.

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
Live enable/disable and safe continuation are not exposed yet.

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
identity, thread ID, normalized target address, binding ID, and the raw instruction
pointer. The Phase 2 reverse DWARF mapper resolves the normalized address back to the
source location for the editor execution marker and Debug Session panel.

## Restoration and teardown

The original byte is retained before every patch. Stop/cancel, runtime teardown, and
debugger restore commands restore all active physical bindings while the image mapping
still exists, then release the blocked target thread. Restoration is verified through
the same protected write/cache-flush path. Restore failures are reported as bounded
backend errors; an old artifact is never patched after a rebuild or stale-map event.

The current Phase 3B runtime does not implement Continue. Correct continuation requires
restoring the byte, executing the original instruction exactly once, single-stepping,
reinstalling `0xCC`, and routing the internal single-step trap. Pause, Step Into, Step
Over, and Step Out remain disabled. Stop Debugging remains supported through the hosted
development-run close path.

## Scope and evidence boundary

Supported target: AMD64, little-endian ELF64, fixed-address `ET_EXEC`, load bias zero,
Windows-hosted Native ELF execution. Bare-metal breakpoint handling, arbitrary memory
read/write, register inspection, call stacks, and user-visible stepping are deferred.

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
AMD64 little-endian ELF64 fixed-address `ET_EXEC` runtime with load bias zero;
Continue, stepping, register/context inspection, memory access, call stacks, and
multi-process/attach workflows remain unsupported.
