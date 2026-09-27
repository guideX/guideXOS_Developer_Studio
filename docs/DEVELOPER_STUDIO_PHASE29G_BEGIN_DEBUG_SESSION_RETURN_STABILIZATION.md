# Phase 29G — `beginDebugSession` Return-Path Stabilization

## Result

**Outcome B.** The historical missing return was traced to the final synchronous NativeElf host-log callback and repaired by making serial echo bounded. A completed focused run passed 5/5 fresh QEMU boots before the later close-request ownership trace was added. The final revalidation then passed two boots and failed on boot 3 before Server start with `DEBUG_START_BREAKPOINT_MAPPING_FAILED error=artifact_changed`. A separate full-scenario probe returned explicitly from symbol initialization with `malformed_dwarf`. The full 10-boot acceptance gate and 25-cycle stress gate therefore remain incomplete.

The repaired post-start path is:

`Server authoritative RUNNING` → `GX_OK` release reply → client controller `Running` → bounded readiness/status publication → `beginDebugSession` returns → production caller emits `BEGIN_DEBUG_SESSION_RETURN`.

## Repository baseline and preserved files

| Repository | Branch | Starting HEAD |
|---|---|---|
| Standalone Developer Studio | `main` | `f224bda9b74501ad64825283e632cddcfc7e903f` |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `19df79f5e54c32db3b1f4bb6f87db399dabdbc56` |

The Phase 29F notes said the standalone and server branches were respectively ahead 2 and ahead 1. At the Phase 29G checkout, both local upstream refs resolved to the supplied starting HEADs (`0/0`); no fetch, remote change, or authentication change was made.

At Phase 29G start the two pre-existing untracked Phase 3B debugger fixtures were present and were preserved:

- `tests/fixtures/debugger-phase3b/guidexos.debugger.json`
- `tests/fixtures/debugger-phase3b/guidexos.debugger.json.bak`

Both had SHA-256 `6A53C3A2E9B5F5CEA837F278FC7CBE0D4BD58542058AB7AAFAB7DA5BBEC972A5` at start and at final inspection. The hosted validation run also left an untracked Phase 15 JSON fixture pair (hash `9DEE80AC5715601268BAB67FC9595EAC12AC1B285EF336B064C3D917A75C0E82`); it was preserved and excluded from commits. A QEMU diagnostic sentinel created by the smoke script, `ESP/Apps/DeveloperStudio/.phase28q-diagnostic`, also remains untracked and is excluded from commits.

## Phase 29F evidence

The Phase 29F failed boot had a ready project, current project/build generations, a valid AMD64 artifact, registered debugger service, nonzero service handle/generation, and a valid start-command generation. The Server consumed the start/release commands, created its target and execution owner, dispatched and executed the target, and published `DEBUG_START_AUTHORITATIVE_RUNNING`. The API returned `GX_OK`.

The last client-visible line in the preserved failed boot was:

`NativeElf host log: Debug: launching dev.guidexos.phase28q`

The client did not emit `BEGIN_DEBUG_SESSION_RETURN` or the following `pollBuild` return marker. Since Phase 29F did not yet place an entry/return marker around the final host logger, the exact blocked CPU instruction was not captured. Mapping that final line to the production call graph identifies the last entered client operation as `beginDebugSession` → `logMarker("Debug: launching …")` → the NativeElf `gx_app_context.host.log` callback → `NativeElf::host_log` serial echo. The old implementation used `serial::puts`/`serial::putc`, whose UART-ready loops had no bound. The most specific likely wait was the CR/LF completion after the payload had already reached the serial log. This is a source-backed explanation of the observed boundary, not a pre-fix 29G instruction trace.

Two other Phase 29F observations remain separate: the historical `NativeElf: external_close_request id=000003E8` had no caller metadata, and one diagnostic boot later failed Phase 29A stop mapping. Neither was conflated with the missing return.

## Complete production call path

