# Developer Studio COMP3R2 — Kernel-Owned Asynchronous Build Architecture

**Disposition: Outcome A — Architecture Accepted**

**Feasibility: Decision B — Small Kernel Foundation Required**

**Scope: Architecture investigation only. No production implementation or guest qualification was performed.**

## Executive decision

The current guest build is synchronous and blocks the NativeElf application call path. The Server has no production-qualified general kernel worker, work queue, or scheduler task lifecycle. Its AMD64 context-switch code and NativeElf cooperative pump provide useful low-level precedents, but they do not constitute a general worker facility.

The smallest viable direction is one **specialized, single-job cooperative compiler continuation** owned by the kernel. It would retain compiler call frames on a dedicated kernel stack and be advanced at safe points from the existing desktop update boundary. The NativeElf application yields to that boundary while waiting for events; the desktop update already pumps its NativeElf target there. A separate build-service pump at that same boundary can advance one bounded compiler slice. Studio polls immutable snapshots through the existing start/poll/release callbacks.

This avoids a general scheduler and parallel compiler calls. It is not ready to implement as-is: compile-only stack accounting found a cache-hit call path whose measured static frames exceed the current 128 KiB build-service stack. A guarded, high-water-measured worker stack and a bounded input snapshot are prerequisites. A **384 KiB stack is a provisional candidate**, not a qualified limit. Final artifact publication also needs generation-specific staging and a source fingerprint that Studio can compare before Run or Debug.

Outcome A accepts this bounded architecture and implementation sequence only. It does not accept asynchronous behavior, stack sufficiency at runtime, or any guest behavior.

## 1. Stale-prompt gate and repository state

The COMP3R2 report did not exist in the current Developer Studio checkout. No COMP3R2 or later report or authoritative phase marker superseded this request. Relevant tracked files had no concurrent modifications.

| Repository | Starting branch and HEAD | Starting worktree |
|---|---|---|
| Developer Studio | main — f68684302bac12d5ea871eee76069be78fc22951 | Tracked worktree clean; only the main worktree |
| Server v0.5 Developer Studio | v0.5_DEVELOPER_STUDIO — 0e8e7e343129d44d5c1bddd422dbb5a290879b0d | Tracked worktree clean; pre-existing untracked validation outputs preserved |
| Server v0.5 detached worktree | detached — bf78d517b513b624f61bee4ae6d144377f6c8000 | Existing phase29w worktree preserved and untouched |

The two primary HEADs match the historical COMP3R1 references in the task. The Server’s existing untracked out/comp2-validation/, out/comp3-validation/, and out/comp3r1-validation/ directories were preserved. The isolated stack-accounting files created for this investigation are in out/comp3r2-validation/stack-usage/; they are untracked evidence, not production files.

Server v0.3 was not inspected or modified. No branch, worktree, phase marker, release artifact, or QEMU state was changed.

## 2. Prior accepted and pending evidence

- COMP3 is Outcome B, COMP3_GUEST_RESPONSIVENESS_DEPENDENCY. The hosted API uses a background thread, but guest BareMetalBuildService::start remains synchronous. No live guest progress is visible during compilation.
- COMP3R1 is Outcome A for compiler stabilization. Its report records Server compiler scripts 9/9 PASS, Developer Studio CTest 34/34 PASS, and the compiler recursion limit of 679 with the next activation rejected. Those results do not measure the kernel build-service stack.
- COMP1 and COMP2 remain Outcome B with guest acceptance 0/3 each.
- Phase 30B remains Outcome B with guest acceptance 0/5; Phase 30C has not started.
- Current QAC documentation records QAC3C2 accepted on 2026-10-22, overall QAC3 open, and QAC3C3 next. This is not a reservation for this task. No current QEMU census or reservation was taken.

