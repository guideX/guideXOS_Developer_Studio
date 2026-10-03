# Developer Studio Phase 29S — Post-Teardown Server Access-Violation Localization

## Outcome

**Outcome C — the historical code-0 Server access-violation exit did not reproduce in the completed hosted runs, and no production defect was established.** No production files were changed. This is a non-reproduction result, not proof that every shutdown lifetime is safe.

The dedicated code-0 repeat gate stopped at **4/5** because iteration 5 failed before a project or target was launched. One separate clean reproduction run completed, so five full code-0 smoke runs (ten target sessions) did complete successfully. The failed pre-target iteration means the requested 5/5 focused gate did not pass; the 25/25 code-0 stress gate was therefore not run.

## Repository lineage and preserved state

| Repository | Branch | Starting HEAD |
| --- | --- | --- |
| Standalone Developer Studio | `main` | `0e67b80b66cad67f24c0ab6846d46188bf805528` |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `bf78d517b513b624f61bee4ae6d144377f6c8000` |

The Phase 29R ledger recorded the standalone branch as 2 ahead and the Server branch as 1 ahead. At Phase 29S start, the actual local remote-tracking comparisons were `origin/main...HEAD = 0/0` and `origin/v0.5_DEVELOPER_STUDIO...HEAD = 0/0`. No fetch or ref change was made; the observed discrepancy is recorded rather than reconciled.

Pre-existing work was left untouched. The standalone checkout still has the unstaged `src/main.cpp` trace change and untracked Phase 29Q fixture/configuration entries: `tests/fixtures/debugger-phase15-h29-0927-phase29i-hosted-run-1/`, `tests/fixtures/debugger-phase15-h29-0927/`, plus the existing `.json` and `.bak` files under `debugger-phase15/`, `debugger-phase29q-positive/`, and `debugger-phase3b/`. The Server checkout still has modified `Apps/DeveloperStudio/app.json`, modified `Apps/DeveloperStudio/bin/amd64/developerstudio.elf`, and untracked `ESP/Apps/DeveloperStudio/.phase28q-diagnostic`.

Verified Server hashes after validation:

| Protected file | SHA-256 |
| --- | --- |
| `Apps/DeveloperStudio/app.json` | `5793567C54ABF22423A8FCDE2F9B32E5DEEB73981E070707BA11E7D23F390401` |
| `Apps/DeveloperStudio/bin/amd64/developerstudio.elf` | `106BD4D1C8827894AB6CE4850E5DC30E55E98A1BAA52BCA149027781B4081FE9` |
| `ESP/Apps/DeveloperStudio/.phase28q-diagnostic` | `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A` |

Only this report is Phase 29S work. No Server source/package file, pre-existing fixture, branch, remote, or authentication setting was modified.

## Phase 29R baseline and code-0 reproduction

Phase 29R separated a normal target return code from debugger/runtime failure. The prior code-0 run reached target exit code 0, terminal `Exited`, debugger teardown success, and the normal `guideXOSServer server exiting.` line. Its enclosing smoke process then surfaced `-1073741819` (`0xC0000005`). The Phase 29R record had no first-chance exception context, fault address, instruction pointer, access type, stack, or dump. That status alone did not identify a dereference or prove whether the fault was in the Server or its wrapper.

The original retained positive-GXSM code-0 fixture and smoke path were used without production changes:

- Staged Server: `C:\Users\guideX\AppData\Local\Temp\phase29r-hosted-stage-20261002`
- Build A, return code 0: `C:\Users\guideX\AppData\Local\Temp\phase29r-debugger-phase29q-positive-build-a-20261002`
- Dedicated repeat-gate evidence: `C:\Users\guideX\AppData\Local\Temp\phase29s-code0-focused-20261002`
- Clean reproduction log: `C:\Users\guideX\AppData\Local\Temp\phase29s-clean-repro-20261002\phase29s-code0-clean-repro.log`

The clean run completed two sessions. Both targets returned exactly 0, reached authoritative `Exited`, completed target/debugger cleanup, invalidated inspection/watch values, printed the normal Server shutdown marker, and the Server wrapper reported exit code 0. The observed sample identities were session generations 1 and 2, target generation 1 in each, with process/runtime IDs `12/2` and `13/3`. The GXSM watch evaluated while the target was live and became invalid after exit; relaunch used fresh identities.