The path starts asynchronously at `requestDebug`: request ownership is captured, project documents are checked, and `beginDebugBuild` submits the build. `pollBuild` observes build success and calls `beginDebugSession`. The `BEGIN_DEBUG_SESSION_RETURN` marker is emitted by `pollBuild` immediately after the real production call returns, using that call's actual boolean result; it is not synthesized by a test wrapper.

### Before the Server start API

1. `BEGIN_ENTRY` records the app/request/project/build/session/controller identity.
2. Project readiness, request ownership, project identity/root, build operation, artifact validity, artifact path, and architecture are checked. Invalid or stale input returns `false` with a Phase 29F result code.
3. `DebugTargetFromBuild` constructs the target from the current project and build result.
4. `loadDebugSymbolsForTarget` joins the artifact path, calls `fsStat` and `fsRead`, verifies the SHA-256, and calls `DebugDwarfMapperLoad`. The mapper parses bounded ELF/DWARF/source tables and normalizes source candidates against the project root. It can call the progress callback, which only emits bounded trace messages; it does not wait for another frame or event.
5. Per-session client runtime state is reset. The client publishes project context and copies the target to the controller.
6. Legacy workspace materialization runs only for legacy hosted sessions. NativeElf manager sessions skip that legacy path.
7. `DebugControllerMapBreakpoints` maps the local source breakpoint table. Enabled unmapped breakpoints produce an explicit pre-start failure.
8. The controller is configured for deferred release for hosted sessions and manager-owned release for the bare-metal NativeElf path.

### In and immediately after `DebugControllerStart`

`START_API_CALL` brackets `DebugControllerStart(&g_debugController, g_debugBackend, target, &error)`. The backend launch call synchronously enters the Server start API. In the successful NativeElf path it registers the service operation and target/execution owner and returns `GX_OK`; the client controller is `Launching`. First dispatch/execution and the durable RUNNING publication are preserved in the Phase 29B release ordering described below.

If `DebugControllerStart` rejects the request, the function reports its specific controller error and returns `false`. Once it returns `true`, `POST_START_STATE_BEGIN` is emitted. The function snapshots the handoff, records the client session generation, clears stop generation, and resets per-session runtime state.

For the manager-owned path, `debuggerWorkspaceMaterialize` then registers executable/module state, refreshes the module/breakpoint snapshot, and applies persisted breakpoint commands. Each manager command is synchronous and generation-tagged. A valid-but-unresolved source row is recorded as unresolved; command/service failures return an explicit handshake failure. The workspace materializer does not await a future breakpoint event.

### Release, RUNNING, and return

`debugUiReleaseExecution` creates a `GX_DEVELOPMENT_DEBUG_RELEASE_EXECUTION` request and calls `debugUiCallGeneral` synchronously. The Server consumes that request, releases the execution owner, performs first execution dispatch, and publishes authoritative RUNNING only after the Phase 29B first-dispatch/first-execution invariant has been satisfied. The Server replies `GX_OK`.

The client then calls `DebugControllerAcceptExternalExecutionRelease` with the current session generation. It transitions `Launching` → `Running` synchronously. Readiness/event/shutdown bookkeeping is initialized, the output panel receives `Debug: launching …`, the host log is called, `PRE_RETURN` is emitted, and `beginDebugSession` executes `return true`. `pollBuild` immediately emits `BEGIN_DEBUG_SESSION_RETURN result=success|failure` from the returned value.

No Server scheduling code or Phase 29B ordering was changed.

## Post-start synchronous-operation audit

