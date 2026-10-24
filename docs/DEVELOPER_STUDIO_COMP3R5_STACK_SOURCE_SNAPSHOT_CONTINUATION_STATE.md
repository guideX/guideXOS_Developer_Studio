# Developer Studio COMP3R5 — Stack, Source Snapshot, and Continuation Safety

**Outcome B — Partial Foundation.** The current compiler’s generated static frames exceed its 128 KiB service stack on supported cache-hit and link paths. No compiler stack guard or high-water mechanism is qualified. The one-slot manager retains its test-only continuation state across desktop updates, but the current VFS cannot supply a coherent, bounded source read step and the real compiler remains synchronous. No unsafe compiler work was moved to the desktop pump.

## Decision

COMP3R4’s continuation boundary and lifecycle foundation remain intact. This investigation reproduced the compiler frame accounting with the current production flags and object code, audited the VFS and stack facilities, and reran the hosted compiler, manager, NativeElf/ABI, and hosted responsiveness checks.

The first safe production preparation step was not integrated. Source acquisition still depends on synchronous VFS reads that can wait for device I/O; the VFS has no source generation or snapshot transaction. The existing production compiler also has confirmed frame paths above its configured stack. Those gates prevent using the desktop pump for real source or compiler work.

## 1. Stale-prompt gate and repository state

COMP3R2, COMP3R3, and COMP3R4 reports were read in full. COMP3R1 compiler correctness evidence was reviewed. No COMP3R5 report, implementation, or authoritative phase marker exists in either primary repository. No architecture change since COMP3R4 was found in the primary Server tree.

| Repository | Starting branch / HEAD | Starting worktree |
|---|---|---|
| Developer Studio | main / 63168f170eb6181b1a78fb87ce1f7a57d9bbcb71 | Tracked tree clean; only the primary worktree. |
| Server v0.5 Developer Studio | v0.5_DEVELOPER_STUDIO / 9594543f37959ee10aa207cb8c3fd6c9f482964e | Tracked tree clean. Pre-existing untracked validation outputs were present. |
| Existing detached Server worktree | detached / bf78d517b513b624f61bee4ae6d144377f6c8000 | Contains pre-existing tracked deletions; left untouched. |

For both primary repositories, Git reported 0 behind and 1 ahead using upstream...HEAD ordering. The branch names and commit references match the requested historical references. No branch or worktree was created or switched. Server v0.3 was not opened or changed. Existing validation directories were preserved.

There were no tracked concurrent edits in the primary Server files for compiler, build service, desktop, VFS, NativeElf, or memory code. Existing untracked validation directories remain. The separate QAC repository had a concurrent modification to docs/phase-validation.md; it was read only and left untouched.

The latest QAC marker still records QAC3 open and QAC3C3 next. Its 2026-10-24 QAC3C3R1 note records no active or queued reservations and no Recovery Required session at its reconciliation point. No exclusive COMP3R5 guest window was obtained or needed; no QEMU process was launched.

## 2. COMP3R4 architecture verified against current source

The AMD64 kernel loop calls desktop::tick() with interrupts enabled. Its build pump calls the one-slot manager’s test-only step and preserves the NativeElf active-target and pump reentrancy checks. A permitted test job advances one fixed arithmetic unit per call and deliberately terminates as FAILED. It does not call VFS, compiler code, or artifact publication.

The normal Build callback still enters BareMetalBuildService::start_owned synchronously. That path switches to the static 128 KiB service stack, invokes run_build_core, reads project metadata and sources through VFS, compiles, links, writes the expected ELF path, validates and hashes the artifact, and only then returns to Studio. Studio cannot poll while that callback is running. Public Build callbacks and structures remain unchanged.

The manager remains one fixed slot and measured 27,352 bytes. It owns bounded request strings and source-membership path records, validates owner/runtime identity and handle generations, and retains coherent result snapshots. It does not own source bytes or header bytes.

## 3. Gate A — current compiler stack audit

