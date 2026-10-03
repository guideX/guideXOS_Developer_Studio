# Developer Studio Phase 29R — Hosted Target Exit-Code Lifecycle

## Scope and repository lineage

Phase 29R closes the Phase 29Q exploratory Build B case where a compiler-produced GXSM target returned `1`. The application result and debugger lifecycle must remain separate: ordinary returns such as `0`, `1`, and `7` are application exit codes, while `Failed` is reserved for a broken target/runtime/debugger lifecycle.

Starting commits:

| Repository | Branch | Starting HEAD |
| --- | --- | --- |
| Standalone Developer Studio | `main` | `ddb84260a1e767f4c5616b20c81e56373882cd18` |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `edb0e56388dd8b96ef5c699bf6ea2a70ba6f771f` |

The standalone history already includes the Phase 29Q endpoint and the separate document-activation commit described in the request. The Server checkout likewise begins at its Phase 29Q endpoint. No branch switch, reset, rebase, remote change, or authentication change was made. Before Phase 29R edits, standalone `src/main.cpp` already had an uncommitted Phase 29Q GXSM summary trace; that hunk remains out of the Phase 29R commit. The Server checkout already had modified packaged `Apps/DeveloperStudio/app.json` and AMD64 ELF plus the untracked `.phase28q-diagnostic` sentinel; all remain preserved and uncommitted.

## Phase 29Q baseline and retained Build B evidence

Phase 29Q accepted the compiler-produced positive GXSM v2 fixture with a signed i32 local `counter`, source mapping, stack-relative memory reads, watches, stepping, and relaunch. Its accepted baseline was 5/5 hosted, 25/25 hosted stress, 5/5 QEMU, Debug and Release CTest 33/33, DWARF capacity 397 DIEs, and passing AMD64/ARM64 package audits.

The retained exploratory Build B changed only `debugProbe`'s return from `0` to `1`. The target reached its normal return path after the line-4 breakpoint, GXSM watch, Step Over, and Step Out. In [the retained Phase 29Q trace](</C:/Users/guideX/AppData/Local/Temp/phase29q-build-ab-20261002/logs/build-b-hosted-corrected2.log>), NativeAppRuntime reported `state=Failed exitCode=1`; the controller ended `state=Failed stop=None stop_gen=0` with `debug_transition=Running->Failed sequence=9`. There is no `TARGET_EXIT_NORMAL` or `debug_state=EXITED` after that transition. The first required marker missing was `debug_state=EXITED`, which caused the smoke's bounded clean-exit wait to fail. The exact code was observed, but the runtime and development-run layers used the nonzero value to assign `Failed`. This reproduces **Class D: exit-reason conflation**: the Server execution and run layers used an application value as a runtime-failure reason. Class B does not apply because the numeric code was not lost or changed. It was not a breakpoint, GXSM, or target-return failure. Additional Phase 29R diagnostic attempts are retained under `%TEMP%\phase29r-hosted-gate-traces-20261002` and `%TEMP%\phase29r-hosted-corrected-gate-traces-20261002`.

## State, reason, and numeric result

The shared controller uses `DebugSessionState::{Idle, Launching, Running, Paused, Stopping, Exited, Failed, Stepping}`. `Exited` means the target terminated and its owned runtime cleanup completed; it does not mean the application returned zero. `Failed` means execution or debugger/runtime handling failed. A normal return transitions directly from `Running` to `Exited` without a synthetic stopped/paused event.

The separate stop/exit reason is `DebugStopReason`: a normal backend return is `Exited`, a debugger-requested termination is `UserRequested`, and exceptions/backend failures remain failures (`Unknown` at the hosted snapshot boundary, with the concrete failure message retained). `DebugEvent` and `DebugBackendSnapshot` carry `int32_t exitCode`; the Server process/runtime records and Native ELF entry result use the same signed 32-bit representation. Thus the current guideXOS contract is a signed 32-bit application result, not a process-success boolean.

## Hosted exit capture and ownership

The Server `NativeElfExecutor` calls the target entry and captures its `int32_t` return. It sets lifecycle `Failed` only when exception/fault handling sets `executionFailed`; any ordinary return, including `1` or `7`, stores the exact code and completes lifecycle `Exited`. A thrown exception still records failure and a failure reason.

The process table keeps process ID, NativeApp runtime ID, lifecycle state, signed exit code, and failure reason. `DevelopmentRun::Poll` checks the process status and the exact owned runtime record. It publishes `GX_DEVELOPMENT_RUN_EXITED` only when process status is available and that runtime is `NativeAppLifecycleState::Exited`; a missing runtime or `Failed` runtime remains a run failure. The run record stays owned until the debugger consumes terminal metadata and releases it.