| Operation | Purpose / inputs / ownership | Bound and wait behavior | Callbacks, filesystem, locks |
|---|---|---|---|
| `DebugControllerStart` / Server start API | Admit the current `DebugTarget`, project/build identity, backend service, and new session generation. | Synchronous API result; returns accepted or a specific `DebugErrorCode`. It does not wait for `pollBuild` or a later UI event. | Direct service call; no re-entry into a Developer Studio event callback. No client lock is held. |
| Controller/session reset and publication | Clear prior runtime state; publish target, project context, session generation, and stop generation. | Fixed-size in-memory state and bounded copies. | No filesystem access or callback dispatch. No explicit lock. |
| `debuggerWorkspaceMaterialize` | Register module list and materialize current generation's persisted breakpoint plan. | Fixed-capacity breakpoint plan; bounded synchronous manager commands; each command returns installed/pending, unresolved, or failure. No wait for future breakpoint delivery. | Calls Server manager APIs synchronously, not Studio callbacks. Reads current project documents from stable controller state; no filesystem read in this phase. No lock held. |
| `debugUiReleaseExecution` / `debugUiCallGeneral` | Send generation-tagged release request to the Server. | One synchronous API/scheduler handoff; `GX_OK` or explicit failure. It does not poll until an event arrives. First dispatch and first execution happen before RUNNING publication. | Server callback is direct; it does not call back into `pollBuild`, source navigation, or the UI event loop. No lock held. |
| `DebugControllerAcceptExternalExecutionRelease` | Validate session generation and publish local `Running`. | Fixed-size controller update; immediate boolean success/error. | No filesystem access or callback invocation. No lock held. |
| Readiness/session/status publication | Reset bounded event/session fields and publish output-panel status. | Fixed arrays and bounded output recording. | No source navigation/window refresh or event wait. No lock held. |
| NativeElf host log | Retain output and echo the final status to COM1. | Accepted log storage is bounded. Serial echo now shares a 65,536-probe budget for the entire line; on exhaustion it sets `hostLogSerialTruncated` and returns `GX_OK`. | No filesystem access or re-entrant callback. No lock held. |
| Phase 29G trace markers | Record last entered/returned stage, frame, app/request/project/build/session generations, controller state, and service handle. | Maximum 96 boundary records per session; each marker uses the bounded host logger. | Does not run a UI callback or acquire a lock. |

The bare-metal client/controller path has no mutex or spinlock held across the tail. The audited controller, symbol, and workspace code uses the single-threaded event-loop model and fixed global state; no lock acquisition occurs in the post-start path. The transient breakpoint materialization plan is stack-owned only for the synchronous manager calls that consume it. The launch target is static, the controller copies it, the build result/project/source mapper are static controller-owned state, and Server operation state is static and generation-checked. No stack-temporary request or project object is retained after the synchronous call.

### Event-loop and reentrancy result

`beginDebugSession` itself is called from `pollBuild`, so the event loop is paused while it runs. The audited path does not wait for work that needs that event loop: no synchronous wait for `pollBuild`, a later frame, debugger event polling, source navigation, window update, breakpoint callback processing, or project callback dispatch exists in this tail. The release service performs its required first dispatch inline and returns a status. Host logging only stores/echoes text and does not invoke Developer Studio callbacks.

There is no target-close or teardown callback in the successful return path. Controller start and manager commands validate the active project/session generations; external release accepts only the current session generation; Server commands validate service and target identity. Re-entry into start/build/poll/teardown was not observed. The repeated QEMU gate exercised only single-session launches; a dedicated reentrancy matrix was not completed.

## Source roots, symbols, and breakpoints

Source-root association is inside `beginDebugSession`, in the pre-start symbol initialization call. The diagnostic records project generation, source root, build root, compilation directory, first candidate, normalized path, containment result, lookup result, and mapper error.

Successful focused-run example:

`project_generation=1 source_root=src build_root=/P28Q compilation_directory=- candidate=src/helper.cpp normalized=src/helper.cpp normalization=contained lookup=success error=none`

On the final one-boot full-scenario probe the mapper returned `malformed_dwarf` before it encountered a source candidate (`candidate=-`, `normalization=not_run`). `beginDebugSession` emitted `SYMBOL_INITIALIZATION_RETURN result=DEBUG_START_SYMBOL_ARTIFACT_LOAD_FAILED`, then the production caller emitted `BEGIN_DEBUG_SESSION_RETURN result=failure`. Thus symbol failure is bounded and explicit; it does not leave the client waiting. Source lookup failure is classified separately as `DEBUG_START_SYMBOL_SOURCE_NOT_FOUND`.

