# TOOLS11 Native Resource Readiness

## Qualification result

**Outcome: B — `TOOLS11_HOSTED_VERIFIED_NATIVE_PENDING`.** The Native ELF stack contract and image bounds are defined in the Server integration source, both Debug/Release hosted suites and both architecture packages pass, and the resource correction is confined to Developer Studio. Guest execution remains unverified because TOOLS11 explicitly excludes QEMU and unmanaged guests; guest acceptance is a separate release gate.

The source checkout started on branch `DEVELOPER_STUDIO_TOOLS` at `19b693c3d0881499b46be255b524d8cc2203d6b7`. The Server integration checkout was read-only at `a13768dea9e5808baadb3c2908a8fca463505f16`. The Server ABI, Native VFS, compilation, debugging, storage, and production packages were not changed.

The TOOLS10 baseline report, [`docs/TOOLS10_PROJECT_SYMBOL_NAVIGATION.md`](TOOLS10_PROJECT_SYMBOL_NAVIGATION.md), records the prior 41/41 Debug and 40/40 Release counts and the AMD64/ARM64 staging roots. Legacy ignored package trees from TOOLS1R through TOOLS8 remained in `out/`; they were not cleaned or reused. The existing integrated Server package at `D:\dev\guideXOS_DeveloperStudio_Tools\Apps\DeveloperStudio` contained 1,128,572-byte AMD64 and 1,286,380-byte ARM64 ELFs (both dated 2026-10-26). TOOLS11 packages were staged separately under `out/validation/tools11-staging/Apps/DeveloperStudio/`.

## Native stack

The authoritative app stack is `NATIVE_ELF_APPLICATION_STACK_SIZE = 512 KiB` in `kernel/core/native_elf/native_elf_stack_policy.h`. `APPLICATION_STACK_BASE` places it at the tail of the 512 MiB NativeElf region (`0x6ff80000` through `0x70000000` for the current `0x50000000` image base). This stack is separate from the loaded image. `APPLICATION_STACK_GUARD_BYTES` is zero, so the runtime provides no guard page. The policy reserves 8 KiB for runtime safety and 40 bytes for the AMD64 trampoline entry.

The TOOLS10 path-resolution figures are reproducible sums of four direct function frames from Clang `-fstack-usage` output, using the packaged Native ELF flags without optimization (Clang defaults to `-O0`):

| Path-resolution frame | AMD64 | ARM64 |
| --- | ---: | ---: |
| `activateProjectSymbol` | 936 B | 1,040 B |
| `ProjectSymbolsResolvePath` | 56 B | 64 B |
| `JoinWorkspacePath` | 856 B | 864 B |
| `NormalizePath` | 2,232 B | 2,272 B |
| **TOOLS10 sum** | **4,080 B** | **4,240 B** |

These figures are not maximum call chains: they omit the surrounding application/UI frames and other work paths. One concrete Go To File stale-index refresh path is `gx_main` → `developerStudioMain` → `handleGoToFileKey` → `activateGoToFileResult` → `refreshGoToFileResults`. The direct frames on that path sum to:

| Direct frames summed | AMD64 `-O0` | ARM64 `-O0` | AMD64 `-O3` | ARM64 `-O3` |
| --- | ---: | ---: | ---: | ---: |
| Go To File stale-index refresh path | 68,808 B | 69,376 B | 2,440 B | 1,552 B |

This sum excludes child helpers called by `refreshGoToFileResults` and the Server's caller frames; it is a measured path estimate, not a whole-program maximum proof. A Project Symbols scan path through `pollProjectSearch`, `pollProjectSymbols`, `ProjectSearchPoll`, `DocumentOutlineScan`, and `CppLexerNext` measures 37,184 B on AMD64 and 37,888 B on ARM64 at `-O0`, including `gx_main` and `developerStudioMain`. Against the 512 KiB stack, the named Go To File frames leave more than 436 KiB before the omitted helper frames; after setting aside the 8 KiB runtime reserve and 40-byte entry overhead, more than 436 KiB remains. This is a wide measured margin, not a proof about every indirect call path.

At `-O3`, the measured UI frames shrink materially: `developerStudioMain` falls from 35,176 B to 1,768 B on AMD64 and from 35,808 B to 1,120 B on ARM64; `refreshGoToFileResults` falls from 16,792/16,832 B to 296/336 B. The production `build.ps1` supplies no optimization flag, so the `-O0` data is the relevant build configuration. Reports were generated with Clang 22.1.8 and `-fstack-usage`; raw files are under `out/tools11-stack-usage/{amd64-O0,amd64-O3,arm64-O0,arm64-O3}`.