The hosted debugger snapshot copies exit code and cleanup status. It maps run `Exited` to `DebugSessionState::Exited`, with `DebugStopReason::Exited`; a close request maps to `UserRequested`. The controller accepts snapshots only for its current session generation, publishes one `DebugEventKind::Exited` on the transition into `Exited`, and rejects stale-generation events. Duplicate terminal snapshots do not add another Exited event. Start resets exit code and teardown status before assigning the next session generation.

The successful two-session proof observed session generations `1` then `2`; each project target generation was `1` in its newly opened project. The target identities were process/runtime `12/2` and `13/3`. The first session stepped over and out, then its accepted Continue was observed as `Running` before `Exited` with code `7` (`debug_transition=Running->Exited`). On the second launch, Continue consumed a terminal snapshot synchronously and the controller transitioned directly from the breakpoint's `Paused` state to `Exited`; it did not publish a synthetic `Running` state after the backend had already returned the terminal result. Both are clean Continue-to-exit paths, and the first proves the ordinary `Running->Exited` route. No backend-failure marker appeared. The durable runtime log reported `classification=normal-return` and cleanup complete.

## Stop, Continue, and pending-step behavior

For a normal target return after Continue, the observed path is:

`Paused` → `Running` → application return → `Exited` → runtime/session cleanup.

A user Stop/Terminate remains distinct: its stop reason is `UserRequested` and UI text says the target was terminated by the debugger. It is not presented as a normal application return. A runtime exception/fault remains `Failed` with a backend/runtime reason.

The controller tests drive target exit over pending Step Into, Step Over, and Step Out, verify the exit remains authoritative, and ensure step completion is not fabricated. Terminal step snapshots clear temporary step state. During polling, terminal snapshots bypass the breakpoint-bind/release gate; otherwise a deferred launch gate could rewrite an already terminal snapshot to `Running`. This was a second controller convergence defect caught while adding the code-7 regression.

## Inspection invalidation, teardown, and relaunch

At exit the controller clears stopped context, call stack, source-step state, temporary step breakpoints, and runtime target bindings. The UI terminal reset preserves the logical breakpoint/watch configuration but clears the watch result. Its `debug_watch_runtime=invalidated` marker is now emitted at the terminal reset that actually clears those results; previously the next poll path was too late because the session-generation marker had already been reset.

Hosted GXSM validation confirms `counter == 2` at the breakpoint, `counter == 3` after Step Over, and a fresh `counter == 2` at the relaunch breakpoint. Continue invalidates the prior stopped inspection. After each normal exit, stack/locals/stop context are no longer current and the watch result is stale. The next launch has a fresh session/process/runtime identity and rereads memory from its own runtime address. The smoke independently asserts target cleanup and `debugger_teardown=PASS`, then confirms `debug_state=EXITED`.

## Exact repair

Server changes:

- `native_elf_executor.cpp` no longer equates a nonzero normal return with Native ELF execution failure. It records runtime `Exited`, preserves the exact signed code, and reports `success` unless execution raised an exception/fault.
- `development_run_service.cpp` classifies termination from the matching NativeApp runtime lifecycle, not from `exitCode == 0`; it retains process/runtime IDs and logs `normal-return` versus `runtime-failure`.

Standalone changes:

- `DebugController` now retains `cleanupComplete` independently of application `exitCode`, copies both from terminal snapshots, and resets both at the next start.
- The hosted adapter keeps normal `Exited`, user-requested termination, and backend failure distinct.
- UI output and markers report the exact code (`TARGET_EXIT_NORMAL code=N`), separate teardown (`debugger_teardown=PASS/INCOMPLETE`), and backend failure (`DEBUG_BACKEND_FAILED`).
- Terminal polling no longer releases/reopens a launch gate or turns a terminal result into `Running`; step temporary state is retired and generation checks remain authoritative.
- The smoke harness takes all per-session baselines before F5, checks exact expected code and relaunch identities, verifies watch-result invalidation, and deduplicates Server log echoes by the runtime/session identities they repeat.

No SDK ABI was changed. The committed Server SDK predates the standalone app's optional document-activation callback, so hosted acceptance used an isolated copy of the Server tree with only a temporary SDK declaration overlay. The runtime does not advertise that optional callback; no tracked SDK or original Server package file was changed for the overlay.

## Regression coverage