The synchronous parser reads the artifact once, checks the captured artifact hash, then parses within `kDebugMapperMaxElfBytes` and fixed mapper table capacities. The capacity test passed with `dies=397`. Missing or malformed symbols fail before Server start; symbols are not loaded by waiting for a target event.

Controller breakpoint mapping happens before `DebugControllerStart`; the manager then applies the session's persisted breakpoint plan after start but before release. Installed, pending, unresolved, invalid, and failed outcomes are distinguished. A breakpoint that is not hit after successful return is independent of the return-path contract. The earlier completed 5-boot focused run reached its first stop mapping in every boot: four `DEVELOPER_STUDIO_PHASE29A_STOP_MAPPING_PASS`, one `...FAIL`. That one mapping failure followed a successful Phase 29G return and is classified as the independent Phase 29A blocker.

Client diagnostics showed `g_controller.model.projectGeneration` as `0` on some successful boots and `1` on others; the associated captured intent generation matched the model value at function entry. The Server target generation remained `1`. The focused harness proves readiness, admission, return, and later stop behavior but does not require the client model-generation field to be nonzero. This is recorded as a generation-contract follow-up and was not changed in Phase 29G.

## Controller and return contract

The client controller uses `Launching` for the transitional state; it has no separate `Starting` enum. NativeElf's `DebugControllerStart` acceptance publishes `Launching`; after Server release returns `GX_OK`, `DebugControllerAcceptExternalExecutionRelease` publishes `Running`. Later debugger polls can move the session to `Paused`, `Exited`, or an explicit failure/cleanup state.

For bare-metal NativeElf, `beginDebugSession == true` means the Server accepted the request, the entry-stop/module and breakpoint setup completed sufficiently to release, the Server returned authoritative RUNNING, the current session was accepted by the controller, and synchronous client launch/status bookkeeping completed. It does not promise that a user breakpoint has already been hit or that a later stop has been source-mapped. The return marker records project/build/session generation, controller state, and service handle; Server lifecycle markers correlate that handle to the target generation.

For hosted sessions, `true` means launch admission and synchronous client setup completed; the controller may still be `Launching` while runtime identity and the later initial stop are published asynchronously. That is pending readiness, not a synchronous future-event guarantee. `false` is paired with a specific Phase 29F failure code/error. The production return marker records the result and current controller/session/target identity.

## Repair

`NativeElf::host_log` previously wrote to COM1 with `serial::puts` and `serial::putc`, each of which can spin forever while UART transmitter-ready is false. It now stores the accepted host message first, then uses a best-effort echo with a shared 65,536 status-probe budget. If the budget is exhausted, the stored message still remains available, the diagnostic flag reports serial truncation, and the synchronous host API returns `GX_OK`. This removes the UART transmit state from the Developer Studio return contract without adding a sleep, timeout, retry, or scheduler yield.

The marker helper now uses a dedicated `g_phase29gUiStatusMessage` buffer so writing a boundary marker cannot overwrite the final status text before it is passed to the output service and host logger.

The Server bootstrap smoke's host-log proof now also requires `hostLogSerialTruncated == false` on the healthy-QEMU run. The smoke script exits after `DEVELOPER_STUDIO_PHASE28Q_PASS` so the idle VM does not consume its full timeout. This changes test harness completion only.

External NativeElf close requests now include origin, category, target generation, request generation, reason, lifecycle state, and window id. The standard GUI-automation close record carries the same ownership fields. No external close was observed in the Phase 29G focused runs; the historical request's origin therefore remains unknown.

## Tests and validation

### Regression coverage

- Added mapper tests for source-association diagnostic reset and successful candidate normalization.
- The NativeElf bootstrap smoke now treats serial truncation as a host-log proof failure.
- The production-path Phase 29G QEMU gate verifies Server RUNNING, client Running publication, `PRE_RETURN`, and the caller's real `BEGIN_DEBUG_SESSION_RETURN` in order.
- No separate synthetic lifecycle implementation was added to tests. The full matrix of stale-generation, callback re-entry, immediate teardown, and target-exit interleavings was not completed; those are not established by the passing serial-return run.

