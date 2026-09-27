# Developer Studio Phase 29F — Debug-Start Admission and Handoff Stabilization

## Scope and starting state

Phase 29F follows Phase 29E and isolates the boundary from a committed, ready project through debug-start validation, artifact handoff, server admission, and client observation of `RUNNING`. It does not reopen the Phase 29E manifest parser, Phase 29C project transaction, Phase 29A source mapping, or the repaired Phase 29B execution ordering.

| Repository | Branch | Starting HEAD |
|---|---|---|
| Standalone Developer Studio | `main` | `ac79846c077bd11ad1de1a7f29291bedd169d86d` |
| Server integration | `v0.5_DEVELOPER_STUDIO` | `a8a31d3c0de6c031a203587ea45f77cd816998d2` |

The Phase 29E worktrees were clean, and both repositories were reported as one commit ahead with no commits to pull. Authentication and remotes were left unchanged. No branch switch, merge, rebase, reset, or history rewrite was performed.

## Phase 29E baseline and original failure

Phase 29E established deterministic manifest reads and identity validation. `/P28Q/app/app.json` was 570 bytes with FNV-1a-64 `0xF67BEB16CE32BC56`; 1,000 production repetitions passed, CTest was 32/32, DWARF capacity was `PASS, dies=397`, package audits passed, and five focused manifest boots passed.

