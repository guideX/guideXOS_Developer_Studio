# Developer Studio Phase 29O — Hosted Stepping and Second-Stop Lifecycle

## Scope and starting revisions

| Repository | Branch | Starting HEAD |
| --- | --- | --- |
| Standalone Developer Studio | `main` | `fec0f05990cdb7048f984e8fd3396adeff328528` |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `a85d9c41ec6f4cac8f7a6696c2133aa4a6b1d20e` |

Phase 29N is the protected baseline. It already passed hosted source-breakpoint/Continue validation, including a source stop at `src/main.cpp:37`, software-breakpoint rebind, teardown, Debug/Release CTest, package audits, and QEMU. Phase 29O did not reopen source association, breakpoint mapping/binding, F5 routing, Continue handling, or the Phase 29N teardown path.

The stepping smoke used the existing Phase 15 fixture, which contains a deterministic `debugLoop` → `debugCaller` → `gx_main` call chain. The built DWARF/source-breakpoint request for this fixture stops at the fixture's executable line 42; that location is distinct from the Phase 29N line-37 proof and both records are retained as their respective baselines. The stepping fixture and source were copied to external validation directories; the six pre-existing standalone untracked artifacts and the Server diagnostic sentinel were left in place.

## Internal breakpoint rebind versus user steps

These are separate debugger operations:

- The Phase 29N internal rebind single-step executes the original instruction underneath a software breakpoint and reinstalls the persistent user breakpoint. Its backend single-step kind is `InternalBreakpoint` and its command generation is zero. It is never published as a user Step stop.
- User Step Into owns a nonzero command generation and publishes one authoritative `Step` stop after the requested instruction/source-step operation. When started at a user breakpoint, the internal unpatch/rebind work is part of that one command.
- User Step Over and Step Out own their own command generations and completion stops. Temporary return traps used by those operations retain their temporary owner identity and are removed as part of completion or cancellation.

The hosted and native runtime traces showed no extra visible stop for the breakpoint-rebind operation. The native runtime regression also checks that internal trap snapshots remain distinct from command-owned user step results.

## Baseline behavior and UI routes

Before the production ownership changes, a real hosted stop accepted all three user commands and completed them. No user-visible routing, controller-capability, execution-handoff, mapping, or inspection-refresh failure was reproduced. The defects found in Phase 29O were in explicit step/stop ownership: the async result path did not carry a unique user command identity end to end, and user single-step completions needed to advance the stop high-water mark independently from the breakpoint stop.

Observed product input routes on a focused run:

| Command | Product input | Handler route | Backend implementation on this fixture |
| --- | --- | --- | --- |
| Step Into | F11 | `keyboard_debug_panel` | `source_single_step` / hardware trap-flag single-step |
| Step Over | F10 | `keyboard_global` | `bounded_source_single_step`, using current DWARF source rows |
| Step Out | Debug menu row 4 | `debug_menu` | `temporary_return_breakpoint` |

The keyboard and menu paths dispatch through the same public controller operation for a given command; no private test-only route was used. The UI renders the controller's `CanContinue`/`CanStepInto`/`CanStepOver`/`CanStepOut` predicates. They are true for all four actions at the initial paused stop and the first two step stops; after Step Out reaches `gx_main` with no caller, `CanStepOut` is false. In `Stepping` or `Running`, step and Continue predicates are false. The controller test submits duplicate Step Into, Step Over, and Continue while Step Into is pending: all are rejected without issuing another backend step or changing the active command identity.

## Stop and command ownership evidence

One representative final focused trace (`focused-13`) reported session generation 1, target generation 1, process 12, runtime 2, thread 24492, module generation 1, and symbol generation 1. The OS thread ID varies between isolated hosted runs. The stop generations below are the authoritative generations published to the UI; backend internals may advance through intermediate events, so a user command is matched by both command identity and a strictly newer authoritative stop.

| Stop | Owner command | Reason | Raw PC | Normalized PC | Frame 0 and source | Refreshed inspection |
| --- | --- | --- | --- | --- | --- | --- |
| STOPPED 1 | Initial user breakpoint (`command_gen=0`) | Breakpoint | `0x200016D7` | `0x200016D7` | `_ZL9debugLoopi`, `src/main.cpp:42` | Stack stop 1, 4 frames; 1 argument, 6 locals |
| STOPPED 6 | Step Into (`command_gen=1`) | Step | `0x200016EA` | `0x200016EA` | `_ZL9debugLoopi`, `src/main.cpp:43` | Stack stop 6, 4 frames; 1 argument, 6 locals |
| STOPPED 10 | Step Over (`command_gen=2`) | Step | `0x20001619` | `0x20001619` | `_ZL11debugCalleri`, `src/main.cpp:48` | Stack stop 10, 3 frames; 1 argument, 1 local |
| STOPPED 11 | Step Out (`command_gen=3`) | Step | `0x2000132A` | `0x2000132A` | `gx_main`, `src/main.cpp:59` | Stack stop 11, 2 frames; 1 argument, 2 locals |

