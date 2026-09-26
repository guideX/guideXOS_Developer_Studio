# Developer Studio Phase 29D — Pre-Project IDE Startup Determinism

Date: 2026-09-26

Result: **Outcome B**

## Repositories and starting state

| Repository | Branch | Starting HEAD |
| --- | --- | --- |
| Standalone Developer Studio | `main` | `82f09bb8f00967cf4398955fb3aa8a301e4af068` |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `4e00d9a24e147c5df7593fcd9560f41f8adf0ecf` |

Both worktrees were clean at those commits. Their upstream branches matched the starting commits (0 ahead / 0 behind), despite the prior handoff note saying they were ahead by one. No branch, remote, or authentication settings were changed.

## Phase 29C baseline and reproduction

Phase 29C's documented baseline was 31/31 CTests, DWARF and locals/arguments PASS, hosted debugger PASS, and AMD64/ARM64 package audits PASS. Its package hashes were AMD64 `DF7669059F98497D454A0FCF18D29EFDB05EDF3A7E63FBAAC84BFC06E5A0CD8F` and ARM64 `C77EEC8C19B11E1756268084E6E0AB0CB0351B5F816254C18546EC8248ACEFD3`. Its five-boot focused run had passed 2/5; the historical boot 3 log (`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-dab84891f18f498ea0a1844f52bc6838\boot3.serial.log`) ended after workspace/search/rename readiness, before navigation/output initialization, window creation, or the first frame. It did not show a project request or Phase 29C transaction.

I reran the unchanged Phase 29C focused route (`-Phase29COnly -BootCount 3 -TimeoutSeconds 180`) for three fresh boots before editing. It passed 3/3; evidence is in `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-ae75e9469f7c4e68ac08c802f6c0273d`. The historical pre-request stall did not reproduce. The old boot log bounds the missing interval, but does not prove why execution stopped there. I therefore do not claim a single historical root cause.

## Startup lifecycle and repair

`DiagnosticStartupOwner` in `src/developer_studio_startup.{h,cpp}` now owns the diagnostic startup for one application instance and startup generation. Its ordered states are:

`initializing → fixture recognized → UI ready → pump entered → request pending → request submitted → request accepted or rejected`

The owner is initialized explicitly, including when its storage begins with poisoned bytes. A live owner cannot be re-entered; every transition validates its generation. A request receives the startup generation as its identity and retains its path, generation, and eventual Phase 29C request ID through acceptance or explicit rejection. Construction requires the recognized fixture, completed UI, and first pump. Duplicate construction, stale generations, invalid paths, and submission without a pending request fail closed.

`gx_main` checks `.phase28q-diagnostic` after the filesystem context is available, records the positive or negative result once, and carries the positive result forward. The check is not repeated speculatively on each frame. UI readiness and pump entry are explicit owner transitions. The bounded `DEVELOPER_STUDIO_PHASE29D_STARTUP_*` trace records application/generation, fixture and diagnostic decision, sequence/frame, request ID, Phase 29C request ID, path, and state. It logs lifecycle transitions only and stops at its fixed trace bound.

The Phase 28Q project-open case constructs and submits one owner-held request. The Phase 29C observer accepts it only on the matching `/P28Q` `load_started` event. A synchronous return before acceptance explicitly rejects the request; a transaction failure after acceptance is reported as a Phase 29C failure and does not issue another open on a later frame. The final trace places `REQUEST_HANDOFF_COMPLETE` at `load_started`, where Phase 29D has handed ownership to Phase 29C. The QEMU harness separately waits for `P28Z APP 05 project_open_return` so it captures the result of the synchronous Phase 29C transaction.

The historical boot 3 is not explained by the current evidence. The code now removes implicit one-shot ownership and a failed-open path that could leave the request stage eligible again, but the historical log ended before the pump; those structural defects cannot be identified as the cause of that earlier stall.

## Regression coverage

The production owner and its hosted tests cover explicit initialization from poisoned storage, absent and present fixtures, one valid request, duplicate/reentrant starts, failed construction, persistent pending state, stale generations, accepted Phase 29C request identity, explicit rejection, and close/relaunch with a fresh generation. The standalone target, native packages, and test build all include the same owner implementation.

## Validation

- CMake build: PASS; CTest: **32/32 passed** (including `developer_studio_startup_test`).
- DWARF capacity: **PASS, 397 DIEs**; locals/arguments validation: PASS.
- Hosted debugger smoke: the first run passed the real breakpoint pause, source navigation, call stack, locals, and clean shutdown. Two runs against the rebuilt server did not reproduce that result: one did not pause at the breakpoint, and a retry after removing the test-generated workspace config timed out at shutdown with the session still `stop_requested`. The fixture config ended with its breakpoint disabled. Hosted breakpoint setup is therefore not repeat-deterministic in this run; the initial full pass is retained as evidence, not treated as a stable gate.
- AMD64 and ARM64 package audit: PASS. Both are ELF64 little-endian ET_EXEC packages without section/debug metadata, and the package contains exactly `app.json` plus the two architecture binaries.
- AMD64 package SHA-256: `9DD69B3B828803B8F7872094086C3A99F5B7E519721B18F1E3E36D607117CF75`.
- ARM64 package SHA-256: `DF3D0CB01D619DA88696663322302C6AC6A20D4914CF183F0445800B7EDE030B`.

### Focused QEMU gate

The requested `-Phase29COnly -BootCount 5 -TimeoutSeconds 180` run stopped at boot 2, as required after the first failed boot. Evidence is in `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-7e51c1e0bc2148389fc08c5667ed14dc`. Boot 1 reached project `ready`. Boot 2 used a unique staged image whose audit passed and showed the application and generation initialized as `app=1 generation=1`; the sentinel was present; the first frame, event loop, first iteration, and startup pump were reached; and exactly one `/P28Q` request (`request=1`) was created, submitted, and accepted by Phase 29C (`project_request=1`) at `load_started`.

Boot 2 then failed inside the synchronous Phase 29C transaction while reading `/P28Q/app/app.json`, reporting `manifest_identity_mismatch` before `loaded`, commit, or `ready`. The staged `guidexos.project` and `app/app.json` contents matched boot 1, and the image audit hashes matched. Boot 1 committed and reached `ready`; boot 2 did not. This is the first unpassed boundary in the focused run and is classified as **E — Phase 29C regression / transaction failure after `load_started`**. The transaction's exact mismatching field was not reported, so its root cause remains unresolved.

The marker-boundary correction and harness wait adjustment described above were made after this QEMU run. A separate one-boot diagnostic run using the final code passed; evidence is in `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-7a32587d73444e80bf18ce82f7ff240b`. The handoff marker appeared at `load_started`, and the harness waited for the transaction return and observed `ready`. That diagnostic did not replace or extend the failed 5-boot acceptance run. Its result remains 1 completed boot followed by a boot 2 failure; no replacement five-boot sequence was counted.

### Full acceptance and stress

The 10/10 Phase 28Q acceptance gate was not started because the required 5/5 focused gate failed. The first later Phase 28Q continuation marker was therefore not evaluated. The 25-cycle stress gate was not started because its 10/10 prerequisite was not met. Physical hardware was not tested; this validation used QEMU and the hosted Server.

## Outcome

**Outcome B.** The observed fresh boots prove the startup owner can recognize the fixture and publish exactly one generation-correct request through Phase 29C `load_started`. The historical pre-request stall was not reproduced or conclusively explained. The required focused gate failed on a later Phase 29C `manifest_identity_mismatch`, so deterministic end-to-end project readiness and the dependent 10/10 and 25-cycle gates remain unproven.
