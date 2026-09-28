# Developer Studio Phase 29K — Project Load Re-entry and Duplicate Validation

## Result

The repeated Phase 29E manifest evidence on Phase 29J boot 4 was repeated observer logging, not a second manifest validation. The first record came from the `Loaded` event; the second came from the subsequent `Failed` event with the same saved diagnostic. `LoadProject` has one call to `ValidateApplicationManifestIdentity`.

The boot 4 load still failed after `Loaded`: `projectLoadIsCurrent` returned false, and the owner reported `LoadInProgress`. The failure path then calls `releaseProjectLoadTransaction`, which clears `projectOpenInProgress` and the global active transaction. The retained boot 4 trace does not expose which component of the current-owner predicate changed, so that failure is not claimed as repaired.

One new fresh QEMU boot reached Phase 29C `ready` with a single startup request. The focused runner rejected the boot because the new Phase 29K evidence was suppressed or did not fit the guest logging path. The runner and marker defects were corrected afterward, but the focused QEMU series was not rerun. The 5/5 gate therefore remains incomplete.

## Starting State

| Repository | Branch | Starting HEAD |
|---|---|---|
| Standalone Developer Studio | `main` | `b40a858e004c4c532f17443830028ee2238a95cb` |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `edcc80f0134b504ff90aa50859dfbf0f131d6921` |

At inspection, `main` matched `origin/main` (`0` ahead / `0` behind). The server branch compared as `125` local-only commits and `198` upstream-only commits, which differs from the Phase 29J narrative that described it as one commit ahead. No branch, remote, authentication, or history changes were made; the server history was not reconciled.

The pre-existing untracked standalone validation artifacts were preserved:

- `tests/fixtures/debugger-phase15-h29-0927-phase29i-hosted-run-1/`
- `tests/fixtures/debugger-phase15-h29-0927/`
- `tests/fixtures/debugger-phase15/guidexos.debugger.json`
- `tests/fixtures/debugger-phase15/guidexos.debugger.json.bak`
- `tests/fixtures/debugger-phase3b/guidexos.debugger.json`
- `tests/fixtures/debugger-phase3b/guidexos.debugger.json.bak`

The pre-existing server sentinel remained untracked and unchanged at `ESP/Apps/DeveloperStudio/.phase28q-diagnostic`. It contained `guideXOS-phase28q` (17 bytes), SHA-256 `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`.

## Phase 29J Boot 4 Classification

| Question | Evidence and finding |
|---|---|
| Startup request count | One created, one submitted, one accepted, one handoff for `/P28Q`; startup app generation was `1`, startup generation/request was `1`. |
| Phase 29C transaction entry | One `load_started` was recorded. No second accepted request or second `load_started` appears in the retained serial trace. |
| First manifest diagnostic caller | `phase28z_project_open_observer` handling `WorkspaceProjectOpenState::Loaded`, after the synchronous `LoadProject` call completed. |
| Second manifest diagnostic caller | The same observer handling the subsequent `WorkspaceProjectOpenState::Failed`. It reused `controller->lastManifestDiagnostic`; it did not call `LoadProject` or the validator. |
| Request/generation/path identity | Both records carried request `1`, request generation `1`, candidate `1`, candidate generation `1`, expected/parsed identity generation `1`, and `/P28Q/app/app.json`. The transaction validated the application manifest; `guidexos.project` was the separate project metadata file read to construct the project. |
| Duplicate classification | **E — observer duplication.** The second diagnostic was repeated reporting of the same validation result, not duplicate request processing, a second validation call, or nested transaction entry. |
| Actual failure point | The current-owner check immediately after notifying `Loaded` returned false. The failure branch emitted `Failed` with `LoadInProgress`, then released the transaction. The older trace cannot distinguish whether controller progress, active transaction, owner identity, or request identity was the false predicate. |
| Was `load_in_progress` left set? | No persistent stuck flag is evidenced. `LoadInProgress` was the reported error; the failure branch calls the release routine, which clears the controller flag and global active owner. The trace ends on the failed request, so it did not settle to `ready`. |
| Historical recursion depth | Not captured in Phase 29J. No nested project-open caller was recorded; the single startup handoff and single `load_started` argue against a second startup request, but do not prove the old owner predicate's exact false component. |

