# Developer Studio Phase 29U — Hosted Terminal and Teardown Observability

**Status:** all hosted terminal and teardown gates passed for code 0 and code 7; Debug and Release CTest each passed 33/33, the 397-DIE capacity gate passed, and a clean fresh QEMU acceptance run passed 5/5. The first QEMU run's boot-5 `invalid_project_root` failure was retained and did not reproduce in the clean repeat. Git closeout remains.

## Scope and starting state

Phase 29U was the next phase. Neither repository contained an authoritative `.phase` marker, Phase 29U report, or Phase 29U history. Existing documentation ended at Phase 29T. No marker was created.

Starting revisions:

| Repository | Branch | Starting HEAD | Ahead/behind |
|---|---|---|---|
| Standalone Developer Studio | `main` | `b17e2e9d354b93ef0e373027c7035b936abc0ca6` | 2/0 |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `bf78d517b513b624f61bee4ae6d144377f6c8000` | 0/0 |

The Phase 29T state was preserved. The standalone `src/main.cpp` already had an unrelated 25-line GXSM trace change, and the listed fixture/configuration paths were untracked. Those paths are excluded from Phase 29U staging. In the Server repository, `Apps/DeveloperStudio/app.json`, the AMD64 package ELF, and `ESP/Apps/DeveloperStudio/.phase28q-diagnostic` were protected pre-existing changes. The protected hashes at task start were:

| Server path | SHA-256 |
|---|---|
| `Apps/DeveloperStudio/app.json` | `5793567C54ABF22423A8FCDE2F9B32E5DEEB73981E070707BA11E7D23F390401` |
| `Apps/DeveloperStudio/bin/amd64/developerstudio.elf` | `106BD4D1C8827894AB6CE4850E5DC30E55E98A1BAA52BCA149027781B4081FE9` |
| `ESP/Apps/DeveloperStudio/.phase28q-diagnostic` | `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A` |

The smoke's `desktop.json` and `desktop.state` were captured before the first current run and matched their starting bytes after each completed run. They must match again at closeout. No Server file was changed for Phase 29U.

Phase 29T's startup repair remains intact: fresh fixture roots, current owner-window discovery, one project-open flow, bounded prompt/acceptance waits, and tracked Server process cleanup. The Phase 29T QEMU baseline was 5/5 fresh boots to project-ready and `DEVELOPER_STUDIO_PHASE28Q_PASS`, with no `invalid_project_root`.

## Phase 29T failed traces

The retained formal traces were read before any Phase 29U edit.

| Code | Child trace SHA-256 | Failure trace SHA-256 | Runner trace SHA-256 | Runtime identity |
|---|---|---|---|---|
| 0 | `B807E2560D27C0F32A4C91284E5A78F1709F299CF7DF79560C8A35B5B8A084AB` | `A5766BB836349FA5BCB4A05849EAD886105FA252E1EC961080C66D02B616455B` | `3923EC380314A257C2F13E7E992F7E290413DF147CB9D71344714EBBA77EE7C4` | session 1, process 12, runtime 2 |
| 7 | `68772C6A576B4EDB6602216B78A307878784BA4A9ABCCAFA90C8CDEFA03D1C7B` | `9A5AF596B3A6071CA73036BA770DE1699E4F014074FA5B619C276971E10F8375` | `77642EA6548F18241B905F1456133176AEB104C6FFB3EDD41C9CE43CBB88388` | session 1, process 12, runtime 2 |

The code-0 trace ended the line-10 Step Out at stop generation 8, accepted Continue, and then showed:

1. `[NativeAppRuntime] Cleanup begin ... runtimeId=2 processId=12 ownedWindows=0`, followed by `Cleanup complete ... state=Exited exitCode=0 cleanedWindows=0 remainingWindows=0`.
2. `Native app execution completed ... exitCode=0`.
3. Developer Studio `debug_state=EXITED` and `debug_session=1 process=12 runtime=2 ... state=Exited stop=Exited stop_gen=0`.
4. `debug_transition=Running->Exited sequence=9`.
5. Smoke failure cleanup and `guideXOSServer server exiting.`

