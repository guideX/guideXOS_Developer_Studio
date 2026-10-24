# Developer Studio COMP3R4 — Cooperative Continuation and Stack Safety

**Disposition: Outcome A — continuation foundation accepted**

**Scope:** One guarded desktop-boundary connection for the existing internal test continuation, plus a current compiler stack and memory audit. No compiler work was moved into the continuation, and no guest responsiveness or runtime stack-safety claim is made.

## Decision

The kernel-owned build-job manager now receives one test-only continuation opportunity at the normal desktop update boundary. A permitted call advances exactly one fixed test step; a nested call while a NativeElf target owns the context is rejected. The test continuation ends in `FAILED`, publishes no artifact identity, and is inaccessible through application Build callbacks. The synchronous compiler callback and its existing service stack remain unchanged.

This meets COMP3R4's continuation-foundation outcome. It does not establish asynchronous compilation, UI responsiveness during compilation, or safety of the current compiler stack. The 128 KiB compiler service stack remains smaller than the 242,512-byte conservative sum of three production-path static frames. No stack guard or runtime high-water measurement exists. A 384 KiB stack remains unqualified and must not be used as a presumed-safe value.

## 1. Stale-prompt gate and repository state

At task start, both primary worktrees had clean tracked state on their expected branches and historical COMP3R3 HEADs. No COMP3R4 report or marker existed. COMP3R2 and COMP3R3 reports were read in full and their assumptions were checked against current Server code. No conflicting tracked changes were found. Existing detached worktree and untracked validation evidence were preserved.

| Repository | Start branch / HEAD | Start state | End state |
|---|---|---|---|
| Developer Studio (`D:\dev\guideXOS_Developer_Studio`) | `main` / `c13628d626dad61f6bb4ba9c2619cc62466d2283` | Tracked tree clean; matched `origin/main`. | `main`; this report is committed. The final report commit is the ending HEAD; its hash is given in the task closeout because a commit cannot contain its own object ID. |
| Server v0.5 (`D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO`) | `v0.5_DEVELOPER_STUDIO` / `f2aa597774f512a390ad3dc71b56ee482e2d8fce` | Tracked tree clean; matched `origin/v0.5_DEVELOPER_STUDIO`. Pre-existing validation output directories were untracked and preserved. | `v0.5_DEVELOPER_STUDIO` at `9594543f` (`COMP3R4 connect test continuation to desktop boundary`); one local commit ahead. Push to existing `origin` failed with `Permission denied (publickey)`. Validation outputs remain untracked. |
| Existing Server detached worktree | detached / `bf78d517b513b624f61bee4ae6d144377f6c8000` | Pre-existing worktree preserved. | Untouched. |

No Server v0.3, QAC source, Studio production source, shared API, phase marker, branch, worktree, remote, or authentication setting was changed. COMP3R4 had not already started, completed, or been superseded. The Server retained its pre-existing untracked evidence under `kernel/out/comp3r3-validation/`, `out/comp2-validation/`, `out/comp3-validation/`, `out/comp3r1-validation/`, `out/comp3r2-validation/`, and `out/comp3r3-validation/`. New validation output is isolated under `out/comp3r4-validation/` and `kernel/out/comp3r4-validation/`.

The QAC read-only marker says `QAC3C2: accepted`, `QAC3: open`, `Next: QAC3C3`. Phase 30B remains Outcome B, 0/5. No exclusive QAC reservation was obtained or needed, and no QEMU process was launched. COMP3R2's architecture and COMP3R3's one-slot ownership/lifecycle foundation remain valid: this change adds only a test-pump boundary and cancellation-at-step behavior; it does not alter the production compiler path or public contract.

## 2. Desktop update and scheduling contract

