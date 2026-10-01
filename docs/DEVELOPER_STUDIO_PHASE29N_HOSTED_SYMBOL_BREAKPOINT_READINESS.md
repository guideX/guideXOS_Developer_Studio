# guideXOS Developer Studio — Phase 29N Hosted Symbol and Source-Breakpoint Readiness

## Outcome

Outcome A: the hosted source breakpoint and symbol readiness path is implemented, documented, and validated. The historical hosted wait failure was a validation-contract defect (classification G), not a mapper or breakpoint-binding defect. The test now follows the actual lifecycle: persist an F9 workspace breakpoint, start the debug session with Ctrl+F5, observe the transient runtime Pending state, then verify the mapped and installed source breakpoint.

## Repository baseline and scope

Work was performed in two repositories:

- Standalone Developer Studio: branch main, starting HEAD 371078b57f4678ead5a882ba37a94e84ca927819.
- Server: branch v0.5_DEVELOPER_STUDIO, starting HEAD c1117803291a4bac024e5d57a68dd00941e97a1a.

The phase prompt described the standalone branch as 2 commits ahead and 0 behind, and the Server branch as 1 ahead and 0 behind. At the time the work began, the available local refs instead reported 0/0 for both corresponding origin refs. No remotes, authentication, or branches were changed to reconcile this discrepancy. Final ahead/behind status is recorded after the commits below.

The accepted Phase 29M baseline, including prior QEMU evidence, was preserved. Six existing untracked standalone validation artifacts were left untouched and unstaged. The Server sentinel file was also left untracked and unchanged; its SHA-256 is recorded in the final audit.

## Historical failure and classification

The historical hosted wait for debug_breakpoint=PENDING was reproduced before production changes. F9 first persists a workspace breakpoint in NotStarted state. At that stage it has no runtime breakpoint ID and no build or symbol generation. Pending is created only after Ctrl+F5 materializes the runtime breakpoint, immediately before synchronous source mapping and backend binding.

The old wait treated the persisted workspace request as though it were already a runtime breakpoint. The run therefore failed to observe a lifecycle state that had not yet been created. Classification G is a hosted validation-contract defect. Once the harness follows the runtime lifecycle, the existing source mapper maps and binds the breakpoint correctly; no mapper algorithm change was needed.

## Symbol readiness and artifact identity

The hosted DebugSymbols build produces the project DebugSymbols ELF at build/bin/amd64/debugger-phase15.elf. This ELF is both the execution image and the DWARF source. It is not a separate debug companion. The Server package ELF is stripped and is a different deliverable.

For hosted stress session 11, the project was com.example.phase29nf11, configured for DebugSymbols and AMD64. The ELF was 12,400 bytes with SHA-256 8333A9FCBD68EDD7FF2275522ADDD1BA106E2BA90C51A947C7C60EF27EF166AC. Its DWARF source path matched src/main.cpp, with the raw compilation path D:\p29n\hosted-29n-final\session-11\src\main.cpp. The requested source path and normalized path were both src/main.cpp.

The bounded parser reported ELF64, 16 section headers, DWARF v5, .debug_info size 2702 bytes, .debug_line size 1021 bytes, 2 compilation units, 413 DIEs, 4 functions, 21 variables, 5 source files, 6 external files, and 180 line rows. It also reported truncated=1, a bounded-parser capacity indicator; the requested source line was present and mapped.

The Server package audit separately confirmed that the distributable AMD64 and ARM64 ELF images are ELF64 ET_EXEC files without section metadata or debug sections. Hosted source mapping uses the unstripped project DebugSymbols output, not either stripped package image.

## Source identity and mapping rules

The fixture source root is src. Source path normalization unifies slash direction, removes dot components, collapses separators, resolves parent components while rejecting escape above the root, supports absolute drive paths, and compares contained paths case-insensitively. External source files remain external. The mapper does not use basename fallback or nearest-line fallback. Missing source files and unmapped lines report SourceNotFound and LineNotMapped explicitly.

DebugDwarfSourceFile now retains the raw DWARF compilation path alongside the normalized source path. This adds diagnostic identity without changing existing normalization or mapping behavior. Phase 29H artifact hash identity continues to guard against stale artifact use; no stale mismatch occurred in the accepted runs.

## Breakpoint lifecycle, remapping, and F5

The workspace request begins in NotStarted, with no runtime ID, operation generation, or symbol generation. After the debug target starts, the runtime breakpoint enters Pending immediately before synchronous mapping. A successful source mapping installs it and transitions it through Mapped to Verified/BOUND with a backend address. If mapping cannot proceed, an enabled runtime request can remain Pending with a specific mapping error such as source_not_found or line_not_mapped.

Remapping is synchronous after mapper-model publication and before debug-target start. There is no per-frame polling, callback retry, or rebuild loop. In the successful hosted run, the runtime breakpoint was Verified with id 1, program generation 1, operation 2, symbol generation 1, line 37 column 1, primary address 0x00000000200016AC, and five mapped addresses.