Evidence references: Developer Studio reports docs/DEVELOPER_STUDIO_COMP3_BUILD_FEEDBACK_AND_RESPONSIVENESS.md, docs/DEVELOPER_STUDIO_COMP3R1_COMPILER_REGRESSION.md, docs/DEVELOPER_STUDIO_COMP1_FRESH_PROJECT_LIFECYCLE.md, docs/DEVELOPER_STUDIO_COMP2_MULTIFILE_PROJECT_WORKFLOW.md, and docs/DEVELOPER_STUDIO_PHASE30B_LONG_ROOT_DWARF_ARTIFACT_PATH.md; QAC docs/phase-validation.md.

## 3. Current guest build path and blocking behavior

Verified path:

1. Studio’s BuildControllerStart validates the request, handles the dirty-document Save All decision, then calls the selected service’s start.
2. bareMetalBuildStart in src/main.cpp builds a local gx_build_request. Its string pointers refer to the controller’s request data.
3. Server host_bare_build_start validates the current NativeElf app context and pointer ranges, copies the eight strings into loader-global s_bareBuildStrings, constructs a local request pointing to those globals, and calls BareMetalBuildService::start. It currently reads the request fields and strings before the service checks the request size/version; asynchronous admission must validate the header before dereferencing those fields.
4. start reserves the single global BuildJob, creates its handle, clears its snapshot, and calls run_build before returning.
5. run_build switches RSP to the static 128 KiB s_buildServiceStack and invokes run_build_core. This is a synchronous stack switch, not a worker or task.
6. run_build_core validates project metadata and source selection, reads through VFS, calls compile_project_incremental, reports compiler/linker results, reopens and validates the NativeElf artifact, hashes it, and writes the terminal snapshot.
7. Control returns through the callback and BuildControllerStart; only then can Studio’s event loop poll or render the result.
8. BareMetalBuildService::poll only copies the snapshot. release clears the singleton.

Studio’s gx_main executes on the current NativeElf call path. Its host callback is a synchronous call from that app execution; it is not dispatched to a process task or kernel worker. NativeElf invocation changes the stack but does not enter a lower privilege ring. During the service call, Studio’s application stack and event loop are suspended while the compiler runs on the static build-service stack. The ordinary server desktop context is not servicing Studio events during that interval.

pollBuild runs at the top of the Studio loop and after event handling, but it cannot run while start has not returned. Studio passes an event-poll timeout as long as 500 ms in its normal loop (50 ms in a diagnostic path). The bare-metal host callback currently ignores that argument, polls input, and yields to the desktop scheduler; therefore guest input cadence is governed by the desktop pump and compiler slice, not a 500 ms sleep. Hosted hosts may honor the requested wait, so the Studio loop should cap it while BuildController is active. Studio currently prevents a second Build and blocks normal app close/workspace switching during a build; there is no project-wide source freeze contract.

Source anchors: Studio src/developer_studio_build.cpp (BuildControllerStart, BuildControllerPoll) and src/main.cpp (bareMetalBuildStart, pollBuild, gx_main, close/workspace handlers); Server kernel/core/native_elf/native_elf_loader.cpp (host_bare_build_start, host_bare_build_poll, host_bare_build_release, teardown_application) and kernel/core/compiler/compiler_build_service.cpp (start, run_build, run_build_core, poll, release).

## 4. Ownership at each current boundary

| Stage | Execution and stack | Memory / lifetime | VFS and progress |
|---|---|---|---|
| Studio admission | NativeElf app call path; Studio event loop blocked inside start | Request belongs to Studio; adapter request is local to callback | Save All precedes start; no live guest progress |
| Host callback | Same synchronous call path | Request struct and caller strings may be in app image/stack; loader copies strings to reusable globals | Pointer range checks are bounds checks, not privilege isolation |
| Build service | Same CPU flow, switched onto static 128 KiB BSS stack | Singleton BuildJob, global project/manifest/artifact buffers, compiler BSS buffers; no request ownership transfer | Service invokes VFS synchronously; no worker snapshot |
| Compiler | Nested call on service stack | Static global source, ELF, object, include-expansion, module, token/parser, and linker state | VFS calls open/read/close or write; no cross-call VFS lease |
| Completion | Returns to Studio callback before polling | Snapshot and artifact remain in singleton/VFS | Studio then polls once or more and releases |