On AMD64, `kernel_main` initializes/enables interrupts at `kernel/core/main.cpp:291`, registers a 100 Hz PIT at lines 294–297, and enters the main loop. The loop calls `desktop::tick()` at line 610 after input/network work, then records work ticks and idles. Normal desktop work therefore runs on the kernel main/boot stack with interrupts enabled. The PIT IRQ updates timer state; it does not call the build manager. The boot stack is a 16 KiB static region (`kernel/arch/amd64/boot.asm:39–43`).

`desktop::tick()` updates desktop services, apps, and taskbar state before its bare-metal continuation boundary (`kernel/core/desktop.cpp`, near line 8431). It now reads `native_elf_scheduler_in_target()`, invokes `BareMetalBuildService::pump_cooperative_test_step()`, and calls the existing NativeElf scheduler pump only when a NativeElf target was not already active. This build call is bounded and does not dispatch GUI work or invoke the compiler.

NativeElf has a specialized same-CPU cooperative context switch and its own reentrancy checks (`native_elf_run_service.cpp`: `native_elf_scheduler_in_target` and `native_elf_scheduler_pump`). The scheduler refuses a pump while the target or another pump is active. A running application can call the event-poll helper, which reaches `desktop::tick()` while the target context is active; the new build wrapper sees that state and returns `GX_ERROR_BUSY`. It cannot recursively advance a build continuation. The NativeElf pump retains its own guard and is not treated as a generic worker API.

No VFS or compiler lock is held by the added desktop call. The manager lock covers bounded job metadata/snapshot operations only. There is no simultaneous compiler/linker execution from this pump because admission marks the continuation `testOnly`, the private work step rejects non-test jobs, and the synchronous compiler path is separate. Shutdown admission remains enforced by the manager; a shut-down manager cannot advance work. Application owner closure and generation validation are handled before any later step. The test work is not an application callback, and no GUI dispatch is called from the manager.

The boundary is qualified for this tiny manager-owned step by its source wiring check and production-manager hosted tests. It is not evidence that arbitrary VFS/compiler work is safe or bounded there, nor a guest runtime demonstration.

## 3. Continuation connection and state integrity

Changes are limited to the Server's build-job manager, service wrapper, desktop tick, and manager test/runner. The public `BuildProjectStart`, `BuildProjectPoll`, and `BuildProjectRelease` paths do not call the new pump. `pump_cooperative_test_step()` queries the real NativeElf target state and delegates to `Manager::pump_one_test_step_at_desktop_boundary()`. That method returns Busy when the target is active. The actual private test pump is callable only for an admitted `testOnly` job and advances one internal unit.

The hosted test verifies: target-active rejection leaves the snapshot unchanged; each eligible call adds exactly one `TEST ONLY: continuation step N/3` line; the third step reaches deliberate `FAILED`; a terminal snapshot remains byte-for-byte stable; no pump acts after release; slot reuse receives a different generation; the old handle cannot see or advance the new job; owner close between steps clears the job before another unit; and shutdown between steps rejects further work. It also requests cancellation after the first step and observes `CANCELLED` at the next boundary, before any second arithmetic step. Cancellation is not reported before the boundary acknowledges it.

The cancellation request is an internal test continuation operation, not a new application ABI. Queued cancellation keeps its existing behavior. Release while a pump is active remains refused, and owner-close/shutdown do not reclaim manager state while a step is active. There is no detached worker or retained stack pointer.

State/progress is truthful: an internal test job begins `PREPARING`, becomes `RUNNING` only when its first step executes, and reaches either `FAILED` after the three-step test or `CANCELLED` at an acknowledged safe boundary. Poll copies the fixed snapshot under the manager lock. Terminal snapshots do not mutate. Artifact validity, size, entry point, path, and hash remain empty/zero; no compiler success, ELF hash, source map, Run/Debug eligibility, or build percentage is emitted.

The normal production compiler path remains `build_project_start → start_owned → run_build → run_build_core` (`compiler_build_service.cpp`, around lines 837, 1005, and 1026). It enters `run_build_core` synchronously through `invoke_native_entry_on_stack` on the existing 128 KiB service stack, performs its existing VFS/compiler/linker/ELF validation/publication work, and returns before `start_owned` returns. Studio cannot poll that callback while it is executing. The callback ABI and behavior are unchanged.

