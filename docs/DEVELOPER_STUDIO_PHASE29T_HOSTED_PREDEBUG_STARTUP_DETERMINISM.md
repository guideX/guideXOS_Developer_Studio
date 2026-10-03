# Developer Studio Phase 29T — Hosted Pre-Project Startup Determinism

**Outcome: B — the original startup/dispatch failure is explained and materially repaired; later hosted debugger gates remain incomplete.**

Date: 2026-10-03
Standalone branch: `main`
Server branch: `v0.5_DEVELOPER_STUDIO`

## Safety gate and preserved worktree state

No `.phase` marker, Phase 29T report, or later-phase history was present. `git log --all --grep=29T` found no completed 29T work. The prompt was valid. Starting HEADs were standalone `66d6a38bb138619d59ad21342e6782058c5c1662` and Server `bf78d517b513b624f61bee4ae6d144377f6c8000`; both branches were at `0 ahead / 0 behind` their upstreams.

The standalone `src/main.cpp` change and pre-existing fixture/configuration files were preserved and not staged. The Server `Apps/DeveloperStudio/app.json`, AMD64 ELF, and `.phase28q-diagnostic` sentinel were treated as protected. A hosted smoke run persisted its window in Server `desktop.json` and `desktop.state`; because those exact additions were absent from the starting state, only those two test-generated lines were removed. No protected package or sentinel was changed. The Server package hashes were checked again at close.

No Server source or package changes were made by Phase 29T. The only standalone changes are the two hosted smoke scripts and this report.

## Phase 29S baseline and recovered iteration 5

The Phase 29S baseline was: code-0 focused gate 4/5, code-7 gate 5/5, Debug and Release CTest 33/33, DWARF capacity 397 DIEs, package audit pass, and QEMU full acceptance 10/10. The 25-session code-0 stress gate had not started because focused iteration 5 failed before a project or target existed.

Retained iteration-5 smoke, child, failure, and Server logs were recovered. The last startup checkpoints present were Server start, Developer Studio app creation, controller readiness, window creation, first render, and event-loop entry. The first missing expected marker was:

`GUIDEXOS_DEVELOPER_STUDIO_MARKER project_open=PASS`

The historical owner listing contained two visible same-title windows. Window `1000` had `ownerPid=0`, `ownerName=<unknown>`, and unknown app ID; window `1001` was owned by PID `11` by `nativeelf:com.guidexos.developerstudio`. The old smoke hardcoded window `1000`. NativeAppHost logged input as skipped because that window was unowned. The project-path command therefore never reached the Developer Studio controller. The retained evidence identifies a harness/UI dispatch failure, **classification E**, rather than a fixture, root-validation, project-transaction, or target-exit failure.

## Startup sequence and repair

The smoke now follows this bounded sequence:

1. Create a previously absent, unique fixture root for the iteration.
2. Copy and hash only the six tracked Phase 29Q positive-fixture files; close each write before proceeding.
3. Validate project/application identity, display name, source root, manifest path, path limits, required files, and absence of generated files.
4. Start the hosted Server once; record PID, parent wrapper PID, executable, and process start time. Wait for the Server startup marker.
5. Wait for Developer Studio application construction, controller readiness, main-window creation, and first event-loop iteration markers.
6. Query `desktop.windows.owners`; require exactly one visible, app-owned Developer Studio window with a nonzero owner PID. Use its returned ID for all later input and close commands.
7. In a positive-GXSM run, click once in the main window body to deliver a fresh focus event, then wait for focused/owned key evidence. This addresses the observed startup focus race: the initial `MT_SetFocus` can arrive before the app has registered its new window as owned and is dropped. Activating an already compositor-focused window does not resend focus.
8. Send Ctrl+Shift+O once, wait for the real project-path prompt text, type the fixture root once, and send Enter once.
9. Record dispatch as sent only after the single input flow, and acceptance only after `project_open=PASS`.
10. Continue through build/debug readiness and bounded target lifecycle waits. On failure, request a graceful Server exit, wait, and only force-terminate the exact launched process tree if needed; record exit/reap status.

No fixed startup sleep, repeated open request, retry, repeated launch, or forever poll was added. The one body click is a single focus repair after owner discovery, not a repeated retry.

## Fixture, identity, and path evidence

The gate makes a new directory for every iteration and refuses to reuse an existing one. It copies only `CMakeLists.txt`, `README.md`, `app/app.json`, `build.ps1`, `guidexos.project`, and `src/main.cpp`. It does not copy debugger settings, build/bin directories, ELF/object files, or other generated output, and it does not rewrite identity. File counts and SHA-256 values are checked before launch; file handles are closed before Server/project-open begins.

For the formal code-0 iteration 001, the fixture root was:

`C:\Users\guideX\AppData\Local\Temp\debugger-phase29q-positive-29t-f631c14d27-001`