No current build-specific lock is held across a real worker boundary because no worker exists. Compiler state is singleton and non-reentrant. A concurrent compiler, smoke operation, or second build would race shared scratch. Initial asynchronous policy must serialize all uses of this compiler lane, not merely reject a second Studio request.

## 5. Scheduler and worker facilities

The Server process table is a stub: kernel/core/process.cpp::schedule halts rather than switching tasks. AMD64 sys_yield is a TODO. kernel/arch/amd64/context_switch.cpp has context APIs and switch code, with outstanding FPU, TSS.RSP0, preemption, interrupt, and thread lifecycle work. These are experimental low-level pieces, not a production task API.

NativeElfRunService has one specialized cooperative target context and a 32 KiB scheduler stack. native_elf_scheduler_pump captures the current owner context afresh on each pump and switches into the NativeElf target. kernel/core/desktop.cpp calls that pump at the normal desktop update boundary. This proves a useful same-CPU cooperative pattern, but the service is tied to NativeElf run state, handles one target, and has no generic job ownership, priority, queue, cancellation, or join contract. It should not own compiler jobs directly.

The Studio host scheduler in src/scheduler.* uses hosted C++ threads, mutexes, condition variables, and an unbounded host-side queue. It is not available in the freestanding Server. Hosted NativeBuildService validates its bounded request, admits one active job, starts a std::thread that launches hidden PowerShell, collects stdout/stderr with readers, polls at 50 ms, and uses a 300,000 ms timeout with a Windows Job Object or termination fallback. Its admission, polling, output, terminal-result, and release semantics are reusable. Its Windows threads, process tree, and forced timeout cleanup are not kernel primitives.

## 6. Compiler state, stack, and bounded memory

The compiler is single-instance and non-reentrant. compiler_driver.cpp owns static source (65,537 bytes), ELF (98,304 bytes), object (131,072 bytes), eight declaration scratch buffers of 16,385 bytes each, include-expansion, module, linker, and verification buffers. compiler_module.cpp owns shared token, IR, and parser scratch, including a 2,048-token array. The build service also owns 16 KiB project and manifest buffers and a 98,304-byte artifact buffer. These are existing singleton BSS, not per-worker memory. CompiledModule/module and linked-program arrays add more BSS and are not included in the proposed incremental job cap. The stack-usage report also assigns run_build_core a large automatic frame. This supports one serialized worker only.

### Isolated compile-only stack accounting

The authorized read-only experiment compiled 11 existing AMD64 freestanding compiler/service translation units with C:\mingw64\bin\g++.exe targeting x86_64-w64-mingw32. It used the kernel AMD64 flags, including -m64, -mcmodel=large, -mno-red-zone, -mno-mmx, -mno-sse, -mno-sse2, -ffreestanding, -O2, -fno-exceptions, and -fno-rtti, adding only -fstack-usage. Objects and reports are preserved under out/comp3r2-validation/stack-usage/. No production source was changed and no test, full kernel build, or guest run was performed.

| Function | Reported static frame |
|---|---:|
| load_cached_object | 116,464 bytes |
| compile_project_impl | 111,152 bytes |
| run_build_core | 14,912 bytes |
| link_modules | 16,928 bytes |
| run_bootstrap_smoke | 323,344 bytes |

compile_project_impl directly calls load_cached_object on the cache lookup path. Adding those two frames and run_build_core gives about **242,528 bytes**, before deeper callees and trampoline frames. This exceeds the configured 131,072-byte service stack by 111,456 bytes (about 109 KiB). The 323,344-byte smoke function is not part of the production build-service path and is excluded from the sum. The generated report is a static per-function estimate for the configured MinGW target/flags, not a runtime stack watermark or guest proof; actual peak and overflow behavior remain unqualified.