## 4. First genuine work-stage feasibility

No genuine compiler/build stage was integrated.

| Candidate | Finding |
|---|---|
| Request validation and deep copy | Bounded and already performed at admission, but admission is reached synchronously from the Build callback. Splitting this alone does not give the UI an independently advancing production job. |
| Project metadata and source/dependency enumeration | Uses synchronous reads/stat/list against global mutable VFS tables. No mount lease, immutable VFS snapshot, or transaction/synchronization contract was found. A project path does not pin saved source identity. |
| Source input staging | The current pipeline reads files synchronously into compiler workspaces. Making this resumable requires owned bounded bytes and source/dependency identity across yields; this does not exist. |
| Per-file compiler preparation/compile | Lexer, parser, IR, symbols, diagnostics, source/include buffers, and code-generation state are tied to local or shared compiler state. Yielding would retain stack-local addresses or shared workspace state. |
| Linker/ELF writer/publication | Operates on the synchronous pipeline's object/artifact workspaces and has no resumable state machine or generation-staged publication contract. |

The smallest safe next refactor is an explicit bounded source/metadata snapshot operation with kernel-owned bytes, dependency/path records, and stable identity, followed by compiler refactoring that moves all state required after a yield into an explicit bounded continuation record. It must preserve existing compiler logic rather than create a parallel pipeline. COMP3R4 did not claim VFS snapshot isolation, source freshness, or a successful preparation stage.

## 5. Compiler stack, ownership, and guard feasibility

Current production stack usage was regenerated from current AMD64 source using MinGW GCC 15.2.0 (`x86_64-w64-mingw32`) and the kernel Makefile's freestanding production flags, with `-fstack-usage`. Inputs, object files, `.su` files, exact flags, and logs are preserved under `kernel/out/comp3r4-validation/stack-usage/`.

| Function/path element | Static frame |
|---|---:|
| `run_build_core` | 14,896 bytes |
| `compile_project_impl` | 111,152 bytes |
| `load_cached_object` | 116,464 bytes |
| **Conservative nested known sum** | **242,512 bytes** |

The named frames represent the current cache-load path through `run_build_core → compile_project_impl → load_cached_object`. The sum exceeds the configured 128 KiB (`131,072` byte) build service stack by 111,440 bytes. It is a conservative static frame sum, not a measured runtime maximum. It omits deeper helper/callee frames, indirect calls, compiler recursion variation, assembly/trampoline frames, interrupt frames, and other path combinations. COMP3R2 reported 242,528 bytes; current compiler changes reduce the known sum by 16 bytes, which does not resolve the overage.

The largest individual frame is `run_bootstrap_smoke` at 323,344 bytes and is not on the production build-service path. Other notable frames include linker 16,928 bytes, AMD64 backend source-map path 5,392 bytes, parser maximum-entry path 1,456 bytes, and include expander 1,344 bytes. The compiler regression suite still exercises the COMP3R1 recursion boundary (limit 679, next activation rejected); that hosted recursion test does not qualify the kernel compiler stack.

The current stack is a static, 16-byte-aligned 128 KiB BSS array. No general kernel task-stack allocator, active stack guard, canary/high-water instrumentation, or runtime watermark exists. Stack-protector and stack-check instrumentation are disabled by production flags. A canary cannot safely be placed without a defined stack owner, boundary, interrupt headroom, and failure path. Filling an active stack to measure high-water would overwrite live frames. No manual stack switch or unsafe measurement was attempted.

The normal desktop/main stack is the 16 KiB boot stack. Static frames on the newly connected toy path are approximately 192 bytes (`kernel_main`) + 112 (`desktop::tick`) + 48 (service wrapper) + 8 (manager boundary) + 336 (private test step) = 696 bytes before nested lock/helper overhead. This is a compile-time estimate only; interrupts remain enabled and the actual nested IRQ/device path and watermark are unqualified. NativeElf's separate 32 KiB aligned cooperative scheduler stack does not become the compiler stack.