The code-7 trace had the same sequence and identity, with runtime `exitCode=7`. Both traces showed the Step Out binding cleanup (`owners=0 refcount=0 user=0 internal=0 installed=FALSE`) before the terminal transition. Neither trace showed `TARGET_EXIT_NORMAL`, `debugger_teardown=PASS`, `debug_watch_runtime=invalidated`, target/session teardown stage markers, or window release. The first missing marker was the expected `TARGET_EXIT_NORMAL code=N` after the generic `debug_state=EXITED`. The old package did not expose the terminal snapshot's exact code, target generation, or cleanup-complete bit through its Developer Studio markers; the runtime cleanup line did expose the exact code. Target generation was therefore unexposed in those traces. The zero-window counts show no app-owned NativeApp windows remained to destroy; the Server's generic exit line is process shutdown, not proof of debugger resource cleanup.

Static audit identified an artifact mismatch. The protected Server package ELF (`106BD4...`) contains generic `debug_state=EXITED` and transition/teardown strings but lacks all four current lifecycle strings: `TARGET_EXIT_NORMAL code=`, `debugger_teardown=PASS`, `debug_inspection=invalidated`, and `debug_watch_runtime=invalidated`. It is an older Phase 29Q Developer Studio package. The retained Phase 29R isolated Server stage contains the current compatible app ELF (`D2EEBE32DAA9FDF9119085063663B03393C8439A9648B1D8AAEBF0A51D0B90AB`) with all four strings. Thus the Phase 29T missing-marker observation did not establish a current production publication defect; those formal runs launched a stale package against a smoke expecting the newer contract.

## Runtime-to-marker path and contracts

The authoritative current path is:

1. The NativeElf target returns. `finish_execution` in the Server's `kernel/core/native_elf/native_elf_run_service.cpp` restores user and temporary breakpoints, uninstalls the debugger trap, clears the execution identity, and records the runtime teardown result. It unregisters the temporary app registration and sets `cleanupComplete` only when both runtime teardown and app unregistration succeed. The run service retains its durable `Exited` record until owner polling/release.
2. The Server's `hostRunPoll` copies the run snapshot (state, process/runtime identity, exact exit code, and `cleanupComplete`) into `RunResult`.
3. `HostedDebugBackend::snapshotFromRun` maps `RunState::Exited`/`Completed` to `DebugSessionState::Exited`, and copies the exact code, process/runtime IDs, and cleanup bit. It also handles an already-terminal run snapshot without repumping an exited target.
4. `DebugControllerPoll` accepts snapshots only for its active session generation and applies the terminal snapshot. Phase 29R's controller tests cover exact exit-code retention, once-only terminal events, duplicate snapshots, and rejection of stale prior-generation snapshots. The resulting controller state is `Exited`, with `active=false` and `cleanupComplete=true` on success.
5. `pollDebug` in standalone `src/main.cpp` is the sole producer of `TARGET_EXIT_NORMAL code=N`. It emits the marker only for controller state `Exited` not caused by user-requested termination. The marker means the current debugger session accepted an ordinary target exit and publishes its exact application return code and session/target/process/runtime identity. It does **not** by itself mean cleanup passed.
6. The same state-transition observer emits `debugger_teardown=PASS` only when the accepted terminal snapshot has `cleanupComplete`; otherwise it emits `INCOMPLETE`. This marker reports target runtime/debug resources and registration cleanup, not Server-process exit and not destruction of the Developer Studio window.
7. `debug_state=EXITED` and the `debug_session=... state=Exited` lifecycle record are emitted from the same `pollDebug` controller-state observer. In the Phase 29T trace, the older package emitted the generic state but did not contain the newer semantic marker producer. Current evidence shows both in the same completed terminal observer path.
8. On Continue, the inspection marker invalidates the old stopped context (`context_valid=0`, `stack_valid=0`, `locals_valid=0`, `watches_stale=1`). When the terminal snapshot makes the controller inactive, the UI terminal reset emits `debug_watch_runtime=invalidated session_gen=... watch_count=...`, then clears runtime frame, stack, locals, watch results, selected frame, and stop-generation state. Logical watch expressions and persistent breakpoint/source configuration remain available for a later launch; runtime values and addresses do not.
9. Server shutdown follows the debugger evidence. It is separately proven by Server exit code 0, wrapper exit code 0, and `process_reaped=True` with no forced termination.