### Method and generated frames

The audit compiled 20 current compiler, AMD64 backend, NativeElf validation, VFS, FAT, block-device, and storage-driver translation units with MinGW GCC 15.2.0 targeting x86_64-w64-mingw32. It used the AMD64 production Makefile flags and added only -fstack-usage. The generated evidence contains 392 function records: all report static frames; none report dynamic stack allocation. Source hashes, flags, object files, .su files, and logs are retained under Server kernel/out/comp3r5-validation/stack-usage/.

| Function or path element | Generated static frame |
|---|---:|
| run_build_core | 14,896 bytes |
| compile_project_impl | 111,152 bytes |
| load_cached_object | 116,464 bytes |
| link_modules | 16,928 bytes |
| expand_include_file | 1,344 bytes per recursive activation |
| parse_translation_unit | 1,456 bytes |
| AMD64 emit_translation_unit_impl | 5,392 bytes |
| run_bootstrap_smoke | 323,344 bytes; not on the Developer Studio build-service path |

The current object disassembly confirms compile_project_impl makes a real call to load_cached_object. The source also calls link_modules later in compile_project_impl. These are distinct live paths; unrelated stages were not added together.

### Reachable over-capacity paths

On a supported object-cache hit, the live service/compiler frames are run_build_core → compile_project_impl → load_cached_object. Their generated frame subtotal is 242,512 bytes, which is 111,440 bytes above the configured 131,072-byte service stack.

On the later link stage, run_build_core → compile_project_impl → link_modules totals 142,976 bytes, or 11,904 bytes above the configured stack.

The wrapper and trampoline overhead, VFS/FAT/block-device callees, interrupt frames, and other nested helpers are not included in those subtotals. The object audit reports run_build_core, compile_project_impl, and load_cached_object as separately reserved frames; the generated call site confirms simultaneous reachability. The link path is separately confirmed by the production source call.

The static frame results show that the existing stack budget cannot be defended for all supported builds. They do not measure a runtime maximum or establish that a particular guest run has overflowed. The 323,344-byte run_bootstrap_smoke frame is unrelated and was excluded. The 16-byte stack alignment, no-red-zone flags, and Microsoft x64 trampoline home-space/alignment behavior are preserved. The trampoline aligns RSP and reserves 32 bytes before calling the service entry, but does not enforce a low stack bound.

Parser/source limits remain 16 translation units, 64 KiB per source, 16 KiB per declaration file, 64 KiB expanded declarations per translation unit, at most 32 dependencies per translation unit, include depth 8, and path capacity 256 bytes. Parser expression, block, and conditional nesting are bounded at 16; loop nesting and call-expression nesting are bounded at 8. The generated-code recursion policy of 679 is a separate NativeElf runtime limit, not a parser-stack allowance.

The include expander is recursive and bounded by the include-depth check. If eight activations are live, its static frames alone account for 10,752 bytes. This is another pressure source, but it is not needed to establish the over-capacity cache and linker paths.

### Current synchronous production risk

The service stack is an aligned 131,072-byte static BSS array. The build service switches to its top through invoke_native_entry_on_stack and executes the compiler on that stack. Interrupts are not masked by this path. The stack report does not include interrupt/exception frames, and no separate interrupt-stack reserve for this compiler call was established.

AMD64 production flags disable stack protector, stack checking, and stack argument probing. The service stack has no runtime low-bound check, guard page, canary, or high-water watermark. Because the cache-hit and link frame subtotals exceed the allocation, the existing synchronous compiler path has a real static stack-safety risk. A runtime overflow was not deliberately invoked.

Disabling object-cache reuse would not resolve the confirmed link-stage overage and would remove an existing supported optimization without making the remaining stack safe. No narrow admission check can safely admit a subset of compiler requests from the current evidence. The production risk therefore remains explicit and unresolved.

## 4. Stack ownership, guard, and high-water findings

Existing facilities do not provide a qualified compiler-worker stack:

- The kernel boot stack is a 16 KiB static assembly stack.
- The compiler service uses its current 128 KiB static stack.
- NativeElf has a separate scheduler stack and fixed application/service stack regions, but those belong to NativeElf execution and do not transfer to the build manager.
- The process scheduler is not a production task facility; process scheduling is a stub and sys_yield remains TODO.
- The kernel has a fixed free-list heap, but no stack allocator contract for page alignment, guard placement, ownership, or quiescent reclamation.
- NativeElf’s local page-permission helper toggles writable and NX bits on existing present pages. It is not an allocation or guard API and is not connected to the compiler service stack.
- NativeElf’s dedicatedStackUsed result checks that the recorded application RSP lies within its stack and that the kernel RSP was restored. It is not a high-water measurement.

No stack watermark, compiler stack canary, or runtime guard is implemented. A canary can report damage after adjacent memory may already have been overwritten; it is not hardware protection. Filling an inactive stack with a known pattern and scanning after quiescence is a viable future high-water method, but it cannot prevent overflow. An inaccessible or write-protected guard page needs an owned page-aligned region and a defined fail-closed page-fault path. Current facilities do not establish those properties for the compiler.

The smallest prerequisite is a kernel-owned stack-region contract with an explicit owner/generation, base, top, usable size, alignment, guard strategy, IRQ headroom, active/quiescent state, shutdown retention, and deterministic failure handling. The region must remain owned until the compiler has returned or the continuation has reached a verified safe boundary. A future high-water report should include stack identity, measured bytes, remaining headroom, guard state, and validity; it must scan only after the owner is quiescent.

### Qualification sequence

A future stack qualification must exercise minimal and multi-file projects, deepest supported expression/block syntax, maximum include depth and dependencies, supported recursive functions, cold and warm object-cache paths, full relink, syntax failure, linker failure, artifact-validation failure, and repeated build/release cycles.

For every scenario, record the compiler/toolchain flags, owner and stack bounds, high-water bytes, remaining headroom, guard integrity, nested interrupt allowance, cleanup result, and memory peak. A proposed acceptance gate is at least 25% measured reserve within usable stack after accounting for the measured interrupt allowance, with the guard excluded from usable bytes and intact on every run. The reserve is a future qualification criterion, not current evidence. Do not treat 384 KiB as qualified; even its 150,704 bytes above the known 242,512-byte cache subtotal do not account for omitted frames and interrupt overhead.

## 5. One-job memory budget

The current selected compiler/build-lane BSS subtotal is 6,148,816 bytes across the audited compiler/build objects. This includes the manager, source and compiler workspaces, and static buffers represented by those objects; it is not total kernel BSS. The current 27,352-byte manager is already part of this subtotal. The COMP3R4 selected-object subtotal was 6,148,800 bytes; the current accounting includes an additional 16-byte diagnostics BSS object.

No source snapshot, path table, extra stack, or continuation buffer was allocated in COMP3R5. Added production residency is zero.

A planning-only one-job extension based on existing limits would reserve:

- 1 MiB for 16 source files at 64 KiB each.
- Up to 1 MiB for expanded declaration/header bytes at 64 KiB per translation unit.
- 160 KiB for up to 512 dependency paths plus bounded source/configuration records and alignment.
- A provisional 384 KiB worker stack replacing the existing 128 KiB stack, a net increase of 256 KiB.

The net planning increase is 2,523,136 bytes. The selected subtotal would become 8,671,952 bytes before unrelated kernel memory. These are capacity estimates only, not allocated memory or a complete kernel memory budget.

## 6. Gate B — source and dependency ownership audit

### Current acquisition path

The build service reads bounded project and manifest metadata into static buffers, selects up to 16 sources, creates bounded object paths, and copies the request and ordered source membership into the manager. The compiler then reads each source with VFS stat/read_file into singleton compiler storage. Existing include expansion resolves supported relative includes through the compiler’s current parser, reads headers through VFS, records dependency path/length/hash metadata, and preserves source-map locations. Object-cache validation rechecks source and dependency identity before reuse.