**Stack policy:** do not execute real compiler work from the desktop continuation and do not treat the existing 128 KiB stack as safe. Do not copy the hosted 8 MiB recursion-test stack. Do not adopt the proposed 384 KiB as safe by arithmetic alone. Before compiler continuation, COMP3R5 needs an explicitly owned, aligned stack allocation; enforceable boundary/guard or equivalent overflow detection; defined IRQ/preemption headroom; quiescence-based lifetime; fail-closed overflow handling; and measured high-water under cache hit/miss, recursion limit, maximum translation units/dependencies, linker, artifact validation, failures, and repeated builds. QEMU/runtime evidence remains pending.

## 6. Memory budget and state lifetime

Current manager size remains 27,352 bytes; COMP3R4 adds no persistent manager fields or second job slot. The one-slot manager retains eight bounded request strings, a copied request, bounded source-membership paths (up to 16 entries with full/relative/object paths), fixed flags/ownership/generation state, and a coherent fixed snapshot. It retains no source-content snapshot and no caller string pointer. The manager's bounded metadata is separate from compiler workspace storage.

The current selected AMD64 compiler/build-lane translation units account for 6,148,800 bytes of BSS: compiler driver 2,457,536; compiler module 1,104,576; parser 1,075,072; AMD64 backend 993,984; build service 379,104; compiler object 138,528; other selected TUs 0. This is a selected-TU subtotal, not total kernel BSS or linked-image memory; it excludes unrelated kernel data and linker folding. The build-service subtotal includes the manager singleton, work snapshot, project/manifest buffers, artifact buffer, 128 KiB service stack, and summary/static state.

For planning only, adding a bounded 2 MiB source snapshot and 160 KiB path/dependency records, while replacing the current 128 KiB stack with a 384 KiB stack, would raise this selected subtotal by 2,523,136 bytes to approximately 8,671,936 bytes. Adding a new 384 KiB stack without replacing the current one would add a further 393,216 bytes instead. These are estimates, not implemented allocations or a safe stack recommendation. No unbounded allocation or second compiler workspace was introduced.

Before yielding, compiler state that currently lives in automatic or global workspaces must be made explicit: lexer cursor/mode and token state; parser frame/recursion state; IR and symbol/type tables; source and include bytes plus mappings; object-cache references and ownership; diagnostics and source locations; code-generation buffers/state; linker inputs/symbol/relocation state; and ELF writer/hash/publication state. Current code does not define a resumable continuation object for these. No stack-local pointer may survive a return to the scheduler.

## 7. VFS, artifact, lifecycle, and ABI limits

The existing VFS uses global mutable state and synchronous operations. No immutable source snapshot, mount lease, or cross-yield handle lifetime was identified. The manager's copied project/request identity and source membership do not prove which bytes were compiled. No VFS behavior changed, and the test step performs no VFS operation.

The test step cannot enter artifact publication. Its test-only admission, empty artifact fields, explicit failure, and unchanged production callback prevent a fake successful Build or stale Run/Debug result. Existing synchronous artifact writing and NativeElf validation remain untouched. No new claims are made about source freshness or atomic publication for a future asynchronous path.

Owner identity and generation-checked handles remain in force. Owner close between steps is tested; shutdown is an admission barrier and work cannot advance after it; release is rejected while the test pump is active; cancellation requested while running is acknowledged at the next safe boundary. Existing owner-close-before-image/stack-clear ordering and shutdown Busy/defer behavior remain intact. No detached worker can reference an app after teardown.

The public `gx_build_request`, `gx_build_snapshot`, SDK ABI, host-call layout, and `build_project_start/poll/release` behavior are unchanged. The added wrapper is internal kernel C++; no asynchronous-build capability is advertised. Developer Studio production source and shared callbacks were not modified, so Studio CTest was not rerun; the historical COMP3R1 result of 34/34 remains historical, not a COMP3R4 run.

