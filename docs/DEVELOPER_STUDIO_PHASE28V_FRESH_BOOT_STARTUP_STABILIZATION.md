# Developer Studio Phase 28V — Fresh-Boot Startup Stabilization

## Outcome

Phase 28V is classified as **Outcome B — startup defect repaired, another independent blocker remains**.

The original fresh-boot startup defect is isolated and repaired. The final candidate reaches the explicit debugger-start-ready state and the first NativeElf dispatch on every final trace captured before the later blocker. Complete Phase28Q fresh-boot acceptance was not reached because an independent post-startup terminal-resume path still stalls after the target returns during the second Continue.

No new debugger feature was added. Phase 28U exit semantics remain in place.

## Repository starting state

Standalone repository:

- Path: `D:\dev\guideXOS_Developer_Studio`
- Branch: `main`
- Starting HEAD: `08122ab9e195cb1714656cad200b9d5d74586b53`
- Starting worktree: clean at the beginning of Phase 28V work

Server integration repository:

- Path: `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO`
- Branch: `v0.5_DEVELOPER_STUDIO`
- Starting HEAD: `e3f95cc45dd69b23251cb32f0d1099e0a8542512`
- Starting worktree: clean at the beginning of Phase 28V work

Both repositories were still on the expected branches and starting HEADs when the Phase 28V changes were reviewed. The working changes include the cumulative Phase 28U repairs that were present in the supplied worktree.

## Phase 28U baseline preserved

The Phase 28U report was read before modification:

`docs\DEVELOPER_STUDIO_PHASE28U_NATIVEELF_TERMINAL_LIFECYCLE_STABILIZATION.md`

The Phase 28U baseline was:

- standalone fast tier: `31/31`;
- DWARF capacity regression: `PASS, dies=397`;
- final AMD64 artifact hash: `F549B6A3718555E19D169849FCEAAF96200C8A1C2423408B1C903DDEA086486C`;
- fresh final-artifact QEMU acceptance: `3/5`;
- failed boots stalled before NativeElf lifecycle execution;
- no post-exit nested NativeElf polling was observed.

The durable EXITED state, idempotent completion publication, generation checks, stale POLL/Pause/Continue rejection, lifecycle-metadata terminal consumption, and no-revival guarantees were not weakened.

## Startup trace model

The diagnostic sentinel `/Apps/DeveloperStudio/.phase28q-diagnostic` enables a bounded trace. The application publishes monotonically increasing startup stages:

1. application started;
2. project/fixture selected;
3. build artifact located;
4. launch request issued;
5. NativeElf executable validated;
6. NativeElf image loaded;
7. process object allocated;
8. launch generation assigned;
9. debugger session allocated;
10. debugger bound to process;
11. terminal session attached;
12. scheduler registration created;
13. initial target state published;
14. debugger-start request issued;
15. debugger-start completion published;
16. target ready for debugger commands;
17. first scheduler dispatch permitted;
18. target entry reached;
19. Phase28Q fixture execution begins.

Additional bounded events cover build/debug handoff, target construction, artifact identity, mapper sub-stages, controller startup, durable startup release, UI refresh, and post-startup Continue boundaries.

## Original failed trace and exact stall boundary

The original startup divergence is preserved at:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-804d39eb5eb440d5949ece3975f9478d\boot1.serial.log`

That boot reached the build result and stages 1–5. The mapper then emitted reset/parse/return diagnostics but did not emit mapper header validation, ELF validation, file/function/row parsing, or sort completion. The generic application diagnostic was:

`Debug launch skipped: debug artifact is unavailable or changed`

The same fixture artifact was successfully parsed in the comparison trace:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-600acc6c0c404b38a89ae648d5309340\boot1.serial.log`

with `output_bytes=10403` and `output_hash=fnv1a64:285E8C42C5212D02`. This made the failure a first-launch storage/readiness defect rather than an artifact-content difference.

Mapper sub-stages were then made explicit. Successful traces consistently show:

`FOOTER_RECOGNIZED -> HEADER_VALIDATED -> PAYLOAD_VALIDATED -> HEADER -> ELF -> FILES -> FUNCTIONS -> ROWS -> SORT_BEGIN -> SORT_END -> RETURN`

The failed original trace has no `HEADER` mapper sub-stage.

## Root cause and repair

The launch path used application-owned static mapper and artifact-capture storage before the first NativeElf debugger launch. The loader/runtime does not provide a sufficient contract that C++ static initialization has established the application’s first-use state. The mapper also had to distinguish a real prior reset publication from arbitrary scalar bytes. Depending on that state, the first fresh launch could reject or fail to parse an otherwise valid artifact before NativeElf process lifecycle execution began.

The repair is:

- explicitly clear the application-owned `DebugDwarfMapper` and artifact-capture buffer at `gx_main` first entry;
- keep these objects in static storage, not callback stack storage;
- use the mapper reset cookie to recognize a published mapper generation rather than trusting stale count/state bytes;
- retain the normal mapper reset at the artifact-generation load boundary, where the consumer is ready;
- avoid invoking the full table-clearing reset redundantly before stage 1, because that reset belongs at mapper load and a duplicate first-entry reset introduced an earlier pre-stage stall risk;
- retain generation/hash checks so a mapper can never accept a stale artifact for the active project generation.

The final source candidate’s AMD64 artifact is:

`D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\Apps\DeveloperStudio\bin\amd64\developerstudio.elf`