The one-slot job owns eight copied request strings and source full/relative/object paths. It retains no caller string pointer after admission, but its source membership record is not a content snapshot. The compiler’s source/header scratch is shared workspace that can be overwritten on the next source; it is not a job-owned immutable source set.

### VFS consistency limits

VFS uses global mutable mount, file-handle, and directory tables without an observed lock, file generation, mount generation, mount lease, or snapshot transaction. read_file opens, reads, and closes synchronously. Unmount refuses while a handle is open, but read_file closes before returning, so that check does not pin a mount across separate source/header reads.

FAT reads iterate sectors and call the block-device function pointer. The virtio block wait loop has no timeout. NVMe has its own command wait limit, but the VFS interface does not expose a common bounded I/O completion contract. A byte limit therefore does not prove a source read has a predictable desktop-tick duration. No cross-tick source read was connected to the desktop pump.

A coherent source snapshot could not be established from the present API. Sequential source and header reads can observe different filesystem states; file timestamps are not content identities, and no VFS generation can detect a mutation or mount replacement. A partial inventory would only be progress metadata, not a consistent final snapshot.

### Identity and mutation behavior

The request snapshot, source/dependency inventory, owned content, verified content identity, and artifact freshness identity remain distinct:

| Layer | Current behavior |
|---|---|
| Project request | Kernel manager copies request strings and metadata. |
| Source inventory | Ordered full, project-relative, and object paths are copied, up to 16 sources. |
| Owned source/header content | No per-job content copy exists; compiler uses global bounded scratch while running synchronously. |
| Verified source identity | Object cache checks source size/hash and recorded header dependency size/hash. The hashes are cache checks, not a project-wide immutable snapshot identity. |
| Artifact freshness | The build service validates the ELF and computes its SHA-256. Studio’s existing gate compares project/configuration/output identity and ordered source membership, but no source-content digest is returned. |

The compiler’s existing include/dependency parser and cache checks remain authoritative; no second include parser was added. No source bytes are silently truncated by a new snapshot path because no snapshot path was added.

Studio’s existing Save All gate runs before Build. The synchronous callback blocks Studio’s event loop until the build returns, and a second Build is rejected while active. Those behaviors do not create a kernel VFS snapshot. The manager does not detect source/header/project-list mutation or mount-generation changes between acquisitions. A read failure is reported by the synchronous compiler path; a consistent multi-file result under mutation is not guaranteed by a documented VFS contract. App close and owner-generation behavior remain as established by COMP3R3/COMP3R4.

## 7. Gate C — persistent state and bounded production work

The static manager owns a one-slot Job record and snapshot that remain valid across separate pump calls. Handle generations and owner identity prevent stale/cross-runtime access. The COMP3R4 test continuation retains only fixed state and advances one arithmetic step per permitted pump; its hosted tests verify step boundaries, cancellation, owner close, shutdown, nested-pump rejection, terminal stability, slot reuse, and absence of artifact publication.

That is persistent test-continuation state, not persistent compiler-preparation state. No compiler cursor, source snapshot, dependency payload, or preparation progress is retained. No genuine production preparation step was integrated.

A source-read step is not safe to add to desktop::tick because VFS/block I/O can wait unpredictably and VFS state has no generation contract. A compiler/cache/link step cannot be called from the pump because current compiler stack use exceeds the service allocation and compiler state is global/non-reentrant. Returning from the existing Build callback with only “prepared” state would change the meaning of the public Build command. No internal-only pump fallback was added that could bypass these constraints.

The only real production Build remains synchronous and keeps its existing compiler semantics. There is no asynchronous compilation, synthetic progress, or test continuation access through application Build callbacks.

## 8. Cancellation, artifact freshness, and ABI