## 8. Validation evidence

| Validation | Current result |
|---|---|
| Hosted production manager test | PASS. The updated runner reports `Desktop build-job pump wiring PASS`; manager lifecycle tests pass with `Manager bytes=27352` and capacity 1. Run twice. Covers one-step boundaries, nested target rejection, no fake artifact, stable terminal state, stale generation/release, owner close, running and queued cancellation, shutdown, lifecycle and concurrent polling/admission. |
| Server compiler regression group | PASS, 9/9 compiler scripts: arrays, structs (including COMP3R1 recursion boundary), struct arrays, pointers, object, multifile, globals, functions, bootstrap. |
| Compiler driver/regression support | PASS: diagnostics/source mapping, incremental cache, linker, artifact stages. |
| NativeElf/runtime/ABI group | PASS, 4/4: validator, runtime, trampoline nonvolatile state, ABI layout. |
| Combined isolated Server regression runner | PASS, 14 suites (9 compiler + diagnostics + 4 NativeElf/ABI). Evidence is under `out/comp3r4-validation/isolated/`; historical output directories were not overwritten. |
| AMD64 freestanding modified objects | PASS, 3/3 modified objects (manager, build service, desktop) with production freestanding flags; relocatable `ld -r` combination passed. Evidence is under `kernel/out/comp3r4-validation/freestanding-objects/`. No unsupported STL, exceptions, RTTI, or new dynamic allocation was introduced. |
| Static stack-usage analysis | PASS as an analysis run; the resulting 128 KiB versus 242,512-byte known nested sum is an unresolved safety gate, not a pass for compiler execution. |
| Selected-TU BSS accounting | PASS as an accounting run; 6,148,800-byte subtotal, not full-kernel BSS. |
| `git diff --check` | PASS in both repositories before commit. |
| Developer Studio build / CTest | Not rerun: Studio production code and shared contracts are unchanged; report-only change. Historical 34/34 is not represented as a current result. |
| Full kernel build or boot | Not performed. Object compilation and relocatable combination do not qualify a kernel image or boot. |
| QEMU / guest compiler responsiveness / runtime stack watermark | Not performed; no exclusive QAC reservation. |

All generated validation evidence remains untracked and outside implementation commits. Server v0.3 and the existing detached Server worktree remain untouched.

The Server implementation was committed path-limited as `9594543f` on `v0.5_DEVELOPER_STUDIO`. An ordinary `git push origin v0.5_DEVELOPER_STUDIO` was attempted and failed with `git@github.com: Permission denied (publickey)`. No remote or authentication setting was changed. The Studio report commit and its ordinary push attempt are recorded in the task closeout.

## 9. Outcome and next boundary

**Outcome A — COMP3R4 continuation foundation accepted.** Evidence supports the normal desktop boundary calling the one-slot manager's test-only bounded step, with the NativeElf nested-target guard preserved, lifecycle and generation behavior tested, production compiler behavior unchanged, current static stack risk quantified, regressions passing, and affected AMD64 objects compiling under freestanding flags. The outcome accepts only that foundation. It does not prove full asynchronous compilation, guest UI responsiveness, compiler stack safety, or a qualified 384 KiB stack.

Recommended COMP3R5 scope: define owned bounded source/dependency snapshots and their source identity; refactor compiler stages into an explicit persistent continuation state without retained stack pointers; establish one-job memory admission; and deliver an owned guarded stack with fail-closed overflow handling and runtime high-water qualification before moving any compiler execution. Keep VFS snapshot consistency, generation-specific artifact staging/freshness, real progress, and safe cancellation as explicit gates. If those stack/ownership requirements cannot be implemented with current kernel facilities, reassess the execution architecture rather than forcing compiler work through this desktop hook.

## 10. COMP3R4 closeout index