Other measured production frames include 1,344 bytes for the recursively called include expander (bounded to depth 8), 1,456 bytes for the largest parser entry frame (with expression and block nesting bounded at 16 and call nesting at 8), 5,392 bytes in the AMD64 backend path that emits source maps/debug-variable records, and 1,328 bytes for object deserialization. Linking has a 16,928-byte frame. The COMP3R1 recursion boundary of 679 is for generated NativeElf execution and is not a compiler/parser stack allowance.

The NativeElf 512 KiB app stack and COMP3R1 host-test 8 MiB stack reserve are unrelated budgets. Do not copy either as the worker policy. A 384 KiB dedicated build stack is a **provisional candidate**: it leaves roughly 147 KiB beyond the known nested-frame sum, but is not accepted until the final compiler flags, all nested paths, and a runtime high-water/guard test confirm sufficient headroom. Prefer reducing the large automatic CompiledModule temporaries in load_cached_object and remeasuring if that lowers the safe budget.

Proposed one-job added memory cap:

- Source and declaration snapshot bytes: at most 2 MiB (16 translation units × 64 KiB source, plus at most 64 KiB expanded declarations per TU).
- Path/dependency records: at most 528 bounded paths (16 source paths plus 16 × 32 declaration dependencies), with byte counts and hashes; target cap 160 KiB.
- Dedicated worker stack: provisional 384 KiB, one instance, reserved as page-aligned kernel-owned BSS or a guarded page-backed allocation if the mapping layer supports it. Include low-bound guard and high-water instrumentation. A canary alone is not memory protection; implementation must prove a real guard mechanism or reduce frames to fit an enforceable stack bound.
- Job, request strings, and progress/result snapshot: bounded by one existing-size build snapshot plus fixed request/path metadata; no per-job heap growth.

This is roughly 2.55 MiB new bounded job memory at the provisional limits, excluding existing compiler BSS, kernel context storage, and the artifact/cache buffers already present. Record and approve exact BSS and stack totals before implementation; there are no multiple workers.

## 7. Architecture options

| Option | Feasibility | Required work and risks | ABI and verification |
|---|---|---|---|
| A. General dedicated kernel worker | Not available now | Requires task creation/termination/join, task identity, stack contract, scheduler integration, FPU/interrupt and VFS rules; scope exceeds the smallest facility | Existing start/poll/release shape could remain, but a task API would be new; requires kernel scheduling, shutdown, cancellation, and guest qualification |
| B. Specialized cooperative compiler continuation | Recommended after foundation | One persistent context and dedicated stack; yield only at bounded compiler safe points; one exclusive compiler lane; deep-copy request and source inputs; stage output. A long uninterruptible compiler loop still blocks a desktop tick until it gains safe points | Existing start/poll/release can report the lifecycle. Needs context/stack tests, bounded-slice tests, source-stale tests, and real guest UI qualification |
| C. Reuse existing service queue | Not suitable | NativeElf run pump is target-specific; process scheduler and syscall yield are stubs; hosted queue relies on host threads and unbounded C++ storage | No compatible qualified queue contract exists; adapting NativeElf run state would entangle application lifetime with compiler lifetime |

### Recommended execution model

Implement one build-specific pump at the normal kernel desktop update boundary, adjacent to the existing NativeElf run pump. The build pump advances a retained compiler continuation by one bounded quantum on its dedicated stack. The NativeElf target yields to the desktop boundary while gx_main waits for input; the desktop then advances the build continuation and resumes the app on a later pump. This keeps compiler execution off the Studio callback stack without introducing preemptive kernel threads.

The build service’s public poll remains a snapshot read; it does not execute compiler code. Studio receives actual progress through its existing polling loop. Cap the NativeElf event wait while a build is active (proposed target: 16 ms), and ensure each compiler quantum stays below a measured desktop-latency budget. Break up lexer/parser/codegen loops where one bounded compiler unit is too long. Never yield while a VFS operation, cache publication, ELF publication, or snapshot mutation is partially complete.

This recommendation is scoped to the same-CPU cooperative path represented by the desktop/NativeElf code. SMP execution and cross-CPU compiler/VFS synchronization are not established; if a build pump can run concurrently on another CPU, explicit serialization is required before this design is used. It does not make the NativeElf context API a general scheduler.