`projectId` and application `id` both equaled `com.example.debuggerphase29qpositive`; both display names were `Debugger Phase 29Q GXSM Fixture`. `guidexos.project`, `app/app.json`, and `src/main.cpp` existed; the source root and metadata path were valid; `generated_before=0`; all six copied-file hashes matched their tracked source. The source return value was zero. The harness normalized the absolute root with `GetFullPath`; the reported normalized root matched the supplied raw root byte-for-byte after canonicalization. Existence, metadata, and manifest checks were true.

Each run logs a harness correlation GUID. In the formal code-0 run it was `61ad820265184092b630ff9a0ffd15a8`. It is **not** the product request ID. The hosted Developer Studio build does not expose its internal project-open request ID, request generation, transaction ID, or normalized-path validator fields through the normal diagnostic route. Those values are therefore reported as not exposed, rather than inferred. `project_open=PASS` and subsequent symbol-backed debugger readiness establish that the request was accepted and the project/build reached a usable state. No `invalid_project_root` signature appeared in the hosted runs.

The separate QEMU Phase 29L full-acceptance regression does expose its internal transaction records. On all five boots it reached `/P28Q`, request `1`, generation `1`, transaction `1`, refresh `1`, project generation `1`, and `ready`. The Phase 29L ownership observer reported `CURRENT` while committed and `TRANSACTION_NOT_ACTIVE` after release. Manifest identity, schema, architecture, artifact path, and project ID matched. This is QEMU evidence for the Server project transaction path; it does not supply hidden IDs for the hosted UI request.

## Current hosted evidence and first later boundary

Two instrumented fresh code-0 startup runs and one formal code-0 iteration reached the Developer Studio path. The formal run used visible owned window `1000`, owner PID `11`, and app ID `com.guidexos.developerstudio`. Its single Ctrl+Shift+O path flow was sent once and accepted once. Raw and normalized root were the same temp fixture root. The target process/runtime were PID `12` / runtime ID `2`.

The formal run passed the real project path prompt, `project_open=PASS`, symbol-backed `debug_variables=PASS`, GXSM v2 validation (`present=1 valid=1 version=2`), live watch evaluation, Step Over, Step Out, and watch-edit markers. The target runtime then reported:

`Cleanup complete ... state=Exited exitCode=0 cleanedWindows=0 remainingWindows=0`

Developer Studio also reported `debug_state=EXITED`, the final Step Out temporary binding as `cleanup=1`, zero binding owners/refcount, and `debug_transition=Running->Exited`. However, the required `TARGET_EXIT_NORMAL code=0` marker was not emitted. That was the first missing expected marker after the debugger lifecycle. `debugger_teardown=PASS` and explicit watch-runtime invalidation markers were also absent. The smoke stopped at the first missing marker, performed `smoke_failure_cleanup`, and reaped Server PID `22808` (wrapper PID `43584`, wrapper exit 0, no forced termination). This is **classification K: an independent later debugger/observability boundary**. The raw runtime/controller evidence supports a normal target exit and cleanup, but does not satisfy the gate’s explicit target-exit, teardown, and watch-invalidation marker requirements.

The focused code-0 gate stopped at iteration 001: **0/5 complete acceptance sessions**. There was no retry. The 25-session code-0 stress gate was not started because the focused prerequisite failed. The later run-wrapper classifier was corrected to avoid false “project not opened/ready” diagnoses when a child fails before end-of-smoke summary text; it now uses the already observed accepted dispatch and `debug_variables=PASS` readiness evidence. The formal code-0 failure consequently names only the real later boundary: no `TARGET_EXIT_NORMAL` or teardown marker.

## Code-7, GXSM/watch, and stepping results

The code-7 gate used a separate isolated six-file fixture with the target return changed to `7`, preserving its project/application identity and valid paths. Iteration 001 reached GXSM v2 validation, the live `counter == 2` watch, Step Over/Out, and Continue. Runtime reported `state=Exited exitCode=7`; Developer Studio reported `debug_state=EXITED`, cleared the Step Out binding, zero refcount, and `Running->Exited`. The explicit `TARGET_EXIT_NORMAL code=7` and `debugger_teardown=PASS` markers were absent. The no-retry gate stopped at iteration 001: **0/5 complete code-7 acceptance sessions**. The initial wrapper summary also mislabeled the already observed project-open/readiness evidence because it expected later summary prose; the wrapper classifier was corrected afterward. The actual first missing lifecycle marker remained the exact code-7 normal-exit marker.

The positive-GXSM code-0 run verifies parsing and live reads: GXSM v2 was accepted, the watch read `counter` from target memory at stops, and Step Over/Out ran. The target exit returned code 0. Because the terminal watch-invalidation marker is absent, Phase 29Q watch invalidation is **not fully proven** in this build. No claim of a complete GXSM/watch exit regression is made.