The reviewed scan/navigation paths do not recurse based on project or source nesting. Project directory traversal uses a bounded queue and a maximum directory depth of 32; the C++ lexer and outline scanner walk tokens iteratively and keep nesting in fixed-capacity storage. Deeper inputs therefore do not add one native call frame per directory or brace. The `.su` files are per-function reports and do not certify a whole-program maximum across indirect UI callbacks.

## Static memory and ownership

The hosted Native ELF packages use fixed-capacity storage. The reviewed project search, Project Symbols, Outline, Task List, Replace Preview, and Go To File paths use static application storage; no `new`/`malloc` allocation path was found in these bounded scanners. The search service owns its scan buffer, result arrays, and four dirty-document snapshots. The service is shared by Find in Files, Replace Preview, Task List, and Project Symbols. Project Symbols results, the active-document Outline, Task List results, Replace Preview plan, and Go To File inventory/results remain in separate globals while the user switches tools. The registry itself is a 344-byte dispatch table; it does not own the result payloads.

Measured Native object sizes (bytes):

| Object/storage | Bytes | Ownership note |
| --- | ---: | --- |
| `ProjectSearchService` | 287,144 | Shared scanner, results, scan buffer, four dirty snapshots |
| `ProjectSymbolResults` | 24,984 | Includes symbol candidates and outline scratch |
| `DocumentOutline` | 10,088 | Active-document Outline state |
| `ProjectTaskList` | 12,692 | Task rows |
| `ReplacePreviewPlan` | 17,856 | Preview rows |
| Go To File file inventory | 8,192 | 32 fixed path entries |
| `DeveloperToolFileSearchResults` | 16,460 | Go To File filtered results |
| `DeveloperToolRegistry` | 344 | Dispatch records only |
| `ReferenceSearchService` | 1,211,712 | Separate reference-search scanner and results |
| `WorkspaceController` | 662,912 | Includes its `WorkspaceModel`; do not add both sizes |

The eight selected non-reference Tools objects above total 377,760 bytes. This is not the entire application BSS: the loaded ELF contains other editor, debugger, workspace, and startup state. Each architecture's current ELF has about 15.02 MB of zero-filled RW data (BSS), plus its file-backed load segments. The ELF's aligned mapped spans are 16,289,792 B (15.54 MiB) on AMD64 and 16,474,112 B (15.71 MiB) on ARM64.

The loader permits application images up to the 512 MiB NativeElf region (`native_elf_contract.h`). Nested debugging separately snapshots the active image into a 16 MiB fixed buffer (`NATIVE_APP_MAX_MAPPED_IMAGE_BYTES` in `native_elf_runtime.h`, allocated for parent and child in `native_elf_loader.cpp`). The current aligned spans fit that snapshot capacity with 487,424 B (476 KiB) headroom on AMD64 and 303,104 B (296 KiB) on ARM64. This nested-debug margin is tight and is a growth constraint even though the app image remains far below the loader's 512 MiB mapping limit. The 512 KiB application stack sits outside both mapped image spans, at the opposite tail of the NativeElf region.

## Correction and regressions

The UI previously held `g_projectSearchSnapshots`, a second array of four `ProjectSearchDocumentSnapshot` objects. Each Native snapshot is 16,664 B, so this duplicated 66,656 B (65.1 KiB) already reserved by `ProjectSearchService`. The UI now captures dirty buffers directly into the shared scanner's storage. Reference search retains its own independent snapshot array, so simultaneous search services cannot overwrite one another's snapshots.

`ProjectSearchStart` now recognizes a request whose snapshot pointer aliases its own service storage and avoids a second copy. It still copies external snapshots. Completed, cancelled, timed-out, failed-start, and released operations clear the scanner-owned snapshot count and bytes. Tests cover two dirty documents through the alias path and verify full snapshot bytes are cleared after completion and cancellation.