## 8. Proposed job and request lifecycle

Use one fixed kernel-owned job slot: one active job, zero waiting jobs. A second request returns Busy deterministically. The compiler lane, its global scratch, and object cache are exclusively leased to this job.

State progression: **Created → Validated → Queued → Running → Completing → Completed / Failed / Cancelled**.

- Reserve a private slot first. Validate request size/version before reading any request fields or strings. Validate every pointer range and bounded NUL terminator, then copy all eight strings into slot-owned storage. Enforce canonical project-root and artifact-path boundaries.
- Copy the project metadata, source membership, and bounded source/declaration bytes into a kernel-owned immutable job snapshot before compilation. Do not retain app-image, app-stack, callback-local, loader-global, or VFS-handle pointers.
- Publish the handle only after the slot is fully initialized. start returns Queued; the desktop pump can start only a fully validated job.
- Keep compiler scratch globally shared but protect it with an exclusive build-lane lease. No unrelated compiler call may enter until terminal cleanup.
- Publish progress through a private snapshot. Under the proposed cooperative same-CPU pump, the worker completely updates the snapshot before yielding, and Studio can poll only after the worker has relinquished control; publish that completed version as one unit. No current build snapshot lock has been established. If the pump can be preempted or run on another CPU, add a short interrupt-safe publication lock or equivalent before enabling that execution mode. Never hold it during compilation or VFS operations, and never expose a partially written diagnostic.
- Once terminal, freeze the result and keep it until the owner releases it. Releasing a running handle returns Busy and cannot free the stack. Capacity is reclaimed only after terminal cleanup and owner release, or after an owner-gone result is safely discarded.
- Admission failure clears the reserved slot and creates no continuation or handle.

## 9. Handle identity and ownership

Keep the public handle an opaque 64-bit token, not a pointer. Encode a fixed slot index and monotonically increasing generation; use an internal monotonic NativeElf runtime-generation cookie as owner identity. The cookie is not an app-context address because the loader reuses a global context object.

On every poll, release, teardown, or future cancel, require all of: slot is live, token slot/generation match, and runtime owner cookie matches. Reject stale, cross-runtime, and double-release tokens. Do not reuse a generation after wrap; retire the slot or fail admission. Since there is one slot, exhaustion has a single deterministic answer: Busy until the live job reaches cleanup and its result is released or discarded.

The kernel owns request bytes, project identity, source/dependency records, scratch-lane lease, worker context/stack, cancellation/owner-gone flags, progress buffer, diagnostics, and artifact generation/path/hash. The application owns only its copied UI result and token.

## 10. VFS authority and source consistency

kernel/core/vfs.cpp uses global mount, file, and directory tables without an observed lock or generation/lease API. read_file opens, reads, and closes within the call. unmount checks currently open file handles, but no persistent authority protects the space between compiler reads. Do not retain VFS handles over a cooperative yield, and do not claim VFS thread safety.

At admission, bind the request to the authenticated Studio runtime generation and canonical project root. Reject traversal and paths outside that root. Snapshot the selected source files and their bounded declaration dependencies into the job’s fixed 2 MiB payload; compiler work must consume these bytes rather than reread mutable source paths. Snapshot VFS calls are sequential; the VFS has no transaction. Capture metadata and hashes before and after the copy and fail if identity or content changes during capture. Revalidate mount availability at each complete file operation. Treat VFS loss as a terminal build failure with cleanup, not as permission to publish.

The project’s 16-source, 64 KiB/source, include depth 8, 32-dependency, and 64 KiB-per-TU declaration bounds support this cap. The project/manifest text limits are 16 KiB each; source path bounds are 256 bytes. Existing object-cache identity uses FNV-1a and is not a cryptographic freshness identity. Compute a separate SHA-256 digest over canonical paths, project/configuration identity, exact snapshot bytes, and source membership for BuildResult freshness.

