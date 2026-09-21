# Developer Studio Phase 28U — NativeElf Terminal Lifecycle Stabilization

## Outcome

**Outcome B — major lifecycle defect repaired; an independent fresh-boot startup blocker remains.**

Phase 28U repaired the reproducible NativeElf exit/poll ownership defect from Phase 28T and added an explicit debugger-start completion transition. Hosted lifecycle evidence reaches the authenticated debugger-ready state and the exit/teardown path is bounded. The required ten-boot QEMU acceptance could not be claimed: the final AMD64 artifact completed three of five attempted fresh boots, while two fresh boots stopped before the debugger proof completed. The failures were startup/build-handoff stalls, not the Phase 28T nested post-exit poll.

No debugger feature ladder work was added.

## Repository starting state

Standalone repository:

- path: `D:\dev\guideXOS_Developer_Studio`
- branch: `main`
- starting HEAD: `ae6e7961aab34b6f7e3b5f3de7ac881d7a7feab4`
- starting worktree: clean

Server integration repository:

- path: `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO`
- branch: `v0.5_DEVELOPER_STUDIO`
- starting HEAD: `ee9cbcd0718e47dad548635fa6804de7d1a0d807`
- starting worktree: clean

The Phase 28T report was read before implementation:
`docs\DEVELOPER_STUDIO_PHASE28T_NATIVE_LAUNCH_DEBUGGER_STABILIZATION.md`.

## Phase 28T reproduction and call/state sequence

The Phase 28T final artifact reproduced the failure after the target returned. The important sequence was:

1. NativeElf target execution returned from `gx_main`.
2. The scheduler/context path cleared target execution ownership while the caller still treated the process as live. The bounded trace showed `RUN_NESTED_RETURN`, then `TARGET_RETURNED state=00000005`, followed by `TARGET_TO_OWNER state=00000006 active=0 target_complete=1`.
3. A caller still in the terminal/debug/run polling chain entered another NativeElf poll. The second entry was not a required exit acknowledgement; it was execution/lifecycle polling through a path whose assumptions were stale.
4. Terminal polling, debugger polling, and the run controller could each reach the NativeElf host synchronously. A target exit could therefore cause callbacks and acknowledgement work to re-enter dispatch while the original dispatch was still on the stack.

The defect was not fixed by suppressing the second poll. It was caused by missing execution ownership and by using `POLL` for both target advancement and lifecycle acknowledgement.

## Ownership model

Before Phase 28U, the effective ownership was split:

| Resource/work | Before | After |
|---|---|---|
| NativeElf target advancement | terminal/UI, debugger, scheduler, and host paths could reach it | NativeElf scheduler pump is the single execution authority |
| EXITED observation | independently inferred by run, terminal, and debugger layers | authoritative NativeElf transition with stable exit metadata |
| terminal completion | could require another poll of the target | metadata-only acknowledgement; no dead-target execution |
| debugger startup | child/session allocation was treated as sufficient in some paths | explicit generation-checked ready transition after identity, binding, and release |
| scheduler registration | could survive target completion | unregister/cleanup is performed once at authoritative exit |
| launch identity | process/runtime fields could be provisional or stale | session generation and NativeElf runtime identity authenticate every path |
| teardown | terminal/debugger dependencies were implicit | bounded, idempotent release and close operations with stale-generation rejection |

## Exit-handshake repairs

The NativeElf run service now makes exit durable. On authoritative target return it:

- records the final exit code once;
- transitions to a terminal EXITED state;
- clears target execution ownership;
- removes scheduler eligibility;
- unregisters the process/runtime registration once;
- publishes terminal metadata without executing the target;
- makes duplicate completion and exit observation idempotent.

Run, terminal, and debugger paths reject or consume stale `POLL`, Pause, Continue, close, and cancellation requests according to terminal metadata. A stale command cannot return an EXITED target to RUNNING or PAUSED. Terminal release/close is allowed to finish the lifecycle without a second execution poll.

The paused-stop path was also made symmetric: cancellation is followed by the deployment-owned close request, so a paused target cannot remain in a local Stopping state while the server waits for a close event.

## Reentrancy findings and repair

The scheduler pump could be reached while a pump or target callback was still active. A bounded `s_schedulerPumpInProgress` boundary now rejects prohibited synchronous re-entry and records `PUMP_REJECT_REENTRANT`. State-change handling publishes completion and lets the outer dispatch finish; it does not synchronously execute more target work.

The trace was corrected so `DEBUG_POLL_RESULT` is emitted after the actual output snapshot is selected. This makes status, trap, pause, binding, session, and state evidence correspond to the result consumed by the caller.

The regression suite includes rejected re-entry/duplicate release behavior and verifies that exit acknowledgement does not execute the target again.

## Debugger-start handshake

The original hosted path could report debugger start from process/session allocation before the process and debugger were ready for commands. The repaired hosted path waits for an authenticated process/runtime identity, the initial debugger snapshot, verified breakpoint binding, and execution release. Only then does it publish `DEBUG_START_HANDSHAKE_READY` and the public `debug_start=PASS` marker.