Every row belongs to the current symbol generation and reports `source_map=current`. The selected frame is 0. The changing local counts reflect scope changes and were read from the new stop, not carried forward from an older stack or variable snapshot.

Each request also carried its command type, command generation, session generation, target generation, and starting stop generation. Representative identities were:

| Command | Command generation | Starting stop | Result stop | Result |
| --- | ---: | ---: | ---: | --- |
| Step Into | 1 | 1 | 6 | accepted; one authoritative source-visible Step stop |
| Step Over | 2 | 6 | 10 | accepted; bounded source single-step to the next mapped line |
| Step Out | 3 | 10 | 11 | accepted; temporary return trap stopped in caller |

Raw and normalized PC handling was checked independently of the Phase 29N INT3 breakpoint normalization. Trap/single-step stops are not decremented as though they were INT3 stops. On this hosted trace raw and normalized PCs match. Controller unit cases also verify raw `instructionPointer=0x109` normalizes to Step Over address `0x108`, and raw `0x202` normalizes to the Step Out caller return address `0x201`; these tests exercise the temporary-trap target contract without applying the trap-flag normalization to it.

## Step implementation details

### Step Into

The hosted path issues a backend source single-step. The native runtime uses the target's AMD64 trap flag and reports a real single-step exception. The observed transition was `STOPPED 1 → command 1 → execution released → STOPPED 6`; the current source advanced from line 42 to line 43 in `debugLoop`. The stop generation increased, register context and current source location were replaced, and the old stopped context was invalid while execution was in progress.

When Step Into starts on a software breakpoint, the backend restores the original instruction, performs the requested user step, clears trap state, and reinstalls the persistent breakpoint. The user receives one new Step stop. The internal rebind event remains command generation zero.

### Step Over

For the hosted fixture's mapped source location, Step Over used the bounded repeated single-step/source-line implementation and stopped at the next mapped location in `debugCaller` at line 48. It did not install a temporary Step Over breakpoint in this run. The production controller's call-instruction branch is separately covered: it decodes the call/fall-through address, binds a temporary return breakpoint with a generated temporary ID, retains the command/session/starting-stop owner, and removes that owner on completion. The controller test also injects a completion with the wrong command generation and verifies rejection plus temporary-owner cleanup.

The fixture's source progression is the acceptance contract; the machine instruction at the initial breakpoint does not imply a one-to-one source-line progression.

### Step Out

Step Out requires a fresh current stop and a fresh selected stack with a caller frame and validated return address. The controller reads the return address from the selected frame's caller context, binds a temporary Step Out owner, and resumes to that return site. The hosted request used temporary owner `9223372036854775809` (`0x8000000000000001`), binding 2, return address `0x2000132A`, and caller lookup address `0x20001329`. It produced `STOPPED 11` in `gx_main` at line 59. The stop reason is `Step`, not `Breakpoint`.

The temporary owner was removed after the new stop (`cleanup=1`, `temp=FALSE`, binding owners 0, not installed). Unit/runtime coverage also checks return-address unwind selection, a return address shared with a persistent user breakpoint, and retention of the persistent owner when the temporary owner is removed.

## Production ownership repair

The repair adds explicit user-step identity across controller, SDK, native backend, and stop snapshot:

- The controller allocates a monotonic nonzero per-session `commandGeneration` and records the starting session, target/process, thread/context, and stop generation for Step Into, Step Over, and Step Out.
- The SDK appends `commandGeneration` to the debug request and snapshot without changing earlier field offsets. The request ABI is now 168 bytes with a 168-byte step extension; the snapshot is 27,392 bytes with the field appended. Compile-time ABI layout checks cover the new offsets and sizes.
- The native backend requires nonzero command identity for user Step Into/Over/Out, stores that identity with the pending operation, and copies it into the resulting authoritative snapshot.
- A user single-step allocates a stop generation above both the pending stop and the per-runtime high-water mark. The controller accepts only the active command identity and a fresh stop; a delayed completion from another command fails as stale.
- Internal breakpoint rebind retains command generation zero and its own trap kind. It cannot satisfy or increment a pending user step.
- Step Over/Out temporary state is keyed by session/command/temp-owner/binding/address and is cleaned on successful completion, stale result, cancellation, target exit, and teardown paths. Persistent user breakpoint ownership remains separate.
- New trace fields record UI route, controller state/capabilities, command and stop generations, implementation mode, raw/normalized PC, selected frame, source/symbol generation, inspection ownership, breakpoint owners, and temporary cleanup.
- The debug trace scratch buffer in `main.cpp` was enlarged from 256 to 512 bytes because the prior buffer truncated Step Out caller lookup diagnostics. This only affects diagnostics.