Block Studio edits, saves, project membership changes, workspace closure, and another Build while the job is active, or mark the job stale when its project revision changes. Because other VFS writers may exist, compare the kernel snapshot digest with the current project digest before accepting Run/Debug. A source change after the snapshot may let compilation finish, but the output must be labeled for that snapshot and refused as current.

## 11. Progress, result, ABI, and artifact publication

The current gx_build_snapshot already includes states, bounded output lines, elapsed time, artifact identity/hash, and module/cache counters. The existing start / poll / release callbacks can represent Queued, Running, validating, terminal completion, diagnostics, and release. Do not fabricate percentages. Count a source unit only after it completes; report cache hits from actual cache validation. Current Studio BuildResult gates already compare project ID/root, target/profile, artifact path/configuration, ordered source membership, artifact validity/entry point, and artifact SHA-256. They do not include source-content identity, so the new snapshot digest and matching Studio check are needed to preserve the strict Run/Debug gate for source edits during or after a build.

On the AMD64 ABI, gx_build_request is 72 bytes and current gx_build_snapshot is 8,800 bytes. Snapshot fields end with linkedModuleCount at offset 8,792; the structure rounds to 8,800-byte size. The bare-metal host-call callback slots are append-only; current build start/poll/release offsets are 240/248/256. No layout change is needed just to make polling asynchronous.

Strict source freshness does need the build snapshot to convey the exact source-snapshot fingerprint to Studio. Proposed extension: append a 32-byte digest at offset 8,800, for a new 8,832-byte snapshot, preserving all current fields. Negotiate by the existing size field: the kernel reads the caller capacity safely, writes only the supported prefix, and old-size clients can still poll but must not treat a result without the digest as Run/Debug eligible. The current loader requires a full snapshot pointer range; that check must become size-aware before this extension. Keep API version 1 if the existing size contract is used; otherwise define an explicit appended API version/capability. Do not change layouts in COMP3R2.

Current compiler final ELF writing goes directly to the expected output path. Build-service validation happens after that write. This is insufficient for a background job that can be cancelled or orphaned. Compile to a unique per-generation staging path; close, reopen, hash, and validate it; verify the source snapshot is still current; then rename to a unique generation-specific final path that did not previously exist. Publish the final path/hash and snapshot fingerprint only after rename succeeds. Never overwrite the previous current artifact. VFS FAT rename is non-replacing, but the current system provides no crash-atomic replace guarantee; do not claim one. If unique-path rename or cleanup fails, mark failure and never return a Run/Debug-eligible result.

Existing persistent object files are ELF relocatable objects with .o paths in this implementation (the request’s .gxo wording is not the on-disk suffix). Reuse only objects validated against the immutable snapshot’s source/dependency identity. A failed cache write is a cache failure, not a successful hit. Failed or cancelled work may leave a valid reusable object, but no unvalidated final executable may become current.

## 12. Cancellation, timeout, and lifecycle

There is no guest in-flight Cancel callback today, although the state enum has GX_BUILD_CANCELLED. First async release should keep Studio close blocked during an active build, matching existing close behavior. A future Cancel command needs an append-only host-call slot at the end of the table, gated by host->size; older apps continue without that command. Do not change existing callback offsets.

Queued cancellation is safe before the first compiler pump. Running cancellation sets a flag and is observed only at verified safe points after complete bounded work units. The worker drains temporary files, resets compiler scratch/lease, then publishes Cancelled. During Completing, reject cancellation or report “too late”; never claim cancellation until cleanup and worker quiescence finish. Never kill a context at an arbitrary instruction.

No build duration timeout exists. Add a heartbeat/progress sequence and elapsed time for observation. A watchdog may report a long or stale job but cannot free its memory or forcefully terminate it. With a cooperative pump, lack of a safe point is a liveness failure; preserve the slot, stack, and VFS-dependent state until the worker yields or the system follows a separate fail-closed recovery policy.