The new `developer_studio_project_symbols_native_limits_test` compiles the scanner, outline, lexer, and symbol visitor with Native bounds in both hosted build configurations. It scans 64 maximum-size 16 KiB files (1 MiB total), checks the 65th-file result-limit/truncation path, reaches the 32-candidate cap and its overflow path, and verifies the 24-row display limit. Existing CTest cases cover repeated scans, path containment, unsupported/malformed inputs, stale results, cancellation, project switching, navigation, and Build/Run/Debugging regression behavior. The two real-Server UI scripts additionally exercise panel switching and document/project lifecycle behavior. Replace Apply remains disabled and is asserted by the Task List/Find/Replace harness.

## Debug/Release parity

The earlier Debug 41/41 versus Release 40/40 difference came from local CMake cache values, not a Debug/Release registration branch: `GUIDEXOS_ENABLE_DOCUMENT_OUTLINE_HOSTED_UI_TEST` was ON in the Debug tree and OFF in the Release tree. The Task List hosted UI option was OFF in both. Both hosted UI options are now enabled for both TOOLS11 trees. A Native-capacity regression target was added to both. Each final configuration therefore registers the same 43 tests: 40 existing model tests, one Native-capacity regression, and two hosted UI tests. Release's `-DNDEBUG` is followed by `-UNDEBUG` for test targets, preserving assertions.

Final results: Debug **43/43 passed**; Release **43/43 passed**. Build and final CTest logs are kept in the ignored `out/` tree (`out/tools11-debug-ctest-final.log`, `out/tools11-release-ctest-final.log`). One earlier Release Task List run timed out while reopening a fixture project; its isolated retry and the final complete Release suite both passed.

## Hosted UI evidence

Both tests use `guideXOSServer.experimental.exe` with the isolated TOOLS11 AMD64 package and disposable runtime fixtures:

- `developer_studio_task_list_hosted_ui_test`: Task List open/close and reopen; Find in Files; Go to File and Go to Line; Replace Preview with Apply disabled; scan cancellation; project switching; cleanup. Evidence: `out/tools11-debug/tools7-hosted-ui-evidence/task-list-hosted-ui.log`.
- `developer_studio_project_symbols_hosted_ui_test`: Project Symbols and Outline scans/filtering/navigation; stale symbol selection rejection; cancellation; switching documents/projects and tools; fixture files unchanged. Evidence: `out/tools11-debug/tools10-hosted-ui-evidence/project-symbols-hosted-ui.log`.

These are real hosted UI interaction and execution-marker results. No screenshots were captured, so this qualification makes no pixel-level acceptance claim. Release runs produce corresponding logs under `out/tools11-release/tools7-hosted-ui-evidence/` and `out/tools11-release/tools10-hosted-ui-evidence/`.

## Package qualification

Both architectures are staged under `out/validation/tools11-staging/Apps/DeveloperStudio/`, independently of Server production packages. The package audit checks manifest entries, architectures, package contents, and stripped section/debug metadata. Final file sizes and SHA256 hashes are recorded below after the final rebuild:

| Architecture | ELF bytes | SHA256 |
| --- | ---: | --- |
| AMD64 | 1,259,204 | `B7FF099CAE474EFFB6B7BBFBC77AB8F6859DD9CEA2F6A4CBD35AA430926CBF6E` |
| ARM64 | 1,446,140 | `8E7CE91EC2A4C9274451B225DF9DEBB197D8F672CA58DD698C922AD1AC366C9B` |

The package verifier is `tests/validate-developer-studio-package.ps1`. Raw Native stack usage is under `out/tools11-stack-usage/`; the package build uses Clang's default `-O0` and no optimization option.

## Guest gate and future navigation

No QEMU or unmanaged guest was launched. Guest launch, native stack/runtime behavior, and on-guest UI acceptance remain a separate release gate; hosted Server tests do not replace that gate.

No parser or language server was added. Extending the lexical scanner is inexpensive and bounded but retains false positives around macros, conditional compilation, and templates. A small selected-declaration parser could improve common declarations at moderate maintenance cost, but still cannot model real C++ preprocessing or template semantics. A Clang-backed index offers the strongest semantic and cross-file results, at substantially higher image, memory, CPU, toolchain, compile-command, and runtime-dependency cost; embedding it in this freestanding app conflicts with the Native ELF constraints.

For TOOLS12, keep lexical navigation and gather concrete user cases before choosing a parser. First protect the current 16 MiB nested-debug snapshot margin through measured data reduction or a separately reviewed Server-side capacity change, then run the distinct guest acceptance gate. If semantic navigation is justified, evaluate an offline/host-side index driven by real build metadata before considering a compiler frontend inside the Native app.
