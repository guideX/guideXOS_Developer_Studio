# guideXOS Developer Studio — Phase 28W
# Second-Continue Post-Exit Stall Stabilization

## Result

Phase 28W conclusively repairs the targeted second-Continue/target-exit race. A
Continue that observes target exit now resolves from authoritative lifecycle
metadata. It does not re-enter the scheduler, revive the target, or wait for a
target-side acknowledgement that cannot arrive.

The full Phase 28W acceptance gate was not reached because the first boot of
the required 10-boot batch exposed a separate pre-Continue paused-context
blocker. That boot reached the second Pause, but the UI/controller rejected
the second Continue before sending a Continue command. This is independent of
the repaired post-exit stall.

Classification: **Outcome B — known second-Continue defect repaired; a new
independent blocker remains.**

## Repository starting state

The repositories were verified clean before implementation:

| Repository | Branch | Starting HEAD | Origin divergence |
| --- | --- | --- | --- |
| `D:\dev\guideXOS_Developer_Studio` | `main` | `951c7ee9d9325c26156f87066728deb13fb9fb3b` | 0 ahead / 0 behind |
| `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO` | `v0.5_DEVELOPER_STUDIO` | `6f25d10ff06fb741ec0eea6e9b31a8c0405a2072` | 0 ahead / 0 behind |

The Phase 28V report was read before implementation:

`D:\dev\guideXOS_Developer_Studio\docs\DEVELOPER_STUDIO_PHASE28V_FRESH_BOOT_STARTUP_STABILIZATION.md`

Its baseline was preserved: startup stages 1–19, explicit launch-storage
initialization, generation-safe exit handling, durable non-reentrant NativeElf
exit, terminal metadata completion, 31/31 CTest, and DWARF capacity `dies=397`.
The Phase 28V artifact hash was
`1889C1D230FA9E9FFD700C11C6C039DD2FB99C6324EB9BAAE45FB2AA96D80642`.

## Part A — exact reproduction and event ordering

The Phase 28V failed boot remains the authoritative pre-repair reproduction:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-15d4841ac38e4750b5785f78645f508a`

Boots 1 and 2 completed. Boot 3 reached startup stage 19, first Pause,
first Continue, second Pause, and the second Continue host-command return.
Its last acknowledged transition was:

```text
PHASE28Q_RESUME_RESULT_MAP_COMPLETE
PHASE28Q_RESUME_HOST_COMMAND_RETURN
HOST_RUN_POLL_RETURN
```

The next expected transition was
`PHASE28Q_CONTROLLER_CONTINUE_RETURN_TRUE`, followed by
`PHASE28Q_CONTINUE_SECOND_PASS`. Neither occurred. The target had already
returned and teardown was waiting on a path that still expected normal
Continue completion.

The repaired one-boot evidence is:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-1470e15291f84552b4fda884556b5d5a`

The relevant serial ordering is:

```text
1297  PHASE28U_TRACE TARGET_RETURNED state=...05 ... target_complete=0
1298  PHASE28U_TRACE TARGET_TO_OWNER state=...06 active=0 ... target_complete=1
1303  PHASE28U_TRACE CONTINUE_TERMINAL_AFTER_PUMP state=...06 active=0 ... target_complete=1
1304  DEVELOPER_STUDIO_PHASE28Q_CONTINUE_PASS
1312  PHASE28Q_RESUME_RESULT_MAP_COMPLETE
1313  PHASE28Q_RESUME_HOST_COMMAND_RETURN
1314  PHASE28U_LOADER HOST_RUN_POLL_ENTRY nested=0 ... runtime_state=...03
1317  PHASE28Q_CONTROLLER_CONTINUE_RETURN_TRUE
1318  DEVELOPER_STUDIO_PHASE28Q_CONTINUE_SECOND_PASS
1339  PHASE28Q_FINAL_RESULT_PASS
1342  PHASE28Q_CLEANUP_PASS
1343  PHASE28Q_PASS
```

The important divergence is that `CONTINUE_TERMINAL_AFTER_PUMP` now maps the
already-authoritative EXITED lifecycle state to a completed Continue result.
The controller then applies the terminal snapshot and returns exactly once.

## Part B — Continue semantics

Continue has two roles, but only one completion owner:

1. In a live PAUSED stop it is a command that permits scheduler progress and
   changes the debugger state to running or to the breakpoint single-step
   pending state.
2. If EXITED has won the race, it is a lifecycle command whose result is
   **completed-by-exit**. It is not an instruction to execute the target again.