`debugger_test.cpp` covers code-7 propagation, one Exited event, duplicate snapshot behavior, user termination reason, clean state reset on a new session, and rejection of a stale prior-session exit. `debugger_step_test.cpp` covers exit over Step Into/Over/Out, absence of synthetic step completion, and cleanup of temporary return traps. Hosted regression uses the real compiler-produced GXSM v2 artifact and verifies exact return code, runtime cleanup, independent debugger teardown, inspection/watch invalidation, and fresh relaunch identities.

The hosted 5/5 and 25/25 code-7 gates pass. A post-fix code-1 pair also passes. The code-0 target sessions reach normal exit and teardown, but the enclosing hosted Server exits with Windows status `-1073741819` (`0xC0000005`) after printing its normal shutdown line; this gate is recorded as failed pending investigation of the host-process crash.

## Validation ledger

| Gate | Result |
| --- | --- |
| Debug CTest | 33/33 PASS (serial run) |
| Release CTest | 33/33 PASS (serial run) |
| DWARF capacity | PASS, 397 DIEs |
| AMD64 package audit | PASS; 1,106,332 bytes, SHA-256 `D2EEBE32DAA9FDF9119085063663B03393C8439A9648B1D8AAEBF0A51D0B90AB` |
| ARM64 package audit | PASS; 1,266,204 bytes, SHA-256 `15C69A2F69E22C7C2FA83C269905A324DB13FFB470ED525C06D46D9CF6DD073F` |
| Hosted code-7 focused gate | PASS, 5/5 iterations, 10 target sessions |
| Hosted code-7 stress | PASS, 25/25 iterations, 50 target sessions, no retries |
| Hosted code-0 regression | Target exit and teardown pass in both sessions; overall gate FAIL because hosted Server exits `-1073741819` (`0xC0000005`) after normal shutdown output |
| Hosted code-1 regression | PASS, 1/1 iteration, two target sessions; exact code `1`, teardown pass, hosted Server exit `0` |
| QEMU full acceptance | FAIL on first boot (`1/10` attempted; no retry). The `/P28Q` project loads, but debug-start build reports `invalid_project_root`, before a target starts. |
| QEMU lifecycle stress | PASS, 25/25 fresh boot stages, Phase 29L project-load ownership |

The first Debug/Release CTest attempts were launched in parallel and collided on shared temporary fixture files (three unrelated search/symbol/reference assertions failed in Debug). Both complete suites passed when rerun serially; no source changes were made to those tests. The hosted focused and stress code-7 gates used the same return-7 artifact. Code 0 and code 1 were each run once after the shared fix; the repeat wrapper now accepts zero and reports generic target-exit labels.

Build A (return `0`) artifact: 5,021 bytes, SHA-256 `80054B83736EE446F416530B95B0611B34C116D6E4B26B122B00190BD3641C46`.

Build B (return `7`) artifact: 5,021 bytes, SHA-256 `3C87B0EAAEEFB8D163BACF8CF064AFFAA68068F411288DB3EE08EC4A5D50AA38`; the changed source SHA-256 is `07F191341D6F0A20C0F0A3F63CE4A813F4D8EF344FA488CD4059DEF8C45D9F98`. It retains GXSM v2 metadata (trailer offset `4273`, size `748`, two variable records, four source records). The target watch remains usable, and the ELF identity changes when only the return constant changes.

Post-fix Build B return `1` artifact: 5,021 bytes, SHA-256 `C0581E184973808AB1FC3F48D432B82D4826A1DB8E87F6F9C325CDFDEF6595C2`; source SHA-256 `0513EB8C00F490EB94382ADB81F2D4982AAA5E863DA6C552FA26F0F2E89FBB19`. It completed both target sessions with the exact signed i32 value and clean debugger teardown.

The code-0 fixture retained its Build A artifact, 5,021 bytes and SHA-256 `80054B83736EE446F416530B95B0611B34C116D6E4B26B122B00190BD3641C46`. Both target sessions published exact normal code `0` and `debugger_teardown=PASS`; only the hosted Server's process exit failed afterward with access-violation status. No retry was made.

The temporary hosted package audited as AMD64 1,106,332 bytes, SHA-256 `D2EEBE32DAA9FDF9119085063663B03393C8439A9648B1D8AAEBF0A51D0B90AB`, and rebuilt ARM64 1,266,204 bytes, SHA-256 `15C69A2F69E22C7C2FA83C269905A324DB13FFB470ED525C06D46D9CF6DD073F`. Both staged packages include Phase 29R app code. The Server repository's pre-existing AMD64 package remains unchanged at its recorded Phase 29Q hash.