The normal marker order observed with the current package is: runtime cleanup/exit → `TARGET_EXIT_NORMAL code=N` → `debugger_teardown=PASS` → `debug_state=EXITED` / terminal snapshot record → watch-runtime invalidation and UI runtime reset → clean Server shutdown. Inspection invalidation is already published on Continue, before target return. No post-exit read or synthetic watch reevaluation is used.

The session itself reaches terminal `Exited` and is inactive while the UI retains a bounded terminal summary and persistent project configuration. `debug_session_teardown=PASS` is a different, UI-close lifecycle marker from `completeDebugShutdownIfReady`; it is emitted only when the user requests a targeted Developer Studio close. The hosted positive-exit gate does not close the UI to manufacture that marker. Natural target completion and Server shutdown are assessed from the inactive Exited controller, cleanup-complete snapshot, invalidated runtime inspection/watch state, and clean process exit. This distinction is retained in the result rather than conflating window closure with target cleanup.

## Trace transport and smoke repairs

There was no evidence of a production output flush or child-drain loss. The hosted smoke redirects Server stdout/stderr to files, polls those files while running, waits for the wrapper process to exit, then reads the complete files. The wrapper captures the child's complete PowerShell pipeline and waits for it before validating the final trace. Current compatible-package runs show terminal markers in the child trace and the final runner summary.

Two smoke-observation defects were corrected:

* The project-open assertion counted `nativeapp.debuglog` replay lines as additional direct product publications. The smoke now counts the direct `[NativeAppHost] ... log: ... project_open=PASS` record, and separately requires one dispatch, one acceptance, no rejection, and no `invalid_project_root`.
* The shutdown trace was capped at 65,536 characters by retaining its tail. Verbose replay output displaced the first target's lifecycle records, leaving only the second session's markers for the outer gate. The trace writer now places compact per-target terminal/inspection/watch records first and caps the prefix, preserving evidence for both sessions.

The first current-package focused attempt stopped on the project-open replay-count false failure: the product published one direct result, but `nativeapp.debuglog` later replayed it. No retry was made. A fresh iteration 006 after that parser repair passed the inner smoke, including both terminal sessions, but its outer gate still stopped: its bounded child trace contained only the second target's `TARGET_EXIT_NORMAL` and `debugger_teardown=PASS` (`target_exit_markers=1`, `debugger_teardown_markers=1`). No retry was made. That file confirmed the trace writer's tail truncation was the remaining gate failure; the next complete focused run used new fixture roots 011–015.

The formal gate now audits the selected manifest's AMD64 ELF before copying fixtures or starting Server. It records the package path, length, SHA-256, and presence of all four required marker strings, then fails immediately with a stale-package explanation if any are absent. This prevents another acceptance run against the protected Phase 29Q package by accident.

## Focused hosted code-0 gate

The gate used the retained isolated Phase 29R Server stage, not the protected Server worktree package. Its Developer Studio AMD64 ELF was 1,106,332 bytes, SHA-256 `D2EEBE32DAA9FDF9119085063663B03393C8439A9648B1D8AAEBF0A51D0B90AB`; all four lifecycle markers passed the package preflight. Every iteration copied six tracked positive-GXSM fixture files into a new temp root without rewriting identity. The source target returns 0.

The first post-fix formal attempt for iteration 006 proved the inner product lifecycle but failed because tail truncation lost the first terminal markers. It was stopped with no retry. The trace-writer repair was then made, and a new independent focused run used iterations 011–015 and new roots. The complete 5/5 result passed in 511.5 seconds:

| Iteration | Result | Target sessions | Target code | Server cleanup |
|---:|---|---:|---:|---|
| 011 | PASS | 2 | 0 | clean, reaped |
| 012 | PASS | 2 | 0 | clean, reaped |
| 013 | PASS | 2 | 0 | clean, reaped |
| 014 | PASS | 2 | 0 | clean, reaped |
| 015 | PASS | 2 | 0 | clean, reaped |

