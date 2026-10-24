# Developer Studio COMP3R3 — Kernel Build Job Foundation

**Disposition: Outcome A — build job foundation accepted**

**Scope: one-slot job ownership and lifecycle foundation; no asynchronous compiler execution or guest acceptance.**

## Decision

Server v0.5 now has a fixed-capacity, kernel-owned build-job manager with deep-copied request strings, runtime-owner checks, generation-checked handles, coherent snapshots, queued cancellation, safe release, owner-close cleanup, and a shutdown admission barrier. The existing NativeElf build callback still invokes the existing synchronous compiler path. The only continuation that advances in bounded steps is explicitly test-only and has no path to the compiler, VFS, application memory, or artifact writer.

The foundation is accepted under Outcome A. This does not accept asynchronous Build behavior, compiler stack sufficiency at runtime, UI responsiveness, in-flight compiler cancellation, or guest Build/Run/Debug behavior. The existing compiler trampoline remains on its current 128 KiB static service stack. COMP3R2's 242,528-byte static frame accounting remains a warning that exceeds that configured size; no guest run, guard measurement, or high-water measurement qualifies it. A guarded stack and measured budget remain COMP3R4 prerequisites.

## Stale-prompt gate

At the start of COMP3R3:

| Repository | Branch / starting HEAD | Starting state |
|---|---|---|
| Developer Studio | `main` / `397a63d7bcc23203adc0c08da43def507e5ef863` | Tracked tree clean; `origin/main` matched. |
| Server v0.5 Developer Studio | `v0.5_DEVELOPER_STUDIO` / `0e8e7e343129d44d5c1bddd422dbb5a290879b0d` | Tracked tree clean; `origin/v0.5_DEVELOPER_STUDIO` matched. Existing validation output directories were untracked and preserved. |
| Existing Server detached worktree | detached / `bf78d517b513b624f61bee4ae6d144377f6c8000` | Existing phase29w worktree contained pre-existing tracked deletions; left untouched. |

The COMP3R2 report was read and its decisions were verified against current code. COMP3R2 was architecture-only and had not implemented the manager. No COMP3R3 marker or implementation existed in either primary repository. No authoritative `.phase` marker existed in those repositories. Compiler, NativeElf, scheduler, VFS, and build-service files had no pre-existing tracked edits in the primary Server worktree.

The review reconfirmed COMP3 and COMP3R1 reports, COMP1 and COMP2 pending status, Phase 30B pending status, and the deferred Phase 30C status. QAC's separate read-only state showed QAC1/2/3A/3B/3C1/3C2 accepted, QAC3 open, and QAC3C3 next. Server v0.3 was read-only and not changed. No v0.3 Developer Studio integration was resumed.

## Scheduler and callback boundary

The AMD64 main loop calls `desktop::tick()` in normal kernel context after input and network polling. Interrupts are enabled during normal desktop operation; the PIT interrupt advances timer state and does not invoke the build manager. `desktop::tick()` updates the app manager and then calls the existing `NativeElfRunService::native_elf_scheduler_pump()` when no NativeElf target is currently running. The pump has its own active-target and reentrancy guards. The NativeElf `host_native_window_run` path also calls `desktop::tick()` while the target is active; the target-state guard prevents a nested scheduler pump there.

That is an existing specialized NativeElf continuation boundary, not a general worker API. `desktop::tick()` does not call the build manager's test pump. The test continuation is exercised by the hosted test only. The synchronous `build_project_start` callback copies the request and calls `BareMetalBuildService::start_owned`, which invokes the existing build trampoline and VFS/compiler pipeline before returning. Thus it does not call the compiler from a desktop update callback, and Studio cannot poll while this synchronous callback is still running.

The VFS uses global file, directory, and mount state without an observed snapshot transaction, mount lease, or synchronization contract. Its operations are synchronous. The manager's lock protects only its bounded metadata copies and snapshot publication; no VFS or compiler operation runs while that lock is held. No callback or interrupt path currently reenters the build service while its synchronous compiler call is active. NativeElf runs at kernel privilege, so the added identity check is lifecycle ownership validation, not an isolation boundary.

## Production implementation

The Server adds `kernel/core/compiler/compiler_build_job_manager.h/.cpp` and routes the existing build service through it. The manager contains one fixed slot and no dynamic allocation or queue. The hosted test measured `sizeof(Manager) == 27,352` bytes, with capacity one. Its source membership bound is statically checked against `COMPILER_MAX_TRANSLATION_UNITS` (16).