The standalone source refers to a trailing optional document-activation callback absent from the Server SDK revision. A declaration overlay was used only in the temporary tree to build the hosted-compatible package; before QEMU kernel builds, the staged SDK header was restored byte-for-byte to the Server checkout's original header (SHA-256 `4135C38EAC0572D00532D298E28E7471D565BB4F329AE93F08A47A59EA8E9884`). The callback is size-guarded by the app and is not advertised by the Server runtime. The QEMU runner also expects the sentinel-definition header in a sibling standalone directory; a temporary sibling containing that one canonical header was supplied, with matching SHA-256 `A115AF43F81D3EC883B6DAE86EE07C55F9D5D194407F28F0D6FBF4CB44FEAD24`.

## QEMU, repository closeout, and outcome

QEMU uses the independent fresh-boot acceptance runner in the temporary Server staging root. Since Phase 29R changes shared controller `EXITED` and terminal-generation logic, the required runs were the full 10-boot acceptance and 25-boot lifecycle stress, in addition to the hosted gates. The 25-boot `-Phase29LOwnershipOnly` run passed all 25 fresh stages; evidence is under `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-72e6d1e90f9d4bebab7fa7b976ce237e`, with aggregate output in `%TEMP%\phase29r-qemu-ownership25-20261002.log`.

The `-Phase29LFullAcceptance -BootCount 10` run stopped at boot 1 under the no-retry policy. The app loaded `/P28Q` and read its manifest/source, but the subsequent bare-metal build reported `invalid_project_root` and Phase 28Q emitted `DEBUG_START_BUILD_FAILED`; therefore the debugger target never started. This independently reproduces the historical Phase 29P `invalid_project_root` failure signature, not a Phase 29R target-exit failure. The aggregate log is `%TEMP%\phase29r-qemu-full10-20261002-final.log`. Two earlier invocations failed preflight before any VM boot: one exposed the temporary SDK overlay's 456-byte `gx_host_calls` against the kernel's 448-byte assertion, and the next lacked the sibling sentinel header. Both were corrected in temporary paths before the actual acceptance boot; neither consumed a boot attempt. Physical hardware was not used; all gates were hosted or software-only QEMU.

The mandatory gates do not all pass: hosted code-0 target lifecycle succeeds but the hosted Server process then exits with `0xC0000005`, and QEMU full acceptance cannot start the Phase 28Q target because the staged `/P28Q` build reports `invalid_project_root`. The focused code-7 gate, 25-iteration hosted stress, code-1 regression, both package audits, and QEMU 25-boot ownership stress pass. **Phase 29R outcome: B — changes and positive-exit regression are complete, with the code-0 host-process crash and QEMU full-acceptance project-root failure outstanding.**

## Repository closeout

The implementation commits are:

- Standalone Developer Studio: `5b5d794c3bf8f7c19cce68ced2d69718193402f2` (`Fix hosted target exit lifecycle propagation`).
- Server integration: `bf78d517b513b624f61bee4ae6d144377f6c8000` (`Treat normal native app returns as exited`).

Ordinary pushes to `origin main` and `origin v0.5_DEVELOPER_STUDIO` were attempted and both failed with `Permission denied (publickey)`. Remotes and authentication were not changed. At those attempts each branch was 1 ahead / 0 behind; the final standalone ledger closeout commit adds one local commit, leaving standalone 2 ahead / 0 behind and Server 1 ahead / 0 behind.

The remaining standalone worktree entries are the pre-existing Phase 29Q `src/main.cpp` host GXSM trace hunk, left unstaged, plus these preserved untracked fixtures/configs: `tests/fixtures/debugger-phase15-h29-0927-phase29i-hosted-run-1/`, `tests/fixtures/debugger-phase15-h29-0927/`, `tests/fixtures/debugger-phase15/guidexos.debugger.json` and `.bak`, `tests/fixtures/debugger-phase29q-positive/guidexos.debugger.json` and `.bak`, and `tests/fixtures/debugger-phase3b/guidexos.debugger.json` and `.bak`.

The remaining Server worktree entries are the user's pre-existing modified `Apps/DeveloperStudio/app.json` (SHA-256 `5793567C54ABF22423A8FCDE2F9B32E5DEEB73981E070707BA11E7D23F390401`) and `Apps/DeveloperStudio/bin/amd64/developerstudio.elf` (SHA-256 `106BD4D1C8827894AB6CE4850E5DC30E55E98A1BAA52BCA149027781B4081FE9`), plus untracked `ESP/Apps/DeveloperStudio/.phase28q-diagnostic` (content `guideXOS-phase28q`, SHA-256 `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`). They were neither staged nor committed.

No physical-device verification was performed.
