# Developer Studio Phase 29L — Project-Load Ownership Convergence

Date: 2026-09-27 (UTC execution crossed into 2026-09-28)

## Starting point and result

| Repository | Branch | Starting HEAD |
| --- | --- | --- |
| Standalone Developer Studio | `main` | `d2e581cf31976ba8431133d338056b8e603fe568` |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `160a14e44f5ab0004f6ea78e4bb43849836c2e4a` |

The supplied Phase 29K note said the branches were 2/0 and 1/0 ahead/behind. At Phase 29L start, both actual worktrees were at 0/0 against their configured upstreams. No branch, remote, credential, or history operation was performed.

**Outcome B.** Ownership semantics, reason reporting, and trace coverage were materially improved, and a real active-project generation publication defect found by QEMU was repaired. Debug and Release CTest, package, and DWARF checks pass. The latest focused series accepted boot 1/5 through `ready`; boot 2/5 reset and triple-faulted after `LoadStarted`, before metadata validation. The focused gate therefore stopped at 1/5. That failed boot was not retried; the 10-boot and 25-cycle gates were not run.

## Phase 29K history and retained boot-4 reconstruction

The Phase 29K interpretation is retained: the later `phase28z_project_open_observer` report at `Failed` reused the same `ManifestValidationDiagnostic`. `LoadProject` has one call to `ValidateApplicationManifestIdentity`; the later line is not a second validation. The failure path reported the compatible `LoadInProgress` error and released the transaction. That error did not mean the progress flag remained set.