The dedicated Phase 15 stepping attempt used a fresh isolated copy of its seven tracked fixture files. Project open and build succeeded, but debug start stopped before variables/stepping. The host symbol trace reported `DWARF_LOAD` failure `malformed_dwarf`, `dies=413`, `truncated=1`, `result=Failed`; the Phase 15 ELF was 12,792 bytes with SHA-256 `abb92ae21f1af8fcdd6eead3a389bed424cfbe78b8d15b7ccdce598417bccd05`. The reported candidate originated from `C:\Program Files\LLVM\lib\clang\22\include\development_debug.h` and normalized to the fixture’s `src/freestanding_memory.cpp`. The dedicated run did not reach Step Into, Step Over, Step Out, or Continue. The representative GXSM run did reach Step Over, Step Out, and Continue, but that is not a substitute for the Phase 15 Step Into regression. This is a later independent debugger/symbol-capacity blocker; no speculative parser change was made.

## State isolation, process cleanup, and temporary resources

The hosted gate runs sequentially. Every iteration uses a new fixture root and unique trace artifact names; the positive gate refuses existing destination roots, records all copied-file hashes, verifies no generated files before launch, checks that fixture files can be opened exclusively after cleanup, checks that no matching Server process remains, and verifies debugger configuration was not left in the fresh fixture. Server PID and wrapper PID are logged and reaped before a next iteration could start. Existing tracked fixture debugger configuration files were not copied or changed.

The QEMU script would replace the protected sentinel in its source `ESP/Apps/DeveloperStudio` staging directory before restoring it. To avoid even a temporary overwrite of the protected worktree, the required QEMU regression ran in a disposable mirror under `%TEMP%`; the source sentinel hash matched before the test. The original Server worktree was not used for QEMU builds or staging. Each boot used a distinct temporary ESP stage; the audit recorded `host_mutations_after_audit=0`.

## Regression and validation ledger

| Check | Result |
|---|---|
| Recovered Phase 29S iteration 5 | Yes; first missing marker was `project_open=PASS` |
| Fresh hosted project-open diagnostic | Accepted once after owner query and one body-focus event |
| Formal hosted code-0 focused gate | Stopped at iteration 001; 0/5 complete, no retry |
| Hosted code-0 stress | 0/25; not started because focused gate failed |
| Hosted code-7 gate | Stopped at iteration 001; 0/5 complete, no retry |
| GXSM v2 and live watch | GXSM accepted and live values read; post-exit invalidation not proven |
| Dedicated stepping | Blocked before first step by 413-DIE truncated DWARF parse |
| Debug CTest | 33/33 passed, serial |
| Release CTest | 33/33 passed, serial |
| DWARF capacity | PASS, 397 DIEs |
| AMD64 package audit | PASS; SHA-256 `106BD4D1C8827894AB6CE4850E5DC30E55E98A1BAA52BCA149027781B4081FE9` |
| ARM64 package audit | PASS; SHA-256 `7F3C61A05A335B47C32B76C5F0769A6C6251F601171D4D01DA0737B926A40335` |
| QEMU Phase 29L full acceptance regression | 5/5 fresh boots passed; all reached project `ready` and `DEVELOPER_STUDIO_PHASE28Q_PASS` |
| QEMU `invalid_project_root` | Not observed in any of the five boots |
| Physical hardware | Not used |

Protected final hashes remained unchanged: `Apps/DeveloperStudio/app.json` `5793567C54ABF22423A8FCDE2F9B32E5DEEB73981E070707BA11E7D23F390401`; AMD64 ELF `106BD4D1C8827894AB6CE4850E5DC30E55E98A1BAA52BCA149027781B4081FE9`; sentinel `.phase28q-diagnostic` `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`. The smoke-generated `desktop.json` and `desktop.state` records were removed, restoring those files to their starting contents. The remaining Server worktree changes are the pre-existing protected package/app changes and sentinel; none were staged.

## Git closeout

Legitimate Phase 29T files to commit in the standalone repository are:

- `tests/smoke-developer-studio-debugger.ps1`
- `tests/smoke-developer-studio-debugger-positive-exit-gate.ps1`
- `docs/DEVELOPER_STUDIO_PHASE29T_HOSTED_PREDEBUG_STARTUP_DETERMINISM.md`

The standalone `src/main.cpp`, user fixture directories/configuration files, Server protected files, `desktop.json`, and `desktop.state` are excluded. The Server repository has no Phase 29T source change and receives no commit. See the task closeout for the local commit and push result.

## Final classification

The historical Phase 29S iteration-5 failure was a harness/UI dispatch defect (E): input targeted an unowned stale window and project open was never issued. Dynamic owner discovery plus one state-backed focus event made project-open deterministic and reached project/build-backed debugging. The new first downstream boundary is the missing hosted normal-exit/teardown/watch-invalidation marker set; separately, the Phase 15 stepping fixture exceeds the current DWARF parser’s observed 397-DIE boundary. The required focused, stress, code-7, and full stepping acceptance gates therefore remain incomplete. **Phase 29T Outcome B.**