The repeated gate then passed iterations 1 through 4. Iteration 5 stopped before `project_open=PASS`, before any target terminal markers, and before Server shutdown. Its failure log records `poll_event skipped event for an unowned window` while entering the project path. That iteration is a hosted UI/harness pre-target failure, not a code-0 target failure and not a crash reproduction. No retry was made.

Across the separate clean run and the four complete repeated iterations, five complete two-session smoke runs finished with target code 0 and Server exit code 0: ten target sessions total. The dedicated gate result remains 4/5. As required by the gate ordering, 25/25 code-0 stress was not started after the 5/5 gate failed.

The code-7 positive-GXSM regression passed 5/5 iterations (ten target sessions). Every target returned exactly 7, reached `Exited`, completed debugger teardown and watch invalidation, and its Server process exited 0. No code-0 versus code-7 teardown divergence appeared in the completed smoke traces or the inspected normal-return path.

## First access-violation evidence

| Requested crash datum | Phase 29S result |
| --- | --- |
| First exception code | Not captured. Historical `0xC0000005` is an exit status, not a recorded first-chance exception. |
| Faulting RIP/address | Not captured. |
| Read, write, or execute | Unknown. |
| Module, function, source line | Unknown. |
| Crashing thread and stack | Unknown; no valid crashing run existed. |
| Last function entered/returned before the access | Unknown for the historical event. In successful runs, the shutdown log call returns and `main` returns normally. |
| Dump | None. The attempted LocalDumps registry command was rejected by command policy; no registry state changed. |

Two GDB attempts during active target debugging were invalid for crash diagnosis: they intercepted the target's ordinary SIGTRAP at a debugger stop and disrupted the smoke before Server shutdown. They are excluded as reproduction evidence. The clean and repeated uninstrumented runs produced no `0xC0000005` exit. Consequently the report cannot identify the first invalid access or assign the historical event to a specific crash-timing class A–H.

## Post-shutdown call path and lifetime audit

In the Server `main` path (`server.cpp`), after the input loop:

1. `DevelopmentRunService::Shutdown()` runs.
2. `Lifecycle::shutdown()` stops the compositor, handles the console/scheduler shutdown path, and marks the lifecycle stopped.
3. `Logger::write(..., "guideXOSServer server exiting.")` prints and synchronously flushes the last normal marker.
4. `return 0` begins function exit. The local `ShutdownGuard` destructor runs and calls `DevelopmentRunService::Shutdown()` and `Lifecycle::shutdown()` a second time. `Lifecycle::shutdown()` returns immediately when already `Stopped`.
5. `main` returns to the C runtime, which performs remaining runtime/static cleanup before process termination.

The failing historical run did not capture which of these later steps completed. The last proven successful call in the passing path is the return from `Logger::write`; the entire `main`/CRT exit path then completes with process exit 0. No last-return-before-fault can be named without a faulting stack.

Source inspection found the following relevant ownership facts and remaining uncertainties:

- **Threads:** `Scheduler::shutdown()` requests stop and joins its owned scheduler threads. In contrast, `ProcessTable::spawn()` launches a `std::thread` and detaches it; `ProcessTable::terminate()` records a tombstone and erases the table entry but does not join that worker. The Native ELF smoke-close helper thread is joined before runtime dispatch ends. No live thread inventory was captured immediately before shutdown, so this source-level detached worker is an unresolved lifetime risk, not an established cause.
- **Debugger callback:** `NativeAppDebugger::RegisterRuntime()` installs `debugVectoredHandler` once using `std::call_once` / `AddVectoredExceptionHandler`. The inspected code has no matching `RemoveVectoredExceptionHandler`; the process-wide handler remains installed. It scans the static runtime table and returns `EXCEPTION_CONTINUE_SEARCH` when there is no active matching runtime. This callback/static-state lifetime is a candidate to revisit if a crash is captured, but no post-unregister callback or AV was observed.
- **Runtime teardown and handles:** the Native ELF executor joins its optional smoke-close thread, calls `EndHostCallDispatch`, clears `activeGxContext`, then calls `NativeAppDebugger::UnregisterRuntime` before runtime cleanup. Unregister restores runtime bindings, closes each non-null gate/trap/resume event, nulls the handles, and marks the static slot inactive. The runtime table stores copied values and owned debug metadata rather than a borrowed `NativeAppRuntimeContext*`. No handle misuse was observed; no crashing Windows process-handle call was captured.
- **Process worker/exit code:** the target's signed `int32_t` return is copied into process/runtime result state before normal completion is published. The `Process` destructor closes its stored Windows thread handle once when non-null. The detached worker and lack of a shutdown thread inventory prevent proving that every worker had exited at the final Server marker.
- **Terminal snapshot:** `DebugBackendSnapshot` carries copied numeric/value fields, including `int32_t exitCode` and independent `cleanupComplete`, plus bounded character arrays. It does not retain a borrowed runtime/process pointer. Stale terminal snapshots are scoped by session generation in the standalone controller.
- **Callbacks and inspection/watch:** the tested terminal path invalidates stop/frame/locals/watch inspection and temporary stepping state; GXSM watch reads occur before exit, and no post-exit reevaluation appeared in the traces. The controller/session cleanup passed. There is no observed late callback, but without the crash thread or a shutdown inventory its absence cannot be generalized beyond the tested runs.
- **Logger:** `Logger` owns static `g_lock` and `g_buf`; writes synchronously append and send to `std::cout` with `std::endl`. There is no explicit logger shutdown. The final log succeeds in every complete smoke run. Static destruction ordering remains unproven for a hypothetical later fault.