The Phase 29E full acceptance boot reached project `ready`, accepted debug start, and entered the server-owned Phase 29B lifecycle. Its serial log is:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-d60e38eeb3804301b2a1375f02e2892e\boot1.serial.log`

The request was received. The target and execution owner were created, the entry dispatch returned, the release command was consumed, and the first execution dispatch returned with the controller in `Running`. The log then recorded `NativeElf: external_close_request id=000003E8`; the controller progressed through stopping to exited. Phase 28Q subsequently emitted `DEVELOPER_STUDIO_PHASE28Q_FAIL reason=debug_start`. It had not emitted `DEVELOPER_STUDIO_PHASE28Q_RUNNING_PASS`; Pause, stop mapping, Continue/Step, exit proof, and teardown were not reached. The log has no Phase 29F close-source trace, so it cannot identify which caller requested that close.

The original Phase 28Q failure branch was:

```cpp
if (g_debugController.state == DebugSessionState::Failed ||
    (!g_debugWaitingForBuild && !DebugControllerIsActive(&g_debugController))) {
    phase28qFail(ctx, "debug_start");
}
```

This collapsed a target that exited before client observation, a client-side start rejection, and several other inactive states into the same string. For the Phase 29E timeline, the precise state condition is now `DEBUG_START_TARGET_EXITED_BEFORE_CLIENT_RUNNING_OBSERVED`: the controller is `Exited`, the client has not observed `Running`, and Phase 28Q has not published its RUNNING marker. The external close that preceded that state remains unattributed.

## Start path and ownership contract

The traced production path is:

1. Phase 28Q stage 2 calls `requestDebug` once. It captures Developer Studio startup generation, debug request sequence, current project request ID, current active project generation, candidate/lifecycle generation, and project ID.
2. Request validation requires the open current project to be `ready`, supports the selected Native GUI target kind, and rejects active run/build/debug operations or dirty documents.
3. `beginDebugBuild` checks the captured project/request/lifecycle identity again and starts the real `BuildController` operation. Its completion returns through `pollBuild`.
4. `beginDebugSession` verifies that the captured project is still the active ready project, that the build request has the same project ID and root, and that the successful build result contains a valid executable, expected path, SHA-256, and matching architecture.
5. `DebugTargetFromBuild` creates the runnable target. `loadDebugSymbolsForTarget` stats and reads the exact artifact size, recomputes SHA-256, and loads the source/symbol map before any server start request.
6. `DebugControllerStart` submits the current target through the registered debugger backend to the Server development-run service.
7. The Server validates the service handle, registered operation, deployment/artifact identity, and current service generation before admission and target creation.
8. Phase 29B creates the execution owner, dispatches the entry target to its first stop, consumes the explicit release command, executes the first target instruction, and only then publishes authoritative `Running`.
9. Developer Studio's main-loop controller poll must return with `Running`; the client then publishes its ready state and Phase 28Q emits `DEVELOPER_STUDIO_PHASE28Q_RUNNING_PASS`.

The build result does not expose a separate opaque project-generation field. Ownership is carried by the single request's captured project generation plus the build operation ID and the build request's project ID/root; those are revalidated together before target construction. The debug target and server operation carry the project/artifact identity used by the request. The Phase 29F snapshots log the captured and active generations separately instead of treating the project `ready` state as proof that a runnable artifact exists.

The tested current artifact was AMD64 `build/bin/amd64/p28q.elf`, 10,403 bytes, SHA-256 `9bae6ac4450bef62ae05d76804987c6d3ad213ec0b5a1e7d692fe5e9e66f2900`. The successful trace reported the same actual and expected artifact path. The Server received that path, size, and hash. No automatic rebuild retry was introduced.

The service is found through the registered debugger launch callback. A successful start showed the operation first allocated as handle 1, then registered with service generation 1; the transient operation-created marker precedes generation assignment. The Server rejected stale/invalid handles and non-registered states explicitly, and recorded registration, admission, target creation, first dispatch, release, first execution, RUNNING publication, close handoff, and retirement. No sleep or repeated command was added.

## Detailed result and trace changes

Phase 29F adds bounded `DEVELOPER_STUDIO_PHASE29F_DEBUG_START_*` client and Server markers. They cover request intent/ownership, project and build identity, artifact path/hash/architecture, symbol load, service lookup, handle registration, API receipt, admission, owner creation, first dispatch, release, first execution, authoritative RUNNING, client poll, and Phase 28Q observation. Client return markers are one-shot; there is no per-frame or per-instruction trace.

The Phase 28Q failure result now uses state-specific bounded names, including:

- `DEBUG_START_TARGET_EXITED_BEFORE_CLIENT_RUNNING_OBSERVED`
- `DEBUG_START_CLIENT_RUNNING_OBSERVED_BUT_PHASE28Q_MARKER_MISSING`
- `DEBUG_START_RUNNING_WITHOUT_PHASE28Q_OBSERVATION`
- `DEBUG_START_BUILD_OR_SESSION_NOT_ACTIVE`
- `DEBUG_START_CONTROLLER_FAILED`

The first client-side rejection is latched through the asynchronous build/start boundary and takes precedence over the later controller state. It can preserve results such as `DEBUG_START_PROJECT_NOT_READY`, `DEBUG_START_PROJECT_GENERATION_MISMATCH`, `DEBUG_START_ARTIFACT_MISSING_OR_INVALID`, `DEBUG_START_ARTIFACT_IDENTITY_MISMATCH`, `DEBUG_START_ARTIFACT_ARCHITECTURE_MISMATCH`, `DEBUG_START_BUILD_FAILED`, `DEBUG_START_CONTROLLER_START_REJECTED`, and `DEBUG_START_SYMBOL_SOURCE_NOT_FOUND`. Symbol-load diagnostics now include project root/generation, artifact path, mapper counts, and the bounded source path that failed project-root association.

The new focused runner mode `-Phase29FDebugStartOnly` requires every client/server start boundary through `DEVELOPER_STUDIO_PHASE28Q_RUNNING_PASS` and stops on either that marker or a Phase 28Q failure. Its mode no longer treats the generic kernel main-loop marker as proof completion.

## Boot evidence and fault classification

### Original Phase 29E failure

The Phase 29E boot proves the request passed client validation and server admission and entered Phase 29B. Its first failed observation is client RUNNING publication: the target was closed and the controller exited before Phase 28Q observed RUNNING. That is classification **G, observation/result propagation**, with an unattributed close request as the preceding event. It is not a server admission or target allocation rejection.

### Focused five-boot attempt

The first Phase 29F focused attempt passed boot 1, then failed boot 2. The boot-2 log is:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-15bfe6cb542847a4b7b858edf0263a22\boot2.serial.log`