A normal F5 key press now continues an active paused session regardless of pane focus. The bounded session 11 trace recorded source=normal_key, active=1, state=Paused, stop=Breakpoint, backend_exec=2, cap=1, can=1, context=1, context_session=1, and stop_gen=1. It then recorded Paused-to-Running, a single-step exception stop, and successful breakpoint rebind. The smoke assertions confirmed breakpoint continuation accepted, EXCEPTION_SINGLE_STEP, and rebound=true. The accepted Continue route is the normal F5 key path.

## Root cause repair and validation contract

The outline UI previously treated document == nullptr as equivalent to symbolCount == 0. In a bare-metal build where the AST symbol database is not indexed, that produced a misleading No symbols message even though it said nothing about whether the ELF/DWARF source was ready. The UI now distinguishes indexing disabled, document symbols not indexed, and no declarations found. Hosted DWARF load/readiness is represented by the actual debug-symbol loader state.

The Phase 20 smoke script now supports Phase29NReadiness. It validates the persisted F9 NotStarted request, starts the real debug lifecycle with Ctrl+F5, and checks the resulting stop and source mapping. The hosted smoke uses a bounded WAITMARK on debug_variables=PASS instead of an arbitrary long sleep, and can archive bounded successful traces when trace output options are supplied. The production change also preserves the raw DWARF source path and makes normal-key F5 continue work independently of pane focus.

Two early fixture preparations, sessions 07 and 08, failed before the accepted gate because the harness fixture initially used a mismatched project/application identity and then inherited an old untracked debugger configuration with the wrong source line. They were setup attempts, not accepted gate iterations. After correcting fixture construction, the five-run gate began at session 09.

## Verification results

- Fresh hosted readiness gate: 5/5 passed, sessions 09–13.
- Additional fresh hosted stress gate: 25/25 passed, sessions 14–38. Each run rebuilt and checked artifact identity, ELF/DWARF/source presence, source mapping and breakpoint installation, a real source stop, source/locals inspection, F5 Continue, single-step/rebind, and full debug-session teardown.
- Hosted session 11 trace confirms exact source match, breakpoint state and address, real RUNNING and Paused transitions, F5 continuation, single-step, and rebind.
- CTest Debug: 33/33 passed.
- CTest Release: 33/33 passed.
- Bare DWARF capacity check: passed, dies=397.
- AMD64 and ARM64 package audits: passed. Both package artifacts are stripped of section metadata/debug sections and package contents passed audit.
- QEMU regression: five fresh Phase 28Q boots passed. No fault, triple fault, or reset records were found. The Phase 28M guest application and cleanup passed on every boot; sentinel and package hashes remained stable.
- The broader 10-boot and 25-boot QEMU gates were not repeated because no shared mapper or guest breakpoint algorithm changed. A five-boot regression was run because the raw DWARF source path field is part of the shared mapper data structure.
- Physical hardware was not tested.

The full hosted smoke covered real source mapping, source and locals inspection, normal F5 continuation, single-step/rebind, and session shutdown. The Debug menu mouse route was not used as the accepted Continue path.

## Deliverables and repository state

Standalone changes include the symbol/source metadata, outline readiness messaging, global F5 handling, focused tests, and smoke harness updates, plus this report at docs/DEVELOPER_STUDIO_PHASE29N_HOSTED_SYMBOL_BREAKPOINT_READINESS.md. The six pre-existing untracked validation artifacts remain unstaged.

The Server repository contains only the rebuilt AMD64 and ARM64 Developer Studio package binaries as intended changes. The sentinel remains untracked.

Final commits and repository status:

- Standalone commit: The standalone Phase 29N readiness commit object ID is reported in the task completion summary; a commit cannot embed its own final object ID.
- Server package commit: a85d9c41ec6f4cac8f7a6696c2133aa4a6b1d20e.
- Server AMD64 package SHA-256: 89DE06D8D9FDF5E3B40BEB9DC9973240E8EBD12B41EDAB142CCE31D49B29F035.
- Server ARM64 package SHA-256: A0AA759694CDAD8C9191B01A1EA0000899423E10479FF784306A0EAC1B4453C4.
- Preserved untracked Server sentinel SHA-256: 967D29A9A500A3108D7331BA47A5E2053A7C2203B81A345BE990084F3F43698A.
- Both local branches are one commit ahead and zero behind their configured origin refs after these commits.
- Normal git push was attempted for both repositories. Both were rejected with GitHub SSH Permission denied (publickey); no remote or authentication settings were changed.
- The six pre-existing untracked standalone validation artifacts remain unstaged: the two debugger-phase15 fixture directories, and the guidexos.debugger.json plus .bak files under debugger-phase15 and debugger-phase3b.
- The untracked Server diagnostic sentinel remains untouched.

No tested hosted source-symbol or source-breakpoint readiness blocker remains.