The manager retains eight request strings, each at most 767 bytes plus a terminator, a copied `gx_build_request`, up to 16 entries of full path, relative path, and object path (each 256 bytes), and a `gx_build_snapshot`. It retains no caller string pointer. Source membership is copied after the existing service has validated project metadata and selected sources. The current compiler still reads source files through VFS during the synchronous pipeline; the retained membership is not an immutable source-content snapshot.

NativeElf assigns a monotonically increasing 64-bit runtime identity in the kernel when preparing each application instance. Build start/poll/release callbacks pass this identity to the manager. A reserved identity remains for existing kernel smoke callers. The identity is not supplied by the app and is stored only in the internal kernel runtime structure; the SDK build structs and host callback table are unchanged. Since NativeElf code shares kernel privilege, the identity does not make a sandbox.

Handles encode a nonzero monotonically increasing 32-bit generation in the upper half and slot id `1` in the lower half. The manager refuses admission before generation wrap can occur. Every poll, release, and queued cancel checks slot liveness, the exact handle/generation, and owner identity. A released handle is invalid; a slot is reused only after safe release or owner/shutdown cleanup.

### States and operations

The implemented internal states are `FREE`, `PREPARING`, `QUEUED`, `RUNNING`, `COMPLETING`, `SUCCEEDED`, `FAILED`, and `CANCELLED`.

| Transition | Cause |
|---|---|
| `FREE → PREPARING` | Valid API header and bounded request copy; one slot admitted. |
| `PREPARING → QUEUED` | Existing project metadata and source selection pass; source membership is copied. |
| `QUEUED → RUNNING` | Existing synchronous build pipeline reaches its compiler stage. |
| `PREPARING/QUEUED/RUNNING → COMPLETING → terminal` | Synchronous service returns a terminal result; a success without a validated artifact identity is converted to failure. |
| `QUEUED → CANCELLED` | Test-only queued cancellation before work starts. |
| `QUEUED/RUNNING → CANCELLED` | Test-only continuation observes owner-close, cancellation, or shutdown at its safe step boundary. |
| terminal `→ FREE` | Valid owner release, owner closure, or shutdown after execution has quiesced. |

Admission returns deterministic `GX_ERROR_BUSY` for an occupied slot and `GX_ERROR_FAILED` after shutdown or generation exhaustion. Invalid request headers/string bounds are rejected before slot activation. Existing project/source validation stays inside the synchronous service, before the job can transition to `QUEUED`. A failed synchronous build retains its terminal snapshot until release, preserving the existing start/poll/release shape.

Polling takes the manager lock, validates owner and generation, and copies the entire fixed-size snapshot before unlocking. Compiler/VFS work happens outside the manager lock in a separate synchronous working snapshot. Terminal publication copies the completed snapshot once and freezes it until release. The manager rejects release while synchronous work or the test pump is active; double release and stale handles return invalid-argument errors.

### Test continuation and cancellation

`pump_one_test_step()` performs exactly one bounded arithmetic step and publishes one test-only progress line. It does not block, access VFS/compiler state, retain caller memory, switch stacks, or create an artifact. After three steps it publishes `FAILED` with an explicit message that no compiler or artifact work ran. It is not called by guest callbacks or the desktop scheduler.

Queued cancellation is supported through the internal manager operation. The production guest ABI does not expose a cancel callback, and an active synchronous compiler cannot be interrupted. Attempting to release active work returns Busy. The test continuation checks cancellation, owner-close, and shutdown at its next safe step boundary; it never reports cancellation while work still retains manager state.

## Owner close and server shutdown

NativeElf teardown calls `owner_closed(runtimeIdentity)` before clearing the app's image or stack. A queued or terminal job is safely cancelled/discarded and the slot is reclaimed; a stale owner cannot poll or release it. If an active call still retains manager storage, `owner_closed` marks the owner invalid and returns Busy. Teardown then defers memory reclamation until the call reaches its safe completion path. Completion sees the invalidated owner and clears the slot instead of preserving an app-readable result.

The bare-metal desktop shutdown path calls the build-service shutdown barrier before issuing platform power-off operations. The manager first rejects new admissions and marks safe cancellation intent. If a synchronous call or test pump is still active, shutdown returns Busy and the desktop path defers shutdown rather than reclaiming live state. Once execution is quiescent, shutdown publishes cleanup and clears the slot. There is no detached worker and no compiler code can outlive the manager or VFS through this foundation.

## Stack, VFS, and artifact limits