Every inner smoke passed project-open once, project/build/symbol readiness, GXSM v2 validation, live `counter == 2`, Step Over, Step Out, Continue invalidation, second-session fresh-memory read, exact runtime exit code, `debug_state=EXITED`, target cleanup, watch-runtime invalidation for both sessions, and clean Server exit. It reported two unique session/process/runtime identities and raw terminal marker counts reflecting the host log replay. All wrappers exited 0 and reaped their Server process without forced termination. No `invalid_project_root` was observed.

Across the 5 focused and 25 stress child traces, a post-run audit found exactly two direct product log publications per file for each of `TARGET_EXIT_NORMAL code=0`, `debugger_teardown=PASS`, and `debug_watch_runtime=invalidated` (60 direct instances of each over 30 fixture iterations). The larger “raw” counts printed by the smoke came from later `nativeapp.debuglog` replay, not repeated terminal observer execution.

**Code-0 25-iteration stress:** passed 25/25 iterations (50 target sessions), elapsed 2,213.8 seconds, trace root `C:\Users\guideX\AppData\Local\Temp\guidexos-phase29u-code0-stress-20261003`.

**Code-7 focused regression:** passed 5/5 iterations (10 target sessions), iterations 041–045, trace root `C:\Users\guideX\AppData\Local\Temp\guidexos-phase29u-code7-focused-20261003`. Each runner reported exactly two target sessions, each with exact application exit code 7, independent teardown success, terminal state, and runtime-watch invalidation. All five wrappers exited 0 and reaped the Server without forced termination. The source fixture was made from the same six positive-GXSM files as code 0; only the return value changed to 7. Its `src/main.cpp` SHA-256 is `07F191341D6F0A20C0F0A3F63CE4A813F4D8EF344FA488CD4059DEF8C45D9F98`; project and application identities were preserved.

A separate direct-publication audit of all five code-7 child traces found exactly two direct product records per trace for each of `TARGET_EXIT_NORMAL code=7`, `debugger_teardown=PASS`, `debug_watch_runtime=invalidated`, and `debug_inspection=invalidated` (10 direct instances of each marker across five fixture iterations). Higher raw counts in the runner summary include host debug-log replay. Representative retained trace hashes are: code-0 iteration 011 runner `C7ACEEEB0D81565E14676EE03F8289BD775C446DB35DCBAA9B8EB8D9DD61F38D`, child `1DB86D339CC70C67E780DC0C192A900B77E3A212350F11838FC76487C82F49C7`; code-7 iteration 041 runner `BFA44ABA7D537056BF4EF8288413C9599E8D2F5139F48D14659B7CDD928F8745`, child `625139D1E7CE46BB8DFBA06B86B82DEE9DB1A124DE1C5363B44188DCBFDF35A2`.

## Fresh QEMU acceptance

The first fresh Phase 29L full-acceptance run used the safe mirror at `C:\Users\guideX\AppData\Local\Temp\guidexos-phase29t-qemu-4dffc3ff\guideXOSServerV0.5_DEVELOPER_STUDIO`, with five new QEMU boot stages under `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-1baa4b4eec3a4e8abeb523c347588197`. Boots 1–4 passed the full Phase 28Q lifecycle and all Phase 29L project-load ownership checkpoints. Boot 5 reached the same `P28Z APP 06 project_ready` state and passed the ownership checkpoints, then its debug-start build reported `build_artifact_validation=SKIPPED reason=invalid_project_root`; it emitted `DEVELOPER_STUDIO_PHASE28Q_FAILURE` instead of the required `DEVELOPER_STUDIO_PHASE28Q_PASS`. The script failed with exit 1 after the fifth boot timed out waiting for the missing lifecycle markers. The preserved boot-5 serial trace is the evidence for this failure.