The target execution loop is never required to acknowledge a Continue after
EXITED. A synchronous NativeElf debug request may therefore return a terminal
snapshot even when no target execution context remains.

The resulting semantics are:

| Situation | Result |
| --- | --- |
| Continue while target is PAUSED | Accept; target becomes runnable and execution resumes. |
| Target exits immediately after Continue is accepted | Exit wins; Continue completes by terminal lifecycle state, with no revival. |
| Target exits before Continue is consumed | Continue resolves early from authoritative EXITED metadata; no scheduler wait. |
| Target exits while Continue acknowledgement is pending | The pending command resolves from EXITED metadata; teardown proceeds once. |
| Late/stale Continue after EXITED | Generation-safe terminal completion or rejection; no state mutation and no execution. |
| Duplicate Continue during/after exit | Idempotent bounded handling; it cannot re-enter the dead target or double-release the run. |

## Parts C–F — race matrix, completion ownership, and pending state

The authoritative logical completion owner is the debugger controller. The
backend and NativeElf service provide the synchronous command result and the
durable lifecycle snapshot. `RunController` owns the later release of the run
service handle. The target execution loop owns execution only; it does not own
the final debugger command acknowledgement.

On exit, the ownership sequence is:

```text
target returns
  -> NativeElf marks EXITED and target_complete
  -> an in-flight Continue maps to terminal metadata
  -> debugger controller applies EXITED and completes once
  -> ordinary run poll publishes/release-completes the terminal run
```

The standalone `RunControllerPoll(..., false)` path permits the Continue path
to observe terminal metadata without releasing the service handle in the same
call. The next ordinary lifecycle poll remains the single release owner. A
terminal pending result is stored with the session generation and consumed
once; a generation mismatch leaves it unavailable to a later launch.

The pending state audited in this phase was:

- `resumeTerminalPending` / `resumeTerminalGeneration`: completed-by-exit
  state, generation-scoped and consumed once;
- ordinary run-controller active/handle state: retained until lifecycle poll
  owns release;
- debugger `SingleStepPending`: set only when Continue did not already resolve
  terminally;
- NativeElf scheduler/runnable state: never re-entered once EXITED is
  authoritative.

No command remains marked as awaiting a target response after exit.

## Parts E and H — exit dominance and scheduler findings

The NativeElf `debug()` path now recognizes Continue-family commands while the
operation is terminal and returns `NativeElf target exited` with a ready
terminal snapshot. The same terminal mapping is performed after a scheduler
pump if the target returns during Continue handling.

Therefore EXITED dominates PAUSED, RUNNING, Continue, Pause, and POLL:

- Continue cannot make EXITED RUNNING;
- Continue cannot enqueue target execution after exit;
- Continue cannot wait for scheduler dispatch;
- Pause cannot reassert PAUSED;
- POLL cannot redispatch a dead target;
- terminal completion remains lifecycle-metadata-driven.

Scheduler work that was already in the operation is harmless after exit because
the target-complete and terminal state checks prevent a second execution. The
successful trace shows `TARGET_TO_OWNER state=...06 active=0` before the
terminal Continue resolution, so the repaired path does not depend on a
post-exit scheduler token.

## Part G — first versus second Continue

The first and second Continue commands use the same command-family semantics.
The second path is not a new debugger feature or a distinct execution mode.
It is structurally different only in timing: the fixture is close to normal
target return, so the scheduler pump can observe `TARGET_RETURNED` and transfer
ownership to the terminal lifecycle during the Continue request.

The failure was therefore a race with target return, not a valid second
Continue requiring different breakpoint or step semantics. The repaired path
handles both the normal acknowledgement and the terminal result from the same
command boundary.

## Parts I and J — teardown and generation safety

The explicit legal ordering is:

1. Target returns.
2. NativeElf marks EXITED and `target_complete`.
3. In-flight Continue resolves from terminal state.
4. Debugger controller applies the terminal snapshot.
5. Normal lifecycle polling publishes terminal completion and releases the run.
6. Debugger/terminal teardown completes.

Launch/session identity checks remain in the NativeElf request path. Stop
generation remains part of the request identity for a live stop. The standalone
terminal completion is tagged with the controller session generation, and a
late completion from launch N cannot satisfy launch N+1. Relaunch starts a new
session generation and requires a new valid stop before Continue is accepted.

## Repairs

Standalone changes:

- `src/developer_studio_debugger_hosted.cpp`: detect the authoritative NativeElf
  exit result for both breakpoint Continue and user-pause Resume; retain a
  terminal completion token; consume it generation-safely and exactly once.