Boot 2 had app generation 1, project request 1, debug request generation 1, active project generation 1, candidate/lifecycle generation 1, project ID `dev.guidexos.phase28q`, state `ready`, and build operation 2. The artifact identity was the expected path/hash/size/architecture above. Service handle 1 was registered as generation 1. The server received and admitted the request, created the execution owner, returned from entry dispatch, consumed the release, returned from first execution dispatch, and published authoritative `Running`.

The exact last Server stage was `DEBUG_START_AUTHORITATIVE_RUNNING`. On the client, the last emitted line was `Debug: launching dev.guidexos.phase28q`, written at the end of `beginDebugSession`. The following `BEGIN_DEBUG_SESSION_RETURN`, `DEBUG_START_BEGIN_SESSION_RETURNED`, build-poll return, `DEBUG_START_MAIN_LOOP_AFTER_BUILD_POLL`, and `DEBUG_START_CLIENT_DEBUG_POLL_ENTERED` markers are all absent. Thus the trace narrows the failure to the tail of `beginDebugSession` or its immediate return path, after the Server RUNNING publication but before control returned through `pollBuild` to the main-loop debugger poll. No explicit Server rejection occurred, and no client running-observation result was produced. The serial trace cannot distinguish a stall inside the final output/log call from a stall immediately after it, so the underlying client-side stall remains unattributed. This focused boot is classification **G**, and it breaks the 5/5 gate. The run stopped at 1 pass followed by 1 failure; later boots were not counted as continuations of that gate.

### Separate client-side symbol rejection

A separate diagnostic boot before the failure-code latch reached project `ready` but stopped before server handoff. The artifact was valid and had the same size, path, architecture, and SHA-256. `DebugDwarfMapperLoad` returned `source_not_found`; the old diagnostic did not retain the raw rejected source path, and Phase 28Q then reduced the idle controller to `DEBUG_START_BUILD_OR_SESSION_NOT_ACTIVE`. This was an observed client validation failure, classification **A**, distinct from the boot-2 server-RUNNING/client-observation failure. The raw path was not available from that old serial log, so it is not asserted as the cause of the Phase 29E failure.

The mapper now preserves and reports the bounded rejected source path and project root on that error. A later one-boot focused diagnostic with the updated package reached the client poll and `DEVELOPER_STUDIO_PHASE28Q_RUNNING_PASS`; it did not reproduce the source rejection. This one diagnostic is not a 5/5 gate and does not repair the earlier failed boot.

### Successful current start control

The final one-boot diagnostic log is:

`C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-ce4d4b3d7b7241af8ec85cd21154a4a8\boot1.serial.log`

It passed request ownership, artifact validation, registered service handle 1/generation 1, server receipt/admission, target and execution-owner creation, entry dispatch, release, first execution, authoritative RUNNING, client poll return, client RUNNING observation, and the Phase 28Q RUNNING marker. This verifies the instrumented path can succeed but does not remove the boot-2 failure from the focused gate.

## Phase 29B invariants

The Phase 29F changes preserve the Phase 29B ordering. `Running` is not made observable before first dispatch/execution. The successful focused trace shows server admission, execution-owner creation, entry dispatch return, release consumption, first execution return, authoritative Server RUNNING, client poll return, and client observation in that order. The Phase 29F change adds instrumentation and first-failure result propagation; it does not alter scheduler admission or early-publish RUNNING.

## Tests and validation