The exact duplicate *diagnostic* caller is therefore known. There was no second manifest-validation caller to identify. The exact cause of the stale/current-owner result remains unproven.

## Transaction Contract and State Machine

The production state names and success progression are:

`idle → load_started → loaded → candidate_allocated → refresh_started → validated → committing → active → ready`

`failed` is a terminal result from any pre-ready operation that fails. A failure path reports the error, releases the active transaction, and leaves the previously committed model in place. `ready` is emitted after the candidate model is assigned to the active model. Only the synchronous transaction owner performs commit and final release; the observer receives snapshots and does not settle the transaction.

During an active transaction, another `WorkspaceControllerOpenProjectFrom` call is rejected with the existing `ProjectErrorCode::LoadInProgress` convention. It does not increment the accepted request ID, allocate a transaction or candidate, reset progress, or release the original owner. It is rejected rather than joined. A call after completion starts a new request and receives new monotonic transaction identity. Caller labels now distinguish the startup pump, project-create-open, project-path dialog, and generic workspace API entry.

Manifest validation belongs to the one `LoadProject` operation in the transaction. `guidexos.project` is read as project metadata; `/P28Q/app/app.json` is parsed and identity-validated with role `application`. Repeating the same validation for the same transaction is not intended. The per-result validation count records one validator invocation.

## Changes Made

- Added monotonic transaction ID/generation, request/candidate identity snapshots, recursion depth, re-entry caller/count, stage count, refresh count, commit count, owner state, manifest role/path, and validation count to the transaction diagnostics.
- Strengthened `projectLoadIsCurrent` to require the active owner plus matching transaction ID/generation, request ID/generation, and candidate ID/generation. Each identity comparison is exposed separately in the failure snapshot.
- Added `WorkspaceControllerOpenProjectFrom` so production call sites identify their entry source while the original API remains a wrapper.
- Kept the existing in-progress rejection contract and added regression cases that trigger nested calls during `LoadStarted`, project metadata read, application manifest read, and refresh listing. The tests verify that the nested call is rejected, the original transaction commits once, validation/refresh/commit counts remain one, and the active load flag settles.
- Added checks for application-manifest versus project-metadata paths, successful and failed settlement, preservation of the prior project on failure, immediate new opens, and fresh transaction identity after controller reinitialization. A focused stale-request-generation callback verifies rejection before refresh/commit, restoration of the idle transaction state, and that a subsequent fresh request succeeds.
- Deduplicated Phase 29E diagnostic output per request so a `Failed` notification cannot print the same completed manifest diagnostic a second time.
- Corrected the QEMU focused runner so Phase 29C-only mode does not require Phase 29F markers. The runner now parses the host-prefixed marker lines and checks ordered transaction stages, one candidate/transaction identity, manifest role/path, one validation, one refresh, one commit, and settled result counts.
- Split long guest diagnostics into bounded records. The first attempted run showed that Phase 29K markers were not emitted after the Phase 29C trace counter's early return, and the enlarged Phase 29E summary was absent while shorter subsequent identity records remained. The Phase 29C cap now limits only Phase 29C records and does not suppress Phase 29K records.

These changes strengthen generation ownership and prove the guard's behavior under the regression callbacks. They do not establish which owner predicate changed in the historical boot 4 failure; no speculative reset, retry, timeout, or forced-clear behavior was added.

## Release Test Repair

The seven Release failures identified in Phase 29J had `NDEBUG` remove setup calls embedded inside `assert(...)`. Those suites now use an always-evaluated `TEST_CHECK` helper. During the full Release run, `debugger_watches_test` also exposed the same pattern as an eighth affected suite: it printed its PASS text but crashed in Release after assertions removed its setup/evaluation calls. That suite was converted to the same helper.

Debug and Release now execute the side-effecting expressions and checks. No production debugger behavior was changed for this repair.

## Validation Results