### Completed checks

| Check | Result |
|---|---|
| Complete CTest | 32/32 passed (run twice after the final standalone source fix) |
| DWARF capacity | PASS, `dies=397` |
| Standalone AMD64 and ARM64 builds | PASS; locals/arguments validation printed PASS |
| Package audit | PASS for sectionless AMD64 and ARM64 production ELFs; package contained `app.json` and both architecture binaries |
| AMD64 Developer Studio SHA-256 | `B59653AE06A6953FFDE1151CB92E065076D7372FD6591AAE17CE474D5E679D04` |
| ARM64 Developer Studio SHA-256 | `73C449E224CFFCBDA094B69C46375CA467C4B6D449A107C6AAE87BE816D1E44B` |
| Server AMD64 kernel compile after close diagnostics | PASS with `mingw32-make -C kernel ARCH=amd64` |
| PowerShell syntax | PASS for the updated QEMU harness and hosted required test script |

### Hosted validation

The hosted required tier stopped in its first `condition-editor` case after 118 seconds waiting for `GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_breakpoint=PENDING`. It emitted no debug-start or Phase 29G marker, so it did not exercise a session return. A three-session hosted return repetition was not supported by that failing setup path and was not claimed as passing. This was a pre-start breakpoint configuration failure, not an optional post-running breakpoint miss.

### Focused QEMU return gate

After the final standalone buffer fix, one completed fresh run passed 5/5: all five boots proved project ready, Server admission/target/owner, first dispatch and first execution, authoritative RUNNING, client post-start progress, `BEGIN_DEBUG_SESSION_RETURN`, client Running, and `DEVELOPER_STUDIO_PHASE28Q_RUNNING_PASS`. No external close request appeared. First Phase 29A mapping was 4/5 pass and 1/5 fail.

After the later Server close-ownership diagnostic change, a fresh 5-boot revalidation passed return and mapping on boots 1 and 2, then failed boot 3 before Server start. Its last successful Phase 29G operation was source initialization; `DebugControllerMapBreakpoints` returned `artifact_changed`, and `beginDebugSession` emitted an explicit failure return. Per the gate rule, the revalidation is failed at boot 3 (2 passed, 1 failed, 2 not run). The failed boot did not enter Server start, so it is a Phase 29F pre-start mapping regression, not a Server RUNNING or client post-start stall.

### Full acceptance and stress

After the earlier 5/5 gate passed, the full-scenario probe was started. Its first boot reached project ready, then `DebugDwarfMapperLoad` returned `malformed_dwarf`; no Server start/RUNNING or successful Phase 29G return occurred. The client recorded an explicit failure return. This breaks the full 10-boot gate at its first boot; 10/10 was not achieved. A previous interrupted full run had reached `PHASE28Q_PASS` on its first boot, but it did not complete the requested 10 boots and the existing Phase 28Q acceptance set reported `LOCALS_NOT_LIVE` and `WATCH_NOT_LIVE`. The requested full sequence (locals, watches, and stepping included) was not proven.

The 25-cycle stress gate was not run because the 10/10 full acceptance gate did not pass. No physical hardware run was performed; all QEMU evidence is software emulation.

## Final classification

The historical missing return was materially repaired and successful production-path returns were demonstrated. The remaining failures occur before authoritative Server RUNNING: a bounded symbol parser failure (`malformed_dwarf`) and a breakpoint-map identity failure (`artifact_changed`). These are the next blockers in source/symbol or Phase 29F start validation. The independent Phase 29A stop-mapping failure remains observed in one successful-return boot. Phase 29B dispatch-before-RUNNING ordering remains intact.

Phase 29G final classification is **Outcome B**. No start was retried to hide a failure. Pushes remain subject to the existing SSH `Permission denied (publickey)` environment behavior; authentication and remotes were not changed.