For bare NativeElf, the manager owns the parked one-shot entry-trap materialization/release. The controller now receives an explicit, generation-checked external execution-release completion through `DebugControllerAcceptExternalExecutionRelease`. The transition is idempotent for the same generation and rejects a late acknowledgement from another generation.

This removes the previous dependency on an ordinary later poll to infer that startup had completed. Startup failure still requests bounded stop/teardown.

## Generation and relaunch safety

Session generation is carried through controller startup, NativeElf runtime identity, terminal metadata, breakpoint binding, and release/close requests. Late events are rejected when their generation does not match the active launch. The controller regression coverage includes:

- duplicate external startup acknowledgement;
- wrong-generation startup acknowledgement;
- stale exit/terminal state not affecting a later launch;
- relaunch state reset and independent binding ownership.

The bounded fake-controller fixtures use static storage for the large controller object, preserving the bounded-storage rule and avoiding a test-stack overflow.

## Files changed

Standalone changes are in:

- `src/developer_studio_debugger.cpp`
- `src/developer_studio_debugger.h`
- `src/developer_studio_debugger_hosted.cpp`
- `src/developer_studio_run.cpp`
- `src/main.cpp`
- `tests/debugger_test.cpp`
- `tests/run_test.cpp`
- `tests/smoke-developer-studio-debugger.ps1`

Server changes are in:

- `development_run_service.cpp`
- `kernel/core/native_elf/native_elf_loader.cpp`
- `kernel/core/native_elf/native_elf_run_service.cpp`
- rebuilt Developer Studio AMD64/ARM64 package artifacts and `desktop.json`

The Phase 28T DWARF capacity change, corrected Phase 28Q markers, and native build dependency repairs were preserved.

## Validation

### Hosted fast tier

- CTest: **31/31 passed**.
- AMD64 package build: passed; audit confirmed ELF64 AMD64 ET_EXEC with no section/debug metadata.
- ARM64 package build: passed; audit passed.
- Phase 28T freestanding DWARF capacity regression: **`PASS, dies=397`**.

The targeted hosted smoke had a complete pass in the normalized fixture run (`TraceRunIndex 102`), including real build/debug start, source breakpoint pause, call stack/locals, Continue/rebind, ordered shutdown, terminal completion, debugger teardown, and clean hosted server exit. A later final-build retry reached explicit startup readiness and a real breakpoint pause but was interrupted after the UI harness stopped making progress; its preserved trace is `logs\phase28u-hosted-final\developer-studio-debugger-shutdown-trace-104.log`. Another retry stopped during scripted breakpoint input. These are harness nondeterminism signals, not access violations or the Phase 28T nested terminal-exit deadlock.

### QEMU

The final AMD64 artifact used for the final QEMU attempts was:

`D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\Apps\DeveloperStudio\bin\amd64\developerstudio.elf`

SHA-256:

`F549B6A3718555E19D169849FCEAAF96200C8A1C2423408B1C903DDEA086486C`

All final-hash QEMU boots below used that exact artifact, verified in each disposable ESP package.

Evidence:

- `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-ea5768dd1832453c9a2bf0623689de20`
  - boot 1: complete Phase 28Q lifecycle pass;
  - boot 2: complete Phase 28Q lifecycle pass;
  - boot 3: stopped after build completion at `DEVELOPER_STUDIO_PHASE28Q_DEBUG_START_PASS`, before debugger lifecycle markers.
- `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-140ad0a716f14da283253370e42f730b`
  - boot 1: complete Phase 28Q lifecycle pass;
  - boot 2: stopped in the post-build startup handoff after `DEVELOPER_STUDIO_PHASE28Q_DEBUG_START_PASS`; it emitted no release or debugger-ready marker.

The successful QEMU boot trace contains the required startup, two pause/continue cycles, `DEVELOPER_STUDIO_PHASE28Q_PASS`, `gx_main returned 0`, EXITED state, cleanup, and no nested target poll after return. The failed boots do not show post-exit polling; they stop before the target lifecycle reaches that point.

The requested ten all-pass fresh-boot acceptance was therefore not met. A 25-cycle hosted stress run was not started because the bounded acceptance prerequisite remained incomplete and the hosted scripted UI harness was already demonstrating nondeterministic stalls.

## Remaining limitations

- Fresh QEMU boot reliability remains below the required 10/10 bar. The remaining blocker is a synchronous startup/build-to-debug handoff stall that occurs before the final startup release marker on some fresh boots.
- No physical hardware testing was performed.
- The hosted UI smoke harness has intermittent input/progress stalls even when the lifecycle trace has reached debugger-ready and paused-breakpoint states.

## Git and push status

At report creation, standalone and server changes were still local and uncommitted. The next stopping action is to commit each repository independently, verify clean worktrees, and attempt the configured upstream pushes without rebasing or merging unrelated branches. If SSH authentication fails, the exact repository, branch, starting/ending HEADs, commit, ahead/behind counts, and `git@github.com: Permission denied (publickey).` error will be recorded in the phase handoff.