The manager’s existing one-slot state, owner/generation checks, queued cancellation, safe test-step cancellation, owner-close cleanup, shutdown admission barrier, and release rules remain unchanged. Production synchronous compiler work still cannot be cancelled in flight. The test continuation acknowledges cancellation at a safe test boundary; no memory is reclaimed while its pump is active.

The test job ends FAILED and publishes no artifact path, hash, entry point, or success state. It cannot reach the ELF writer or enable Run/Debug. The production synchronous path remains authoritative: it writes the expected artifact, then the service reopens, validates, hashes, and reports it. No generation-specific asynchronous staging or source digest was added.

The public gx_build_request, gx_build_snapshot, SDK ABI, NativeElf host-call layout, and start/poll/release behavior are unchanged. No async capability was exposed and the Build command still means synchronous compilation.

## 9. Verification

| Check | COMP3R5 result |
|---|---|
| AMD64 compiler/service stack audit | PASS: 20 freestanding production translation units compiled with -fstack-usage; 392 static records; direct call-path review reproduced the 242,512-byte cache subtotal and found a 142,976-byte link subtotal. This is evidence of risk, not a safety pass. |
| Audit object linkage | PASS: 20 objects combined with ld -r. No full kernel link was performed. |
| Server compiler regression | PASS: 9/9 scripts, including COMP3R1 recursion-boundary coverage. |
| Compiler diagnostics, source mapping, cache, linker, artifact stages | PASS. |
| NativeElf validator/runtime/trampoline/ABI | PASS: 4/4. |
| Build-job manager and desktop pump wiring | PASS: 27,352-byte manager, capacity one; lifecycle and pump-wiring assertions passed. |
| Hosted build responsiveness | PASS: hosted worker responsiveness, live output, admission, and completion. This is not guest responsiveness. |
| Source snapshot tests | Not run: no production snapshot implementation exists to exercise. No separate permissive test implementation was added. |
| New continuation tests | None added. Existing manager test suite passed and exercises the COMP3R3/R4 test continuation and lifecycle. |
| Developer Studio build and CTest | Not rerun: only this report changed in Studio; production Studio and shared Build interfaces are unchanged. COMP3R1’s 34/34 result is historical. |
| Freestanding production object compilation | PASS as an audit: 20 current production translation units compiled with AMD64 freestanding flags. No production source was modified. |
| Full kernel build | Not run. |
| QEMU/guest stack guard/high-water/responsiveness | Not run. No exclusive guest reservation was needed or claimed. |

All generated COMP3R5 validation outputs remain untracked under Server out/comp3r5-validation/ and kernel/out/comp3r5-validation/. Existing COMP3R2/3/4 and earlier validation directories were not overwritten.

## 10. Outcome and next prerequisites

**Outcome B — Partial Foundation.** The current architecture remains viable as a one-slot cooperative design, but this task did not establish its compiler stack or source-identity prerequisites. The synchronous compiler path also retains a production stack risk independent of asynchronous work. No production source or public interface was changed to avoid moving work across an unproven boundary.

Recommended next scope: first reduce or otherwise safely bound the large automatic compiler frames, add an explicitly owned stack with a real overflow boundary and quiescent lifetime, and measure high-water under the scenario matrix above. Separately establish a VFS mutation/mount-generation contract or capture a fully owned bounded source/dependency set before any cross-tick work. Only after those gates pass should a genuine preparation stage be considered. Do not start full asynchronous compilation, COMP4, Phase 30C, or Server release integration as part of this closeout.

## 11. Required report index

