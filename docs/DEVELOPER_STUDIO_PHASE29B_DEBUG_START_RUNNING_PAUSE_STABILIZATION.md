# guideXOS Developer Studio — Phase 29B Debug Start → Running / First-Pause Stabilization

## Result

Phase 29B is **Outcome B**. The debug-start → first execution → `RUNNING` → Pause → authoritative `STOPPED` lifecycle defect is repaired and proven. The remaining acceptance boundary is a pre-debug transactional project-load failure in the focused five-boot repetition attempt; it occurs before debugger start and is not a Phase 29B lifecycle failure.

## Repository baselines

| Repository | Branch | Starting HEAD | Ending HEAD |
|---|---|---|---|
| Standalone Developer Studio | `main` | `83e2290df48efc0aa22d968fd96012459a5c857f` | final Phase 29B commit reported below |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `b46bc53b013a95d26906964d59c4856d363cf55f` | final Phase 29B commit reported below |

No credentials, remotes, authentication configuration, branch history, or prior commits were changed.

## Phase 29A baseline and preserved evidence

Phase 29A correctly established authoritative stop ownership, module and target identity, project/artifact generations, DWARF state, source identity, and bounded runtime-helper → application call-site mapping. The baseline gates were CTest 31/31, DWARF `dies=397`, hosted debugger PASS, package audit PASS, and rebuilt AMD64/ARM64 packages.

The preserved Phase 29A focused evidence was:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-d40202afdede4395bf54f7688a5a49b6`

Boot 3 reached project readiness and debugger-start processing but did not reach the expected running/pause sequence. The last useful baseline event was `DEVELOPER_STUDIO_PHASE28V_EVENT_BUILD_DEBUG_WAITING_CLEARED`. The current investigation reproduced and localized this class of failure before changing the mapping path.

## Reproduction and localization

The first fresh reproduction showed that the target could in fact execute and pause, but Phase 29A mapping failed because the raw cooperative-pause helper RIP remained the frame-zero source candidate when it carried a plausible DWARF mapping. The repaired lifecycle then produced the complete Phase 29B service sequence, while the only remaining failure was the mapping marker.

The final package was also run once from a fresh QEMU boot. Evidence:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-c8dad105f8994c33967f2216e6ef2a24`

That boot contains `DEVELOPER_STUDIO_PHASE29A_STOP_MAPPING_PASS`, `DEVELOPER_STUDIO_PHASE28Q_FINAL_RESULT_PASS`, `DEVELOPER_STUDIO_PHASE28Q_CLEANUP_PASS`, and `DEVELOPER_STUDIO_PHASE28Q_PASS`.

## Start lifecycle and ownership

The production path is:

1. `requestDebug` publishes the build-backed debug-start intent through `g_debugWaitingForBuild`.
2. The build completion consumer consumes that intent and calls `beginDebugSession` exactly once for the current build result.
3. `beginDebugSession` constructs the target from the project generation and artifact, loads DWARF, maps breakpoints, and starts the debug controller.
4. The NativeElf service `prepare` registers the current session/target identity; `start` consumes the registered handle, publishes the target generation, allocates the execution owner, installs the trap, and performs the first scheduler dispatch.
5. The initial dispatch reaches the execution owner and returns at the debugger’s entry boundary. The service does not treat allocation alone as proof of execution.
6. NativeElf external release owns the first real execution dispatch. It publishes the durable startup-ready boundary only after that dispatch returns and the target is still non-terminal.
7. The controller then observes the current generation as `RUNNING`. A Pause request is accepted only against that generation, consumed by the service, and converted into a cooperative stop.
8. The owner captures the stopped register context, increments the stop generation, publishes the snapshot, and only then does the UI/controller inspect Phase 28X ownership and invoke Phase 29A mapping.

The bounded service trace uses `DEVELOPER_STUDIO_PHASE29B_SERVICE` records with session handle, target generation, stop generation, scheduler state, target-entry state, yield context, and completion state. The trace is capped at 128 records and does not log instructions.

## Command ownership and generation audit

The standalone build completion path owns the debug-start intent until the build result is consumed. The service owns the registered run handle and its generation-local execution state. `start` emits `START_COMMAND_CONSUMED`; target creation emits `TARGET_RUNNING_STATE_PUBLISHED` and `EXECUTION_OWNER_CREATED`; the first dispatch emits begin/return markers. Release and Pause operate on the same service handle and target generation.

The important invariant is that a command is not acknowledged merely because an object was allocated. The service records the current generation and does not publish startup-ready until the corresponding execution owner has crossed its first dispatch. Terminal states are checked immediately after that dispatch, so an immediate exit cannot strand the controller waiting for `RUNNING` or `STOPPED`.

The older app-level `g_debugWaitingForBuild` trace showed that its one-shot flag was cleared at the build-completion handoff before `beginDebugSession` returned. This was retained as an existing build-consumer protocol, not used as a timing workaround. The material Phase 29B defect was the server-side held first dispatch described below.

## Target, execution owner, and scheduler result

The target/process allocation and debugger attachment completed for the current project and artifact generation. The execution owner was created and reached `EXECUTION_OWNER_ENTERED`. The scheduler was active, the target was entered, and the first dispatch returned through the debugger boundary. This rules out “never made runnable,” “runnable but never scheduled,” and “scheduled but immediately blocked” for the repaired path.