- The debugger result-contract unit test now verifies state-derived diagnosis and that a latched precise client rejection (for example `DEBUG_START_SYMBOL_SOURCE_NOT_FOUND`) is not overwritten by a later `Idle` controller state.
- CMake native build: PASS.
- Complete CTest: **32/32 passed**.
- DWARF capacity: **PASS, dies=397**.
- AMD64/ARM64 package builds and DWARF locals/arguments checks: PASS.
- Package content/ELF audit: PASS for both packages; exactly `app.json` plus the two architecture ELFs.
- Hosted project/parser/build model validations: PASS in the package validation runs.
- Representative hosted debugger lifecycle: PASS after the Phase 29F changes. It reached the source breakpoint, call stack, locals, Continue/rebind, orderly teardown, and Server exit code 0.
- Bounded hosted repetition: `-Iterations 3` stopped on its first failure, so only iteration 1 ran. It reached hosted `Running` and orderly teardown but did not register the scripted source breakpoint; `BEGIN_DEBUG_SESSION_BREAKPOINTS_READY` showed zero workspace/controller breakpoints. Iterations 2–3 were not run. This is a separate hosted breakpoint-input failure, not evidence of a debug-start admission rejection.
- Focused debug-start gate: **1/2 attempted boots**; boot 1 passed and boot 2 failed, breaking the 5/5 gate. Boots 3–5 were not run. One later isolated diagnostic passed, but it is not counted toward that gate.
- The separate one-boot Phase 28Q diagnostic that reached RUNNING later emitted `DEVELOPER_STUDIO_PHASE29A_STOP_MAPPING_FAIL` with `source_gen=0` and `debug_editor_execution=UNAVAILABLE`; the harness's first missing required marker was `DEVELOPER_STUDIO_PHASE29A_STOP_MAPPING_PASS`. This is the first later independent Phase 28Q blocker. The Phase 28Q 10-boot acceptance was not run because the prerequisite 5/5 focused gate did not pass.
- The 25-cycle lifecycle stress was not run because the 10/10 acceptance prerequisite was not met.
- Physical AMD64/ARM64 hardware validation was not performed.

## Package identities

| Architecture | SHA-256 | Audit |
|---|---|---|
| AMD64 | `D49C8358F47C11EF04130E88DC2DEA9A6EE23F1247229A05007DEA4A93A2AC7E` | PASS, ELF64 AMD64 ET_EXEC |
| ARM64 | `9FC88F032AC8811A5C7B7DBB728834F0D70D1425A5BE108D1A5D084ED60FC34A` | PASS, ELF64 AArch64 ET_EXEC |

## Root cause, repair, and remaining fault domain

The generic Phase 29E string was caused by Phase 28Q treating every inactive/failed controller as `debug_start`. The historical trace narrows that boot to a target that exited before client RUNNING observation, after an external close request; the close origin is not present in the old evidence. The Phase 29F focused gate independently proves that a request can be received, admitted, instantiated, dispatched, executed, and published as authoritative Server RUNNING while the client fails to publish the RUNNING observation.

The repair in this phase is diagnostic/result-propagation stabilization: first client rejection is retained across async build completion, Phase 28Q reports the specific rejected boundary or controller state, symbol path failures retain their root/path context, and Server/client markers expose the start lifecycle. No scheduler or validation bypass was added. The unexplained early close/client observation failure is not claimed repaired, and the intermittent symbol source rejection was not reproduced with enough evidence to justify a behavioral change.

The remaining fault domain is the tail of client `beginDebugSession` after the Server has published RUNNING and before `pollBuild`/the main-loop debugger poll resumes. The exact next missing marker is `BEGIN_DEBUG_SESSION_RETURN`; consequently `DEBUG_START_CLIENT_DEBUG_POLL_ENTERED`, `DEBUG_START_CLIENT_RUNNING_OBSERVED`, `DEBUG_START_AUTHORITATIVE_RUNNING_READY`, and `DEVELOPER_STUDIO_PHASE28Q_RUNNING_PASS` are not reached. The separate `source_not_found` source-root/path association also remains unresolved when it recurs.

## Outcome

**Outcome B.** The original `debug_start` result is no longer opaque: its historical condition is identified as target exit before client RUNNING observation, and a separate client-side `source_not_found` branch is preserved distinctly. The 5/5 focused gate failed on boot 2, so full 10/10 acceptance and 25-cycle stress remain unrun. Phase 29F has not established the repeated fresh-boot result needed for Outcome A.