No code-0-only cleanup branch was found. Both ordinary codes 0 and 7 reach the same `Exited` lifecycle, preserve their signed application result, clean debugger state, and leave the enclosing Server result at 0.

## Validation ledger

| Gate | Result |
| --- | --- |
| Hosted code-0 clean smoke | PASS, 1/1 run, 2 targets; both code 0, teardown PASS, Server exit 0 |
| Hosted code-0 dedicated focused gate | 4/5 complete iterations PASS; iteration 5 failed before project/target launch; gate not passed |
| Hosted code-0 complete runs overall | 5 runs / 10 targets passed when counting the separate clean smoke plus the four completed gate iterations |
| Hosted code-0 stress | 0/25, not run because the required dedicated 5/5 gate did not pass |
| Hosted code-7 regression | PASS, 5/5 runs / 10 targets, exact code 7, Server exit 0 |
| Positive GXSM/watch behavior | PASS in the completed code-0 and code-7 runs: watch reads before exit, invalidation after exit, fresh relaunch identities |
| Debug CTest | PASS, 33/33 (serial) |
| Release CTest | PASS, 33/33 (serial) |
| DWARF capacity | PASS, 397 DIEs |
| Package audit | PASS for AMD64 and ARM64 |
| QEMU `Phase29LFullAcceptance` | PASS, runner exit 0 across 10 configured fresh boots; all 10 reached project-load-ready for `/P28Q`, all classified `NATIVE_LOADER_REACHED`; no `invalid_project_root` occurrence |
| QEMU 25-boot stress | Not run; no shared guest/controller code changed in Phase 29S |
| Physical hardware | Not used |

Package hashes from the temporary audited Server stage (`phase29r-hosted-stage-20261002`):

| Package | Size | SHA-256 |
| --- | ---: | --- |
| AMD64 Developer Studio | 1,106,332 bytes | `D2EEBE32DAA9FDF9119085063663B03393C8439A9648B1D8AAEBF0A51D0B90AB` |
| ARM64 Developer Studio | 1,266,204 bytes | `15C69A2F69E22C7C2FA83C269905A324DB13FFB470ED525C06D46D9CF6DD073F` |

The QEMU acceptance command was `scripts/smoke-compiler-bootstrap.ps1 -Phase29LFullAcceptance -BootCount 10 -TimeoutSeconds 120`. Its per-boot host trace retained ten fresh stages and ten `PROJECT_LOAD_READY` records with `path=/P28Q`; the historical Phase 29R `invalid_project_root` signature did not recur. The aggregate log is `C:\Users\guideX\AppData\Local\Temp\phase29s-qemu-full10-20261002.log` and the preserved evidence directory is `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-d5fc27d13bbf49459ca2487d82f4257d`. The failed Phase 29R boundary is therefore not the current blocker.

## Classification and next evidence needed

Primary classification: **J — non-reproducible historical crash**. No root cause and no repair are claimed. The process-wide vectored exception handler and detached `ProcessTable` worker are source-level lifetime risks only; assigning either as the cause would exceed the available evidence. No timing workaround or teardown change was made.

The remaining fault domain is the historical post-marker `0xC0000005`, if it recurs. A future diagnosis needs a valid first-chance native exception record or dump with instruction pointer, access type/address, thread, and stack. The independent QEMU project-root boundary passed its Phase 29S ten-boot rerun.