- `src/developer_studio_debugger.cpp`: apply terminal completion immediately
  after either Continue form resolves; do not leave breakpoint continuation in
  `SingleStepPending` after exit.
- `src/developer_studio_run.cpp/.h`: allow a terminal observation without
  releasing the run; preserve ordinary lifecycle release ownership.
- Added bounded Continue/exit traces around command entry, terminal mapping,
  consumption, controller completion, and pending state.

Server changes:

- `kernel/core/native_elf/native_elf_run_service.cpp`: add an early
  terminal-resolution path for Continue-family commands and terminal mapping
  after a Continue scheduler pump.
- Regenerated the tracked AMD64 Developer Studio artifact used by the QEMU
  fixture.

No new debugger panels, stepping modes, breakpoint types, expression features,
Watch features, or Data Tip features were added.

## Part K — deterministic regression tests

The focused tests model the race without sleeps or timing luck:

- Continue accepted at a breakpoint, followed immediately by target exit;
- terminal completion consumed exactly once;
- duplicate Continue after exit does not call the backend again;
- user-pause Continue completing by exit;
- terminal observation before ordinary run release;
- relaunch receives a new session generation and cannot consume the old
  terminal completion.

Validation completed:

```text
CTest:                                      31/31 passed
Developer Studio debugger model:            PASS
Developer Studio run controller:            PASS
Developer Studio bare DWARF capacity:      PASS, dies=397
```

The DWARF capacity binary was run with the large Windows test stack required
by this fixture; the result was `PASS, dies=397`.

## Part L — startup preservation

The Phase 28V startup initialization and stage 1–19 tracing remain intact.
The repaired fresh boot reached stage 19 before exercising the Continue/exit
race. No startup diagnostics were removed or weakened.

## Part N — hosted control

The real hosted Native ELF control was rerun with the rebuilt server and the
Continue variant. It passed the hosted desktop launch, project open, real
source breakpoint pause, source navigation, call stack, locals, F5 Continue /
single-step and breakpoint rebind, exact target identity publication, targeted
stop, durable EXITED teardown, window release, and clean Server exit. The
control ended with:

```text
Developer Studio Debugger Phase 3B end-to-end smoke PASS
hosted Server exits cleanly after the debugger proof (exit code 0)
```

This hosted smoke is a control for the live Native ELF lifecycle. It does not
run the Phase 28Q two-Pause/two-Continue acceptance sequence and did not
reproduce the QEMU timing race deterministically; therefore it is not counted
as a new 10/10 claim.

## Part O — fresh QEMU acceptance

The rebuilt AMD64 artifact was:

`D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\Apps\DeveloperStudio\bin\amd64\developerstudio.elf`

SHA-256:

`4B6D72316D8B7B544038E90E9A35F53505415D314C590CF8157267EAB2012D24`

One independent fresh boot completed the repaired flow. It reached stage 19,
completed both Continue operations, reached `DEVELOPER_STUDIO_PHASE28Q_PASS`,
returned from `gx_main`, published EXITED, completed terminal lifecycle, and
showed no pending Continue after exit. Evidence:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-1470e15291f84552b4fda884556b5d5a`

The required 10-boot run was then started with the rebuilt artifact. Boot 1
reached stage 19, first Continue, and second Pause, but failed before issuing
the second Continue:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-4160120f5e014a5fb90b50065bd2430c`

The decisive evidence is:

```text
DEBUG_POST_REFRESH_STOP_MAPPING_UNRESOLVED
DEBUG_POST_REFRESH_CALL_STACK_PARTIAL
PHASE28Q_SECOND_PAUSE_PASS
PHASE28Q_CAN_CONTINUE_FALSE
PHASE28Q_CONTINUE_REJECT_CONTEXT
PHASE28Q_FAIL_REASON_SECOND_CONTINUE
PHASE28Q_FAILURE
```

Because the command was rejected before the target-side Continue request,
this is not a reproduction of the repaired post-exit stall. It is an
independent paused-context/stop-refresh blocker. The 10/10 acceptance result
is therefore **not reached** and must not be classified as stable.

## Part P — 25-cycle stress

Not run. The Phase 28W rule permits the deferred 25-cycle stress only after
10/10 fresh-QEMU acceptance, which was not reached.

## Part R — hardware and remaining limitations

Physical hardware was not tested. No hardware validation is claimed.

Remaining acceptance work is the independent paused-context refresh failure
that can reject the second Continue before command dispatch. The targeted
post-exit Continue lifecycle defect is repaired and covered by deterministic
tests plus the successful fresh QEMU trace.