Normal Studio close is already blocked while BuildController is active. On NativeElf teardown or abnormal owner loss, invalidate the runtime generation, set ownerGone/cancel, and let the kernel pump drain at safe points. The job must not dereference app/runtime state. A reopened Studio gets a new owner generation and cannot poll or release the prior handle. Its Build is Busy until the orphan has cleaned up.

The current perform_shutdown path issues ACPI/QEMU power signals and does not quiesce build services. Future ordering must be: stop admission; mark cancellation; pump to a safe terminal boundary while VFS, scheduler, and stacks remain live; discard owner-gone result; reclaim context/stack; then tear down/unmount dependencies and request power-off. If safe completion cannot be reached, do not free worker memory or unmount its dependencies.

## 13. Security limits

The NativeElf trampoline changes stack but has no ring transition. NativeElf code executes at the caller’s privilege level; app-pointer range checks are useful bounds checks, not a sandbox against malicious CPL0 code. The job model protects against stale pointers, stale handles, accidental cross-runtime access, path traversal, and lifecycle races. It does not create privilege isolation. Validate and scope all project paths, but do not claim an untrusted-app security boundary.

## 14. Failure and recovery matrix

| Failure | Required owner/state | Safe behavior |
|---|---|---|
| Invalid size/version/string/path | Admission before handle publication | Reject; clear reserved slot; no worker |
| Second Build or slot occupied | Existing job owns compiler lane | Return Busy; do not queue |
| Stale/cross-owner/double handle | Poll/release/cancel boundary | Reject without exposing snapshot or changing job |
| VFS read failure/unmount | Running; no file handle retained across yield | Fail at next complete operation; discard current artifact stage |
| Source changes during snapshot | Preparing | Fail snapshot admission or retry only under a bounded policy |
| Source changes after snapshot | Running/Completing | Build is for captured digest; mark stale and refuse current Run/Debug |
| Cache object malformed or mismatched | Running | Treat as cache miss; compile from snapshot; never count as hit |
| Cache write failure | Running | Report/cache miss; preserve compiler correctness |
| Stack guard/high-water breach | Running | Fail closed; do not resume with an untrusted stack; retain diagnostic evidence |
| Cancel request | Queued/Running | Set flag; exit only at a safe point and after cleanup |
| Cancel during publication | Completing | Reject or report too late; no false Cancelled state |
| Owner close/crash | OwnerGone | No app pointers retained; cancel and pump cleanup; discard terminal result |
| Server shutdown | Quiescing | Stop admission and drain before VFS/service teardown; never free live job storage |
| Staging/rename/hash/ELF validation failure | Completing | Remove staging best-effort; terminal failure; no current artifact result |
| Pump does not yield | Running / watchdog-observed | Report stalled if possible; never reclaim or force-kill active context |

## 15. Verification plan

These are future gates; none were run in COMP3R2.

| Gate | Hosted/focused coverage | Required kernel or guest proof |
|---|---|---|
| Request copy, size/version, bounds, path allowlist | Unit and malformed-input tests | NativeElf callback pointer/range and deep-copy proof |
| Slot admission, generation, owner, stale/double release | Deterministic service harness | App restart, cross-owner rejection, capacity Busy |
| Stack budget and guard | Per-function stack report and static frame review | Canary/guard plus high-water under cache hit, cache miss, max TU/dependency, link, validation |
| Progress and snapshot consistency | State-machine and buffer publication tests | Poll while real build advances; UI remains interactive |
| VFS loss and source change | Fake mount/change injection | Unmount/remount and edits during a guest build; no current stale artifact |
| Cache and multi-file behavior | Existing compiler scripts, cold/warm cache projects | Same project in guest; accurate module/cache counters |
| Artifact staging/freshness | Rename-failure, malformed ELF, hash and cleanup injection | Run/Debug accepts only exact generation/hash/source digest |
| Cancellation and app lifecycle | Queued/running/completing state tests | Studio close, app teardown/crash, reopen; no callback into old runtime |
| Shutdown order | Service-order harness | Guest shutdown during active job with VFS still live until quiescent |
| Compiler regression | Existing compiler regression suite and 679 recursion boundary | Real guest build/compiler diagnostics; no mock worker substitutes |
| Responsiveness and QAC | Hosted API checks are supplemental only | QAC-exclusive QEMU acceptance: input/redraw during compile, progress, completion, failure, Run/Debug |