A clean five-boot repeat ran from a distinct fresh QEMU stage root, `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-57424de951c84c7f8eda84ec20907df9`. All five boots reached project-ready, passed all 33 Phase 29L ownership checkpoints, and emitted `DEVELOPER_STUDIO_PHASE28Q_PASS`; none emitted `DEVELOPER_STUDIO_PHASE28Q_FAILURE` or `invalid_project_root`. The repeat completed with exit 0. This clean 5/5 run satisfies the bounded acceptance gate, while the unexplained first-run boot-5 failure remains recorded as an intermittent, unreproduced startup/debug-build observation.

Boot-5 serial trace SHA-256 values: failed first run `D5A707B786FB89FA2FFF483C7CC29F9ACB7E8E64F6A53FC481D12AB8A4F806BC`; passing clean repeat `77950A680635F663BFE774EECF250B843FD9E6B9D0C3F12B6C562843A3B0768F`.

The Server mirror's `.phase28q-diagnostic` SHA-256 was `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A` before and after the failed run. The protected Server worktree was never used by QEMU.

## Independent Phase 15 boundary

The known Phase 15 fixture contains 413 DIEs. Its dedicated parser result is `malformed_dwarf`, `truncated=1`, before the requested Step Into. The global freestanding capacity fixture remains the separately proven 397-DIE path. Phase 29U does not change parser capacity or conflate the 413-DIE condition with the positive GXSM terminal gate.

## Validation, repository closeout, and outcome

Validation results before repository closeout:

| Check | Result |
|---|---|
| Hosted code-0 focused | PASS, 5/5 |
| Hosted code-0 stress | PASS, 25/25 (50 target sessions) |
| Hosted code-7 focused | PASS, 5/5 (10 target sessions, exact code 7) |
| GXSM watch and representative stepping | PASS in each completed code-0 iteration |
| Debug CTest | PASS, 33/33 serial tests |
| Release CTest | PASS, 33/33 serial tests |
| DWARF baseline | PASS, 397 DIEs (`tests/run-debug-symbols-capacity.ps1`) |
| AMD64/ARM64 package audit | PASS, read-only audit of the hosted Phase 29R stage; AMD64 1,106,332 bytes SHA-256 `D2EEBE32DAA9FDF9119085063663B03393C8439A9648B1D8AAEBF0A51D0B90AB`, ARM64 1,266,204 bytes SHA-256 `15C69A2F69E22C7C2FA83C269905A324DB13FFB470ED525C06D46D9CF6DD073F` |
| Fresh QEMU regression | PASS, clean full-acceptance repeat 5/5 (all 33 ownership checkpoints per boot). Initial run 4/5; boot 5's `invalid_project_root` debug-build failure did not reproduce. |
| Phase 15 fixture | 413 DIEs, `malformed_dwarf`, `truncated=1`; separate blocker |
| Physical hardware | Not used |

## Preservation and outcome

At closeout, the preservation manifest comparison found all 28 pre-existing files unchanged, excluding this new report. This includes the pre-existing 25-line standalone `src/main.cpp` change, whose starting/current SHA-256 remained `1B642D906074BD0982656EF78903A2F28D001CC96E56B11B1B1EC5E2884DCC74`. The saved desktop-state snapshots were `desktop.json` (1,048 bytes, SHA-256 `3350CEA62281120BD8CF9C9AAE7F240BB3A1A6427F7CBB1ABB5D3385009B24D8`) and `desktop.state` (64 bytes, SHA-256 `F93FD4143F26D7463E36C5059B1460379D7ACA4596823FAA78CF5E5C2FC666CC`); the smoke checked them against their starting bytes after its completed runs. The protected Server worktree remained unchanged at its start hashes, and the QEMU mirror's diagnostic sentinel retained SHA-256 `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A` after both QEMU attempts.

**Outcome B.** The hosted terminal, watch invalidation, exact-code-0/code-7, teardown, stress, CTest, package, DWARF, and final clean 5/5 QEMU gates pass. The initial QEMU attempt's boot-5 debug build still failed once with `invalid_project_root` after project-ready and ownership validation; the clean 5/5 repeat did not reproduce it, but no root cause was established. That intermittent first-run observation remains the bounded stability limitation. No production Server files were changed. Git closeout and the ordinary push result will be appended after the local commit attempt. The Phase 15 413-DIE fixture remains a separate malformed-input result and is not changed by this work.