| Check | Result |
|---|---|
| Debug CTest | **33/33 passed** |
| Release CTest | **33/33 passed** |
| Project transaction regression | Passed in the standalone build-script tests, including nested callbacks at the four points listed above. |
| DWARF capacity | **PASS, dies=397** |
| PowerShell parser | **PASS** for the modified QEMU runner. |
| Package audit | **PASS** for exact expected app package files and sectionless ELF64 AMD64/ARM64 ET_EXEC payloads. |
| AMD64 package SHA-256 | `0EB2A7F0C7428818ECA9F0CD8995B4A3D00E3236F492A83AC3D6E98BF9146308` |
| ARM64 package SHA-256 | `2569F6ABB2D6760D3623BB7C3C0B740ACBB2FB115DFA55AD352DAE3C8857FFF9` |

## Focused QEMU Attempt and Later Gates

The attempted command was:

```powershell
scripts/smoke-compiler-bootstrap.ps1 -Phase29COnly -BootCount 5 -TimeoutSeconds 120
```

The first fresh stage was `d58a99fcc8ee487487768ee7dd125778` under `%TEMP%\guidexos-phase28g-f350a446e60847f4a73d086e80d143d0`. Its ESP audit, firmware/loader/kernel/UART/NativeElf path, and Phase 29I sentinel passed. Phase 29D showed one `/P28Q` request. Existing Phase 29C markers showed one progression through `load_started`, `loaded`, candidate allocation, refresh, validation, commit, active publication, and `ready` (active generation `1`).

The formal runner nevertheless rejected boot 1 because it found no Phase 29K records and counted no complete Phase 29E manifest record. In the captured serial, Phase 29C records were emitted, but the Phase 29K observer block was behind a Phase 29C counter early return. The enlarged Phase 29E summary also exceeded the guest log record size while the shorter following records arrived. The subsequent source changes split the records, moved the Phase 29C cap around only its own trace, and corrected prefix matching. Since the gate attempt failed and the request forbids retrying failed boots, no further QEMU boot was launched.

| Gate/boundary | Result |
|---|---|
| Fresh focused boot | One boot attempted; application reached `ready`, but the Phase 29K evidence gate rejected it. **0/5 boots accepted by the focused gate; no retry.** |
| Phase 29J early boot | Passed on the attempted boot through NativeElf. |
| Phase 29I sentinel | Passed on the attempted boot; exact 17-byte sentinel recognized. |
| Phase 29D request | One created/submitted/accepted/handoff sequence on the attempted boot. |
| Phase 29C project transaction | Existing markers show `ready` on the attempted boot; Phase 29K ownership counters were unavailable, so it is not a verified gate pass. |
| Phase 29H artifact/symbol snapshot | Not validated. |
| Phase 29F start / Phase 29B RUNNING | Debug-start intent occurred after `ready`, but start admission/RUNNING was not part of the accepted focused proof. Not validated. |
| Phase 29G return | Not validated. |
| Phase 29A stop mapping | Not validated. |
| Hosted project | The QEMU-hosted project reached `ready` on the attempted boot. |
| Hosted breakpoint | Not reached/validated; no Phase 29H snapshot or breakpoint-mapping result was accepted. |
| Extended same-boot gate | Not started because 5/5 focused acceptance did not pass. |
| Full 10-boot gate | **0/10; not started.** |
| 25-cycle stress | **0/25; not started.** |
| Physical hardware | Not tested. |

## Remaining Boundary and Outcome

The historical Phase 29J duplicate is explained as observer duplication. The current code has focused unit coverage for nested re-entry and the observed new QEMU boot reaches `ready`, but the 29J owner-currentness failure has not been reproduced with working per-field transaction evidence. The repaired trace and gate parser have not been exercised in a new QEMU run. The next boundary is therefore the Phase 29C current-owner result and a fresh 5/5 focused gate; later debugger, hosted breakpoint, 10-boot, and 25-cycle acceptance remain unverified.

**Phase outcome: B (partial).** The duplicate-validation claim is disproven and the current in-progress rejection semantics have regression coverage. The full QEMU acceptance remains incomplete, and the historical owner-predicate invalidation is not declared fixed.

## Git and Pushes

Changes are committed locally in separate standalone and server commits. Existing untracked validation files and the server sentinel are excluded. Normal pushes are attempted without changing remotes or authentication; if the host rejects SSH with `Permission denied (publickey)`, the local commits are retained and their ahead/behind state is reported in the final task response.