The retained Phase 29J boot-4 serial log is:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-806518b997864430bef4386cdaf8c9d0\boot4.serial.log`

Recoverable values:

| Stage | Recovered value |
| --- | --- |
| Startup/request creation | app generation `1`, startup request `1`, project request `1`, path `/P28Q` |
| Transaction acceptance / `LoadStarted` | one `LoadStarted`; request ID `1`, request generation `1`, candidate ID `1`, candidate generation `1`; metadata path `/P28Q/guidexos.project` |
| Manifest validation | path `/P28Q/app/app.json`; expected and parsed identity generation `1` |
| `Loaded` | active project generation `0`; candidate model project generation `0` |
| Observer failure | the same diagnostic was reported at `Failed`; `projectLoadIsCurrent` had returned false after `Loaded` |

The boot-4 evidence contains no transaction ID/generation, owner pointer, controller progress flag, refresh owner, or per-field equality results. It cannot identify which then-existing request/transaction/candidate condition failed. No root cause is inferred from it. The historical false result was not reproduced in hosted regressions. A later fresh QEMU run did expose a current `ACTIVE_PROJECT_GENERATION_MISMATCH` immediately after commit: the candidate generation was 1 while the published active model still reported 0. Commit publication was repaired to explicitly carry the already validated candidate project generation into the active model, then covered by hosted regression assertions. The latest fresh boot reached `ready` with the active model generation equal to the candidate generation. The separate boot-2 failure described below occurred before metadata validation and did not report an ownership mismatch.

## Ownership contract and namespaces

`projectLoadIsCurrent(controller, requestId)` now delegates to `projectLoadOwnershipResult`. The result is the first failed check in this order:

1. A non-null controller must be supplied.
2. The single global project-load transaction must be active and owned by this controller; `controller->projectOpenInProgress` must also be true.
3. Transaction ID and transaction generation must equal their controller-published copies.
4. The transaction request ID must equal both the checked argument and the controller request ID; the transaction request generation must equal the controller request generation.
5. Transaction candidate ID and candidate generation must equal the controller-published values.
6. Candidate project generation must equal both the private candidate model's `projectGeneration` and the controller's candidate-project-generation copy. Once nonzero, the stored candidate project ID must equal the private candidate model's project ID.
7. Transaction refresh generation must equal the controller's refresh-generation copy.
8. Before commit, active project ID/generation must still equal the transaction's saved previous active ID/generation. After commit, they must equal the candidate ID/generation.

The exact current tuple is therefore:

`controller pointer + active transaction owner pointer + controller load flag + request ID + project request generation + transaction ID + transaction generation + candidate ID + candidate generation + candidate project ID/generation + refresh generation + lifecycle-appropriate active project ID/generation`.

`CURRENT` means every check above passed. Startup generation and app-instance generation are not fields in this predicate; they remain in the separate startup/diagnostic request ownership path.

| Field | Namespace / allocator | Legitimate transition |
| --- | --- | --- |
| Project request ID | Per-controller `projectOpenRequestId` | Increments for each accepted request on that controller |
| Project request generation | Global `g_nextWorkspaceProjectRequestGeneration` | Allocated independently for each accepted request |
| Transaction ID | Global `g_nextWorkspaceProjectTransactionId` | Allocated at transaction creation |
| Transaction generation | Global `g_nextWorkspaceProjectTransactionGeneration` | Allocated at transaction creation, independently of transaction ID |
| Candidate ID | Global `g_nextWorkspaceProjectCandidateId` | Allocated with the request's private candidate |
| Candidate generation | Global `g_nextWorkspaceProjectCandidateGeneration` | Allocated independently of request generation |
| Candidate project ID / generation | Parsed manifest project ID; model `g_nextProjectGeneration` via `WorkspaceModelSetRoot` | Becomes available after `Loaded`, when the private candidate model is created |
| Refresh generation | Global `g_nextWorkspaceProjectRefreshGeneration` | Allocated after candidate creation and before `RefreshStarted` |
| Active project ID / generation | Controller's committed `WorkspaceModel`; generation from the model namespace | Previous project remains active through pre-commit checks; candidate publishes at commit |
| Startup / app generation | Diagnostic startup/app lifecycle owner | Independent of project request and model generations; not compared by this predicate |

The earlier manifest validator incorrectly required request generation to numerically equal candidate generation. This was removed. Manifest identity's expected and parsed generations are checked against candidate generation; request generation remains its own namespace. Equal numbers in two namespaces are incidental, not proof of shared ownership.

## Result model and lifecycle semantics

`WorkspaceProjectLoadOwnershipResult` provides explicit results for `CURRENT`, no controller, inactive transaction, controller not in progress, transaction owner mismatch, request ID/generation mismatch, transaction ID/generation mismatch, candidate ID/generation mismatch, candidate project ID/generation mismatch, refresh generation mismatch, and active project ID/generation mismatch. The boolean helper remains for existing control flow; it stores its exact result on the controller. Trace and observer events also carry that result.

The hosted tests mutate request, transaction, candidate, refresh, active-project, and controller-progress fields during the real `Loaded` observer and assert each precise result. Stale post-release checks assert `TRANSACTION_NOT_ACTIVE`. Existing depth-2 re-entry tests remain in place.

`Loaded` means project metadata and the application manifest parsed and validated into a `ProjectOperationResult`. At this event the private candidate `WorkspaceModel` has not yet been set up, refresh has not started, the active model is still the prior committed project, and the observer has run synchronously. Ownership is checked immediately after the observer returns. Candidate ID/generation is published in the next `CandidateAllocated` event.

The candidate receives a model project generation from the model allocator. The old active ID/generation remains expected through refresh, `Validated`, and the pre-commit check. After the single commit assignment, the expected active ID/generation changes to the candidate. This avoids comparing the pre-commit active generation to the future candidate generation.

Refresh generation is allocated by the project-load transaction, copied to the controller, and then checked against that owner. The public `WorkspaceControllerRefresh` rejects while a project load is in progress, and transaction re-entry is rejected, so an unrelated controller refresh cannot silently replace the candidate refresh. A changed published refresh value produces `REFRESH_GENERATION_MISMATCH`.

Observers receive an authoritative state/result snapshot. The production observer reports state and consumes diagnostics; it does not validate again, advance generations, or release the transaction. Synchronous observer changes before commit are caught by the subsequent ownership check. The observer API remains a callback and relies on its documented read-only ownership role.

Release records `TransactionRelease` while the transaction is current, changes progress to false and active to false once, emits `TransactionReleased` with `TRANSACTION_NOT_ACTIVE`, then clears private candidate storage. This preserves identity values for the release record. All exercised success and failure paths release at most once. The legacy `ProjectErrorCode::LoadInProgress` remains for compatibility and describes a rejection/failure observed during an active load; `projectOpenInProgress` is separately traced as a boolean current-state field.

## Trace and guest-log design

Phase 29L has a dedicated fixed 320-byte staging buffer and a reserved trace counter independent of the Phase 29K/29C verbose trace count. It allows at most 64 tuples and 512 compact records; each tuple emits ten split records. The one-load happy path uses 33 tuples / 330 records, leaving bounded headroom. One overflow marker is emitted if a cap is reached. Request/transaction/candidate IDs and generations, candidate-project identity, refresh/active identity, state, result, commit count, and release count are separate records instead of one large summary.

Long candidate project IDs are split into expected and actual records. Phase 29E no longer duplicates the request-generation field in its manifest summary. The harness checks every ownership line is at most 240 characters after the guest host-log prefix and parses every tuple family. The earliest boot used a pre-final marker layout; it exposed that the parser did not distinguish the eight repeated observer callback/return tuples by state, and that the Phase 29E parser assumed request and candidate generations shared a namespace. A later boot exposed a genuine active-model generation publication mismatch; that production defect was fixed and added to hosted assertions. The latest final package hashes were then used in a new focused series. No failed individual boot was retried.

## Validation

| Check | Result |
| --- | --- |
| Hosted Debug CTest | 33/33 passed |
| Hosted Release CTest | 33/33 passed; `NDEBUG` setup fixes retained |
| Project ownership reason tests | Passed; exact mismatches and normal path exercised |
| Re-entry regression maximum depth | 2 (retained and passing) |
| DWARF capacity | PASS, 397 DIEs |
| Package audit | PASS: amd64 and arm64 ELF64, little-endian, `ET_EXEC`, no section metadata/debug sections; exact runtime file set |
| Phase 29J evidence-helper tests | PASS |
| PowerShell parser (`smoke-compiler-bootstrap.ps1`) | PASS after runner changes |

Final package hashes:

| Architecture | SHA-256 |
| --- | --- |
| AMD64 | `DA634886357A97774CE23600AA806A0F93358028B4AECE9F2E2D724798998342` |
| ARM64 | `5DEC7248F9F2BB4EB47A34BBBCB1D9DFB3138E18E1E318E305C36FE0A2E8231F` |

The QEMU history is kept separate by fresh series; no failed individual boot was retried:

1. The first evidence-validation attempt used AMD64 hash `3B8ECC5DC22B9C3C079228C9F5826BE7416F97EB7BA6B3735168F74A14F3DB9B`. Production reached `ready`, with `CURRENT` through ready and `TRANSACTION_NOT_ACTIVE` at release. Evidence was rejected because observer tuple records lacked state keys, the release snapshot omitted candidate-generation/owner identity, and Phase 29E parsing assumed request and candidate generations shared a namespace. The evidence gate failed at 0/5.
2. After parser/trace corrections, a new attempt on AMD64 hash `DA634886357A97774CE23600AA806A0F93358028B4AECE9F2E2D724798998342` stopped on an observed `ACTIVE_PROJECT_GENERATION_MISMATCH` after commit: active model generation was 0 while candidate project generation was 1. This was a production ownership defect, not evidence loss. Active generation publication and hosted assertions were corrected.
3. With final hashes `BE256467CD05E197579C30BF9DD91E912A16918D0A582B4B353C99D069105117` (AMD64) and `882D5A438DAC513C9A0850BBFA2A639ECCA5BCEF6703D7E9880AF1CD5676C950` (ARM64), boot 1/5 passed the focused acceptance gate through `ready` and exactly-once release. Boot 2/5 passed Phase 29J early boot and Phase 29I sentinel checks, created exactly one `/P28Q` request, and reached `LoadStarted`; QEMU then repeatedly reset and logged `Triple fault`. The serial log contains no project-metadata `STAT_BEGIN`, metadata validation, manifest validation, or later ownership checkpoint. This is an early guest/runtime failure before an ownership result can be evaluated, not an ownership mismatch or evidence-only failure. The focused gate stopped at 1/5; boots 3–5 were not run.

| Requested gate | Result |
| --- | --- |
| Focused ownership QEMU | Latest series: 2/5 attempted, 1/5 accepted; boot 2 reset/triple-faulted after `LoadStarted`; stopped |
| Phase 29J firmware/loader/kernel/UART/NativeElf | PASS on latest boot 1 and boot 2 through Developer Studio entry |
| Phase 29I sentinel | PASS on latest boot 1 and boot 2: exact sentinel, diagnostic mode enabled |
| Phase 29D request | PASS on both latest boots: exactly one `/P28Q` request |
| Phase 29C owner checkpoints | Latest boot 1: `CURRENT` through ready and exactly-once release; boot 2 stopped after `LoadStarted` before metadata validation |
| Phase 29H artifact/symbol snapshot | Not run as a gated extension |
| Phase 29F/29B start and RUNNING | Not run as a gated extension |
| Phase 29G return / Phase 29A mapping | Not run |
| Hosted project load | PASS through the production `OpenProject` path in the hosted project test suite, including load, refresh, commit, and ready |
| Hosted breakpoint state | Not re-evaluated in Phase 29L; existing source-association / pending breakpoint boundary remains independent |
| Full Phase 28Q QEMU | Not run; focused 5/5 did not pass |
| 25-cycle stress | Not run; 10/10 did not pass |
| Physical hardware | Not tested; validation used QEMU and hosted tests |

The first missing project-load marker on the latest failed boot was project metadata access/validation after `LoadStarted`. The preserved serial log ends at the request-accepted/load-started events; the QEMU debug log records repeated CPU resets and `Triple fault`. The exact internal guest faulting instruction was not recovered from these logs, so no narrower root cause is claimed. Both final package hashes were used by the latest fresh focused series; boot 1 passed, boot 2 failed before metadata validation. No failed boot was retried.

## Repository state

The six pre-existing untracked validation artifacts under `tests/fixtures/debugger-phase15*` and `tests/fixtures/debugger-phase3b` remain untracked and preserved. The server sentinel remains untracked with SHA-256 `967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A`. The package ELF files are tracked server package outputs and are part of the Phase 29L server changes. No physical hardware run occurred.