SHA-256:

`1889C1D230FA9E9FFD700C11C6C039DD2FB99C6324EB9BAAE45FB2AA96D80642`

The ESP-staged AMD64 copy has the same hash.

## Dependency graph findings

The observed real order is:

`application -> workspace/fixture -> build -> artifact identity -> symbols -> controller/process -> terminal/scheduler ownership -> durable startup release -> target-ready publication -> first dispatch -> fixture entry`

The startup gate is now explicit. For NativeElf, the manager owns the entry-stop handshake, materializes the current generation’s breakpoint state, and acknowledges the parked entry before the controller opens execution. The debugger-start completion is durable and generation checked; it is not inferred from a target event that could already have been consumed.

Hosted startup remains asynchronous because its runtime identity is published by the hosted run service. Hosted release is therefore deferred until the current process/runtime identity and session generation are visible.

No startup cycle requiring target execution to publish debugger readiness remains in the NativeElf path.

## Scheduler, event, and publication audits

- Process registration and launch generation are carried together through controller/backend state.
- The scheduler does not use a prior generation’s registration as the current launch’s readiness signal.
- Runnable/release state is durable in controller/backend state; it is not represented only by a one-shot POLL token.
- Build completion is a durable build-result predicate consumed by the debug request path.
- Debug-start completion is explicit and idempotent for the active session generation.
- Late completion from an older session/process/launch generation cannot satisfy a new launch.
- Process identity, executable identity, terminal identity, debugger identity, initial state, and command storage are initialized before readiness publication.
- UI refresh is downstream of the launch-ready transition and does not create readiness.
- The existing mapper reset-cookie regression test exercises a pristine mapper object with stale-looking bytes and verifies that a real load can establish Ready state.

## Fresh-boot evidence after repair

The final candidate was run through the Phase28Q-only fresh-boot harness. Evidence was preserved at:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-15d4841ac38e4750b5785f78645f508a`

Boots 1 and 2 reached all stages 1–19 and emitted `DEVELOPER_STUDIO_PHASE28Q_PASS`.

Boot 3 also reached all stages 1–19, including:

- `STAGE_15_DEBUG_START_COMPLETION_PUBLISHED`;
- `STAGE_16_TARGET_READY_FOR_DEBUG_COMMANDS`;
- `STAGE_17_FIRST_SCHEDULER_DISPATCH_PERMITTED`;
- `STAGE_18_TARGET_ENTRY_REACHED`;
- `STAGE_19_PHASE28Q_FIXTURE_EXECUTION_BEGINS`.

Its later trace reached the second Continue, the target returned with `gx_main returned 0`, and the Phase28U lifecycle trace recorded target completion. It stopped before the controller returned from the terminal-consuming second Continue. The last relevant trace was:

`PHASE28Q_RESUME_RESULT_MAP_COMPLETE -> PHASE28Q_RESUME_HOST_COMMAND_RETURN -> HOST_RUN_POLL_RETURN`

with no `CONTROLLER_CONTINUE_RETURN_TRUE` or `CONTINUE_SECOND_PASS` after that point. This is after startup and is independent of the original mapper/startup defect.

An earlier candidate’s boot 9 showed a different independent post-startup failure after the first pause, at the optional paused UI variable-refresh sequence; that trace is preserved at:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-584198c51b0d42e8bd8d61ba3ca48778\boot9.serial.log`

Because a later boot stalled after startup, the Phase28V final-artifact acceptance result is **not 10/10** and is not called stable.

## Hosted validation

The hosted diagnostic smoke previously completed successfully during this phase’s validation: project open, build, NativeElf process creation, explicit debug-start completion, real source-breakpoint pause, call-stack inspection, exact target identity, server exit 0, debugger teardown, and clean close. No `0xC0000005` was observed in those hosted traces.

A later final-index hosted rerun was interrupted after it produced no trace file/output beyond its initial checks, so it is not counted as a new successful final hosted run. The hosted ContinueBreakpoint mode remains blocked by the independent source-breakpoint mapping issue documented in the Phase28U/Phase28V working traces.

## Validation results

Passed:

- standalone fast tier: `31/31`;
- DWARF capacity regression: `PASS, dies=397`;
- AMD64 freestanding Developer Studio build;
- ARM64 freestanding Developer Studio build;
- final AMD64 artifact identity: `1889C1D230FA9E9FFD700C11C6C039DD2FB99C6324EB9BAAE45FB2AA96D80642`;
- Phase28U terminal/lifecycle repairs remained present in the final traces;
- no post-exit nested NativeElf polling was observed in the completed boots.

Not reached:

- required 10/10 final-artifact fresh-QEMU acceptance;
- deferred 25-cycle real lifecycle stress after 10/10 acceptance.

The previously attempted lifecycle stress reached 1/25 clean lifecycle completion, then stopped on the independent ordinary source-breakpoint mapping failure. It was not rerun after the final boot series because the prerequisite 10/10 acceptance was not reached.

## Physical hardware

No physical hardware was tested. This phase used hosted validation and QEMU only.

## Remaining limitations

The original pre-lifecycle fresh-boot startup nondeterminism is repaired and precisely observable through stage 19. The remaining blocker is later in the NativeElf terminal-resume/second-Continue path, after the target has entered, paused twice, and returned from `gx_main`. Source-breakpoint mapping also remains an independent hosted/stress limitation. Normal debugger feature development should remain deferred until those independent acceptance blockers are handled.