The first-execution proof is the ordered service trace:

`START_COMMAND_CONSUMED` → `TARGET_RUNNING_STATE_PUBLISHED` → `EXECUTION_OWNER_CREATED` → `FIRST_DISPATCH_BEGIN` → `EXECUTION_OWNER_ENTERED` → `FIRST_DISPATCH_RETURNED`

After external release, the corresponding sequence is:

`EXECUTION_OWNER_RELEASED` → `FIRST_EXECUTION_DISPATCH_BEGIN` → `FIRST_EXECUTION_DISPATCH_RETURN`

`RUNNING` now means that the current target generation has a live execution owner, that owner has been released to execution, and the target has not entered `STOPPED`, `EXITED`, or `FAILED`. It is no longer an anticipation of execution based only on command acceptance or allocation.

## Pause lifecycle

The UI/controller generates Pause for the current session and target generation. The service records `PAUSE_COMMAND_CONSUMED`, preserves the existing Phase 28Q cooperative pause mechanism, captures the owner context, increments the stop generation, and emits `STOPPED_CONTEXT_CAPTURED` followed by `STOPPED_PUBLISHED`.

The final fresh boot proves two pauses, context capture, Phase 28X ownership, Phase 29A mapping, Continue/rebind, final result, and cleanup. Duplicate or already-stopped Pause behavior remains handled by the existing generation/state checks; no repeated Pause or start attempt was added.

## Root cause

The NativeElf start path initially marked the operation `RUNNING` while retaining the first scheduler dispatch in a held state. The controller could therefore observe a successful release boundary while the execution owner had not yet performed the first real execution dispatch. This split the externally visible lifecycle from the scheduler-owned lifecycle and made startup progress dependent on the next control request.

The independent Phase 29A symptom was that a cooperative stop retained the raw runtime-helper RIP. The ordinary frame-pointer unwinder could assign that address a plausible mapping, so the old source-only fallback did not replace frame zero with the already-authoritative application stop location.

## Exact repair

In `kernel/core/native_elf/native_elf_run_service.cpp`:

- added bounded Phase 29B service markers at command consumption, target publication, owner creation/entry/return, first dispatch, Pause consumption, context capture, and stopped publication;
- changed external release so it clears the startup dispatch hold, owns the first real scheduler pump, verifies a non-terminal return, and only then publishes `DEVELOPER_STUDIO_PHASE28V_NATIVE_STARTUP_READY`;
- added explicit immediate-exit/failure handling after the first dispatch.

In `src/developer_studio_debugger_stack.cpp`:

- preserved the raw register context and Phase 28X ownership data;
- for an authoritative user Pause with a validated current application location, rebound frame zero to the already-resolved stop location even when the helper RIP has a plausible mapping;
- performed only bounded symbol lookup for the rebound frame, with no guest timing or hot-path mapper call.

The stack regression fixture now supplies the authoritative resolved source location and verifies the source-bearing frame.

## Regression and validation results

- CTest: **31/31 passed**.
- DWARF capacity: **PASS, dies=397**.
- Hosted debugger smoke/control: **PASS**, including launch, breakpoint pause, source navigation, call stack, locals, Continue/rebind, exact target identity, teardown ordering, durable Exited state, and bounded diagnostics.
- Package/staging audit: **PASS** for `app.json`, sectionless ELF packaging, architecture, entry points, and absence of debug sections.
- Fresh final-package QEMU boot: **1/1 PASS**; Phase 29A mapping and Phase 29B lifecycle markers are present.

The focused repetition command was run with five fresh boots. Boots 1 and 2 passed. Boot 3 failed during transactional project loading with `P28Z PROJECT state=failed`, after `load_started` and before project ready/debugger launch. It produced no debugger-start or Phase 29B lifecycle sequence, so the focused gate is **2/5**, not 5/5. The failed boot is retained and counted; it was not discarded.

The full 10-boot acceptance gate was **not run** because the 5/5 prerequisite failed. The 25-cycle stress gate was **not run** because the 10/10 prerequisite was not met. No physical hardware validation was available; validation was hosted plus QEMU.

## Package state

Both final package architectures were rebuilt from the final source tree and passed audit:

| Package | Result | SHA-256 |
|---|---|---|
| AMD64 `Apps/DeveloperStudio/bin/amd64/developerstudio.elf` | PASS | `E62B505F1C1FD3DEC50AD700C5FD3C39848865C4438DB4E8331C2822A72DA516` |
| ARM64 `Apps/DeveloperStudio/bin/arm64/developerstudio.elf` | PASS | `123D2D9796318E8A0B756AEAF0F48777513615E981CF31DF71129B57FA67D7A2` |

## Final boundary

The central Phase 29B question is answered **yes** for the repaired debugger path: Developer Studio proves an uninterrupted, generation-correct start → runnable owner → actual execution → `RUNNING` → Pause → captured context → authoritative `STOPPED` sequence, with Phase 28X ownership and Phase 29A mapping intact.

Outcome remains **B** because the required focused repetition gate was broken by a separate pre-debug transactional project-load failure. That is the remaining fault-domain boundary; the evidence does not justify broadening Phase 29B into a timing change or another debugger lifecycle redesign.