No build-service stack size, compiler recursion limit, compiler language behavior, linker behavior, object-cache format, or artifact publication path changed. The service still uses its existing `NativeElfTrampoline` invocation on the static 128 KiB build-service stack. The prior compile-only audit's approximately 242,528 bytes of nested static frames remains larger than that configured stack. This phase did not run a guest build or prove the stack's runtime high-water, guard placement, or safety. The value 384 KiB remains only a provisional candidate and was not applied.

Before a future compiler continuation can execute, COMP3R4 must provide a supported dedicated stack allocation with a guard/canary, an enforced lifetime through quiescence, stack alignment checks, and high-water measurement. Qualification should cover cache hit and miss, the deepest compiler fixture, recursion limit 679 plus rejection of the next activation, maximum translation-unit/dependency bounds, multiple translation units, linker and artifact validation frames, failure cleanup, and repeated builds. Memory impact and overflow behavior must be measured. Static frame sums alone do not establish safety.

The manager owns request and membership metadata only. It does not snapshot source bytes, included headers, cache inputs, or VFS mount state. A future worker must explicitly bind compiled bytes and dependencies to the admitted project identity, record the exact source content compiled, detect source edits, avoid retaining VFS handles across yields, and distinguish source freshness from current saved content. The existing compiler/linker continues to publish to its existing expected artifact path. Test continuation work never enters that path. A future asynchronous build still needs generation-specific output staging, validation/hash before publication, and current-source freshness checks.

## ABI and compatibility

The public `gx_build_request`, `gx_build_snapshot`, build host-call offsets, and SDK capability surface did not change. The existing `build_project_start`, `build_project_poll`, and `build_project_release` callbacks retain synchronous start behavior and their old result contract. Internal kernel smoke callers keep the old service entry points through a reserved kernel owner identity. No async capability is advertised. The existing compiler, AMD64 backend, parser/type limits, object cache, internal linker, ELF writer, diagnostics, incremental behavior, and NativeElf validator remain in place.

## Qualification evidence

| Check | Result |
|---|---|
| Hosted production-manager lifecycle test | PASS; one slot, 27,352-byte manager. Covers deep copy after caller mutation, invalid header, admission/Busy, owner rejection, state transitions, stable terminal snapshot, invalid/double/stale handles, slot reuse, queued cancellation, owner close, shutdown barrier, generation exhaustion, and concurrent poll/admission. |
| Server compiler-script group | 9/9 PASS. Includes compiler functions and structs suites carrying COMP3R1 recursion-boundary coverage. |
| NativeElf and ABI group | 4/4 PASS: ELF validator, runtime, trampoline nonvolatile-state check, and native ABI layout. |
| AMD64 freestanding affected-object compilation | 9/9 PASS with production `kernel/Makefile` and `arch/amd64/Makefile.arch` flags. Objects: build-job manager, build service, NativeElf loader, NativeElf run service, NativeElf smoke, desktop, main, shell, AMD64 syscall. This was object compilation only, not a full kernel link/build. |
| Developer Studio CTest | Not run: no Studio source or shared Build API changed; only this report was added there. |
| `git diff --check` | Run at final qualification before commit; outcome recorded in the task completion message. |
| QEMU | Not launched. No exclusive QAC acceptance reservation was requested or used. |

Validation outputs were isolated under Server `out/comp3r3-validation/` and `kernel/out/comp3r3-validation/`. Pre-existing `out/comp2-validation/`, `out/comp3-validation/`, `out/comp3r1-validation/`, and `out/comp3r2-validation/` were preserved. All generated evidence remains outside the implementation commit.

The Server implementation and hosted test are committed on the existing `v0.5_DEVELOPER_STUDIO` branch as `f2aa597774f512a390ad3dc71b56ee482e2d8fce`. An ordinary push was attempted and failed with `Permission denied (publickey)`; no remote or authentication settings were changed. The Studio report is committed separately on the existing `main` branch.

## Remaining gates and result

COMP3R3 establishes the one-slot kernel-owned job lifecycle foundation. It does not complete COMP3 guest responsiveness or any guest lifecycle acceptance. COMP1 remains 0/3, COMP2 remains 0/3, Phase 30B remains 0/5, COMP3 guest responsiveness remains unqualified, overall QAC3 remains open, and Phase 30C remains not started.

COMP3R4 prerequisites are a qualified guarded compiler stack, real resumable compiler stages with measured bounds, and a safe update-boundary pump contract for build jobs. Later phases must address immutable source/dependency identity, artifact staging/freshness, real progress, cancellation safe points, and guest responsiveness. The foundation's hosted continuation cannot serve as that evidence.

**COMP3R3 Outcome A is limited to the build-job foundation. COMP3R4, COMP4, and Phase 30C were not started.**