The current software breakpoint remained logically enabled and installed at `0x200016D7` after the user steps (user breakpoint ID 1; one user owner/reference, no internal owner). Continue after STOPPED 11 used the current stop, changed the target to Running, invalidated old inspection context, then the fixture reached its normal close/exit lifecycle. Target and session teardown completed; no temp step owner or pending command remained.

## Watches and inspection refresh

The existing watch expression was `counter == 2`. The hosted panel attempted a fresh evaluation at STOPPED 1 and STOPPED 6 (and again at STOPPED 10), keyed to the requested stop generation. The target ELF is 12,480 bytes and has no authoritative GXSM metadata (`present=0 valid=0 mappings=0`), so the backend correctly returned “authoritative GXSM metadata is unavailable” instead of a value. This fixture does not support expression evaluation; no watch value is claimed. During Step Out's internal return-trap handoff, an intermediate watch request was rejected as “internal debugger trap is not a user pause”; it was not accepted or displayed as a user stop.

Stack and locals were refreshed at every visible stop. The source highlight, call stack, locals/arguments, watch request generation, debugger status, and current frame were tied to the new stop. While execution was released, the controller invalidated the previous stopped context, call stack, variable view, and watch freshness.

## Exit, cancellation, teardown, and relaunch

A focused production-controller regression makes target exit win while Step Into is pending. It verifies an `Exited` state, cancelled step command, no synthetic new STOPPED generation, no valid old stopped context/stack/locals, and a retained per-session stop high-water mark. Duplicate Step Into, Step Over, Continue, and Pause are rejected while the controller is in `Stepping`; they do not issue another backend command or alter the active step identity. After the completed source step is continued to `Running`, a real Pause request produces a new `UserPause` stop with command generation zero; it leaves the user step completion generation unchanged and does not masquerade as another Step result. The real hosted 5/5 and 25/25 lifecycles continued after the final step, reached the normal target exit, and completed debugger/window/server teardown.

Each hosted gate iteration launched a fresh hosted Server/Developer Studio process. The new process began without an inherited step command or temporary breakpoint, remapped the logical source breakpoint, and reached its first breakpoint stop. The focused Pause case is controller/backend-path coverage; a hardware-timed Pause-versus-trap race was not run.

## Regression coverage and gates

- Standalone step-controller tests cover current-stop admission, stale stop rejection, new stop generation, wrong thread, stale command completion, Step Over temp identity/cleanup, Step Out caller/return validation, persistent breakpoint overlap, fresh inspection invalidation, target exit during a pending step, rejection of duplicate Into/Over/Continue/Pause while a step is active, and a Pause stop after resuming a completed step.
- Server native runtime tests exercise real trap-flag user single-step, stop-generation advancement, command identity on user Into/Over/Out results, internal rebind distinction, return traps, persistent/temporary breakpoint overlap, and teardown byte restoration.
- Native ABI layout test: PASS.
- Debug CTest: 33/33 PASS.
- Release CTest: 33/33 PASS.
- DWARF capacity: PASS (`397` DIEs).
- Developer Studio AMD64/ARM64 package content and ELF audit: PASS.
- Hosted focused stepping: 5/5 consecutive fresh sessions PASS. Each executes initial breakpoint, Into, Over, Out, inspection, Continue, exit, and teardown.
- Hosted stepping stress: 25/25 consecutive fresh sessions PASS with all three user steps repeated in each lifecycle.
- QEMU fresh Phase 28Q regression: 10/10 PASS.
- QEMU fresh Phase 28Q stress: 25/25 PASS. Every boot reached the native loader and passed the existing diagnostic sentinel/project/package checks. These gates ran from an external mirror so the protected Server ESP and sentinel were not staged or mutated.
- No physical hardware run was performed.

## Package hashes and outcome

The final package audit reported:

| Architecture | SHA-256 |
| --- | --- |
| AMD64 | `570B9083B615A17D2FF63185600B32A54A8C584B9DD0D66FD440DF800593E23E` |
| ARM64 | `7F3C61A05A335B47C32B76C5F0769A6C6251F601171D4D01DA0737B926A40335` |

Phase 29O reproduced no broken user-step behavior in its initial baseline observation. It materially hardens production command/stop ownership and verifies the true UI paths. The fixture's absent GXSM variable metadata prevents a watch value from being evaluated, as allowed by the fixture-support condition. No user-step lifecycle blocker remains for this phase. **Final Outcome: A.**