## 16. Proposed implementation phases

1. **COMP3R3 — Kernel Build-Job Foundation.** Implement the one-slot kernel-owned request/result record, early request validation and deep copy, owner runtime generation, slot/generation handle policy, exclusive compiler lane, terminal retention, and bounded admission. Audit/refactor large automatic compiler frames; establish a guarded worker-stack contract and runtime high-water instrumentation. Keep compilation synchronous until the stack gate passes.
2. **COMP3R4 — Cooperative Worker Execution.** Add a specialized build continuation and one bounded service pump at the desktop update boundary. Add safe yield points and measured quantum bounds. Keep one worker and no general-purpose scheduler.
3. **COMP3R5 — Progress, Snapshot Identity, and Publication.** Publish consistent real progress through existing poll fields; compile from immutable source/dependency snapshot; stage generation-specific artifacts; append a size-negotiated source digest and make Studio Run/Debug reject stale results.
4. **COMP3R6 — Cancellation and Recovery.** Add safe cancellation, owner-gone cleanup, close behavior, timeout observation, and shutdown quiescence. Add an append-only Cancel callback only when the UI exposes cancellation.
5. **COMP3R7 — Guest Qualification.** After implementation gates pass, coordinate exclusive QAC ownership and validate real guest responsiveness and Build/Run/Debug flows. No QEMU launch belongs to COMP3R2.

## 17. Unresolved risks and hard gates

- The 128 KiB configured build-service stack is below the measured nested static-frame sum. No guest stack overflow was observed because no guest was run; actual peak, ABI transfer, and guard behavior remain unqualified.
- A 384 KiB stack is provisional. Implementation must establish an enforceable guard and measured high-water across all supported builds before running asynchronously.
- VFS has no snapshot transaction, mount lease, versioned file identity, or observed synchronization. Bounded copies plus before/after identity checks and current-source verification are required; they cannot claim atomic VFS snapshots against all possible external writers.
- Existing compiler cache identity is FNV-1a; freshness requires a separate snapshot digest and cache comparison rules.
- Context switching is experimental and lacks general thread lifecycle/FPU/TSS/preemption support. The recommendation is scoped to the same-CPU cooperative path represented in the inspected code; it must be re-audited if kernel execution changes.
- Artifact rename and cache writes have no documented crash-atomic replacement guarantee.
- NativeElf code shares kernel privilege. Job ownership does not sandbox app code.
- A NativeElf fault that is kernel-fatal cannot be recovered by the job manager; the current runtime provides no general fault-containment boundary for CPL0 app code.
- Server shutdown currently does not drain a compiler service; explicit quiescence must precede dependency teardown.
- QAC3C2 acceptance is not a QEMU reservation. Future guest qualification depends on QAC’s current admission policy and exclusive ownership.

## 18. COMP3R2 experiment and final state

The only isolated experiment was compile-only GCC stack-usage accounting for 11 existing Server translation units. Outputs remain preserved and untracked in out/comp3r2-validation/stack-usage/. It did not compile or modify production sources in place, run tests, execute the kernel, or launch QEMU.

At completion, Developer Studio contains this report as its sole task-owned change. Server v0.5 tracked files remain at the starting HEAD; all pre-existing validation outputs and the new stack-usage evidence remain untracked and preserved. The detached phase29w worktree and QAC repository are untouched. No phase marker changed. No Server v0.3 work occurred.

**First recommended milestone: COMP3R3.** Resolve and instrument the compiler worker stack, then implement the bounded job ownership and handle foundation. Do not move compilation off the callback until the stack and source-ownership gates pass.

**COMP3R3, COMP3R4, COMP3R5, COMP3R6, COMP3R7, COMP4, and Phase 30C were not started.**