1. **Outcome:** A, limited to the continuation foundation.
2. **Stale prompt:** clear; no earlier COMP3R4 report/marker; current foundations confirmed.
3. **Starting/ending HEADs:** start hashes in §1; ending Server and Studio hashes in task closeout.
4. **Branches/worktrees:** existing `main` and `v0.5_DEVELOPER_STUDIO`; detached worktree untouched; no branch/worktree creation.
5. **COMP3R3:** accepted one-slot ownership/lifecycle foundation remains in force.
6. **Scheduler entry:** AMD64 `kernel_main → desktop::tick()`; §2 has lines and interrupt state.
7. **Reentrancy:** NativeElf `in_target` and `pumpInProgress` guards preserved; nested desktop call rejects the build step.
8. **Pump implementation:** internal build-service wrapper calls one manager test-step boundary.
9. **Test behavior:** one fixed step per allowed invocation; deliberate failed terminal, no artifact.
10. **Real work feasibility:** admission copy is bounded but callback-synchronous; VFS/compiler stages need snapshot and resumable state.
11. **Genuine stage integrated:** none.
12. **Synchronous path:** remains available and unchanged.
13. **Service stack:** 128 KiB / 131,072 bytes.
14. **Static compiler estimate:** 242,512-byte known nested sum; detailed in §5.
15. **Guard feasibility:** no owner-safe guard/high-water facility exists; unsafe in-place instrumentation was not added.
16. **Runtime high-water:** not measured.
17. **Dedicated stack policy:** 384 KiB remains provisional; see §5.
18. **Memory budget:** manager size, current selected-TU BSS, and prospective estimate in §6.
19. **Ownership/generation:** preserved and tested.
20. **Request copy:** fixed-size owned request/membership; no source bytes retained.
21. **Progress:** only actual test step lines and coherent locked snapshots.
22. **Owner close:** between-step clearing tested; existing teardown order preserved.
23. **Cancellation:** queued behavior retained; running test cancellation acknowledged at next boundary.
24. **Shutdown:** admission barrier and between-step stop tested.
25. **VFS:** no test-step access; global synchronous VFS lacks snapshot/lease contract.
26. **Artifact safety:** test path cannot publish; production artifact path unchanged.
27. **ABI:** public structs, callbacks, and layouts unchanged.
28. **Hosted manager:** PASS, current runner twice.
29. **Server compiler scripts:** 9/9 PASS.
30. **NativeElf/ABI:** 4/4 PASS.
31. **Studio CTest:** not rerun, no Studio production/shared API change; 34/34 is historical.
32. **Freestanding objects:** 3/3 modified objects and relocatable combination PASS.
33. **Full kernel build:** not performed.
34. **QAC/guest:** QAC3C2 accepted, QAC3 open/next C3; no reservation/QEMU/guest proof.
35. **Files modified:** Server manager `.h/.cpp`, service `.h/.cpp`, `desktop.cpp`, manager host test, and its PowerShell runner; this Studio report.
36. **New tests:** desktop wiring assertion and hosted nested-target/cancel-between-steps/terminal-artifact/owner-close/shutdown/stale-generation assertions.
37. **Documentation:** this file.
38. **Local commits:** Server implementation commit and Studio report commit are listed in task closeout.
39. **Pushes:** ordinary existing-remote attempts and results are listed in task closeout; no remote/auth changes.
40. **Ahead/behind:** final values are listed in task closeout.
41. **Blockers:** guarded compiler stack and runtime high-water; resumable compiler state; immutable bounded source/dependency identity; guest qualification.
42. **COMP3R5:** owned source snapshot, persistent compiler state, guarded measured stack, one-job memory cap, truthful progress/artifact freshness.
43. **Final worktrees:** scoped commits only; generated evidence remains untracked; exact status in task closeout.
44. **Next phases:** COMP3R5, COMP4, and Phase 30C were not started.

**COMP3R5, COMP4, Phase 30C, and Server release integration were not started.**