1. **Outcome:** B — Partial Foundation.
2. **Stale-prompt result:** clear; no COMP3R5 report or marker existed.
3. **Starting/ending HEADs:** Studio started at 63168f1 and ends at this report's commit (full SHA in task closeout); Server started and ends at 9594543f37959ee10aa207cb8c3fd6c9f482964e.
4. **Branches/worktrees:** main and v0.5_DEVELOPER_STUDIO retained; detached bf78d51 worktree untouched.
5. **COMP3R4 baseline:** desktop test pump, reentrancy guard, manager state, synchronous compiler path verified.
6. **Static stack analysis:** 20 TUs, GCC 15.2, 392 static frame records.
7. **Reachable compiler paths:** cache subtotal 242,512; link subtotal 142,976; unrelated smoke frame excluded.
8. **128 KiB determination:** not defensible; two reachable static path subtotals exceed 131,072.
9. **Stack guard:** none for compiler service; no new guard added.
10. **High-water:** no compiler-stack watermark or runtime measurement.
11. **Provisional stack:** 384 KiB remains unqualified; smallest prerequisite is owned, guarded stack plus measured headroom.
12. **One-job memory:** Manager 27,352 bytes; current selected BSS 6,148,816; prospective selected subtotal 8,671,952 with unallocated 2,523,136-byte estimate.
13. **VFS/source findings:** synchronous reads, mutable global tables, no generation/lease/snapshot transaction.
14. **Source snapshot:** none; the job owns request/membership metadata only.
15. **Dependency closure:** current bounded include parser and cache checks retained; no snapshot closure copied.
16. **Source mutation:** no cross-read mutation or mount-generation detection; read errors fail synchronously.
17. **Mount generation:** no VFS mount-generation identifier exists.
18. **Request/source identity:** request strings and ordered paths owned; no content snapshot digest.
19. **Persistent continuation:** fixed test state survives ticks; no persistent compiler-preparation state.
20. **First real work stage:** none; bounded VFS read and compiler stack prerequisites fail.
21. **Bounded step guarantee:** test continuation is one arithmetic unit per allowed pump only.
22. **Progress:** test-only step lines are actual; no compiler percentage or preparation progress is emitted.
23. **Cancellation:** test boundary cancellation remains; active synchronous compiler cannot be interrupted.
24. **Owner close:** generation/owner cleanup retained and hosted-tested.
25. **Shutdown:** admission barrier and no-reclaim-while-active behavior retained and hosted-tested.
26. **Generation/handle safety:** existing slot/generation checks unchanged and hosted-tested.
27. **Artifact freshness:** synchronous ELF validate/hash path unchanged; no source digest or async staging.
28. **ABI:** public build structs/callbacks and NativeElf host-call layout unchanged.
29. **Snapshot tests:** none; no snapshot production code exists.
30. **Continuation tests:** existing manager/lifecycle suite passed.
31. **Server compiler suite:** 9/9 PASS.
32. **NativeElf/ABI:** 4/4 PASS.
33. **Studio CTest:** not rerun because Studio production/shared code unchanged; 34/34 remains historical.
34. **Freestanding compile:** 20/20 current audit objects compiled.
35. **Full kernel build:** not run.
36. **Guest runtime qualification:** not run.
37. **QAC:** QAC3 open, C3 next; no COMP3R5 reservation or QEMU launch.
38. **Production files:** none modified.
39. **Added tests:** none.
40. **Evidence directories:** Server out/comp3r5-validation/ and kernel/out/comp3r5-validation/.
41. **Documentation:** this file.
42. **Local commits:** Studio report commit recorded in task closeout; no Server source commit.
43. **Push:** not attempted.
44. **Ahead/behind:** `git rev-list --left-right --count '@{upstream}...HEAD'` was 0 behind / 1 ahead for both at start; at closeout, Studio is 0 behind / 1 ahead and Server is synchronized at 0 behind / 0 ahead.
45. **Final worktrees:** Studio report committed; Server tracked tree unchanged; all validation output remains untracked; detached deletions preserved.
46. **Remaining safety blockers:** current production stack, no guard/high-water, no coherent VFS snapshot or generation contract.
47. **Recommended COMP3R6 scope:** bound/guard/measure stack and establish source consistency before any real cross-tick preparation.
48. **Later work:** COMP3R6, COMP4, Phase 30C, and Server release integration were not started.
