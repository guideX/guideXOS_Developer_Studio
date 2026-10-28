# TOOLS10: Project-Wide C/C++ Symbol Search and Navigation

## Result

TOOLS10 adds **Tools → Project Symbols**, a read-only project-wide navigator for lexical function, method, class, struct, enum, and namespace candidates. Results show the symbol category, project-relative file, 1-based line, and bounded signature excerpt. Name and file filters are case-insensitive substring matches. Results are sorted by relative path, line, byte offset, name, and category; duplicate names remain separate candidates.

This is lexical navigation. It is not a C++ parser, language server, semantic index, or completion engine. Build and Run continue to use the project's existing source-file inventory.

## Reused scanner infrastructure

The feature does not add a second lexer. The existing `ProjectSearchService` performs bounded project traversal, applies the established generated/dependency directory exclusions, filters supported C/C++ source and header extensions, skips binary or unavailable files, and supplies one file view at a time. Its scan visitor calls the TOOLS9 `DocumentOutlineScan`, which uses the shared C/C++ lexer. The literal text matcher is bypassed for this visitor request.

Supported extensions are `.c`, `.h`, `.cc`, `.cpp`, `.cxx`, `.hh`, `.hpp`, and `.hxx`, with case-insensitive extension checks. Excluded directories are `.git`, `.vs`, `.idea`, `out`, `build`, `bin`, `obj`, `dist`, `node_modules`, `.hg`, `.svn`, `.cache`, `target`, `bazel-out`, `.guidexos`, `.developerstudio`, `.gxdeploy`, `.gxbuild`, `developerstudio-tmp`, `guidexos-tmp`, and names beginning with `cmake-build-`.

The scanner does not expand macros, instantiate templates, resolve aliases or types, resolve overloads, or analyze across translation units. Conditional-compilation branches are scanned lexically without evaluating which branch is active. See [TOOLS9 Document Outline](TOOLS9_DOCUMENT_OUTLINE.md) for declaration recognition details and parser limitations.

## Unsaved buffers

At scan start, the app copies every dirty open document into the existing bounded Project Search snapshot array. Dirty buffers override disk contents for their paths; saved files are read from disk. The snapshot count is bounded by the editor's existing open-document limit (8 hosted documents, 4 Native ELF documents). The scan retains no pointer into a mutable editor buffer.

On selection, an already-open file is checked against its current editor buffer, including later unsaved edits. A closed file is freshly read from disk. If the symbol no longer matches its scanned category, signature, or duplicate ordinal, navigation is rejected and the project scan restarts. Line shifts alone are allowed; the current line and offset are recalculated.

## Navigation safety

Before opening or moving the caret, the app verifies the captured project ID, generation, and normalized root against the active project. The result path must be relative and traversal-free; `JoinWorkspacePath` normalizes it and enforces a separator-bounded project-root containment check. The target must still exist as a regular supported text file, fit the existing editor limit, and contain no binary bytes.

The current source is rescanned immediately before navigation. If opening a closed file changes its contents between validation and open, the newly opened document is closed and the previous active document, scroll position, and focus are restored. The symbol path performs no filesystem writes and does not change source buffers. The scanner request disables write, directory-creation, and remove callbacks.

## Resource limits

| Resource | Hosted C++ model | Native ELF |
| --- | ---: | ---: |
| Files visited by the symbol visitor | 512 | 64 |
| Maximum bytes in one source file | 256 KiB | 16 KiB |
| Total source bytes consumed | 64 MiB | 2 MiB |
| Retained candidates | 256 | 32 |
| Displayed results | 100 | 24 |
| Name filter | 64 bytes | 64 bytes |
| File filter | 128 bytes | 128 bytes |
| Relative path buffer | 768 bytes | 256 bytes |
| Signature excerpt | 128 bytes | 128 bytes |
| Maximum traversal depth | 64 | 64 |
| Work budget passed per UI poll | 1 entry | 1 entry |
| Synchronous source reads per poll | At most 1 | At most 1 |
| Overall symbol-scan duration | 60 seconds | 20 seconds |
| Outline symbols retained per source file | 128 | 48 |
| Declaration lookahead | 96 tokens / 1,024 bytes | 96 tokens / 1,024 bytes |
| Scope nesting | 32 | 32 |
| Fixed result and UI state storage | 278,120 bytes | 25,768 bytes |

The per-poll budget is passed to the shared scanner, whose hard ceiling is 16 directory entries and one synchronous file read per poll. The Project Symbols UI passes a budget of one. The shared service also caps file size at the editor limit, traversal depth at 64, pending directories at 512 hosted / 32 Native ELF, and visited directories at 4,096 hosted / 128 Native ELF. The tighter symbol-visitor limits above stop a scan earlier.

`ProjectSymbolResults` occupies 276,824 bytes in the hosted model and 24,984 bytes in the Native ELF model; that fixed object includes candidate storage and the TOOLS9 outline scratch object. The remaining navigator state (filters, project identity, status, and selection) accounts for 1,296 bytes hosted / 784 bytes Native ELF. The existing shared `ProjectSearchService` occupies 5,290,248 bytes hosted / 287,144 bytes Native ELF and is reused by existing project search tools; it is not allocated a second time for Project Symbols.

Clang `-fstack-usage` measurements at the Native ELF build settings report the following direct frame sums for the new path-resolution chain `activateProjectSymbol → ProjectSymbolsResolvePath → JoinWorkspacePath → NormalizePath`: 4,080 bytes AMD64 and 4,240 bytes ARM64. These figures exclude the event-loop caller frame and ABI bookkeeping. The symbol visitor and validation reuse the TOOLS9 scanner's bounded fixed scope/lookahead arrays; there is no recursion, heap allocation, or new thread.

All result, filter, traversal, and snapshot storage is fixed-capacity. Result overflow, visitor limits, skipped files, and timeouts are surfaced as incomplete/truncated status with partial counts. Cancellation is cooperative between bounded polls.

## Tests and hosted UI evidence

`developer_studio_project_symbols_test` uses `TEST_CHECK` rather than `assert()` so fixture setup and checks execute in Release. It covers multiple files, duplicate names, all six categories, filtering, deterministic order, capacity bounds, empty projects, unsupported/binary files, comments and strings, dirty-buffer snapshots, modified and removed files, project identity changes, cancellation/rescan, path containment, navigation revalidation, no filesystem writes, and Build/Run inventory isolation. The existing Document Outline regression target remains in the full CTest suite.

The extended hosted UI test launches the real experimental Server with a disposable runtime and actual GUI input. It covers Tools-menu activation, scanning a multi-file fixture, name and filename filtering, cross-document navigation, unsaved contents, stale-result rejection and refresh, cancellation, source-file immutability, and clean app/Server shutdown. The final Debug CTest run passed this acceptance test in 125.40 seconds. Its evidence log is recorded at:

`D:\dev\TOOLS10\build-cmake-debug\tools10-hosted-ui-evidence\project-symbols-hosted-ui.log`

## Debug and Release verification

Both CMake builds use separate clean Ninja build directories under `D:\dev\TOOLS10`. CTest runs serially because unrelated existing tests use fixed temporary directory names.

| Configuration | CMake build | CTest |
| --- | --- | --- |
| Debug | `D:\dev\TOOLS10\build-cmake-debug` | 41/41 passed (40 model/regression tests and hosted UI acceptance) |
| Release | `D:\dev\TOOLS10\build-cmake-release` | 40/40 model/regression tests passed |

Release test executables explicitly undefine `NDEBUG`, keeping `assert()` checks and their legacy fixture setup active in test sources; the library remains compiled with normal Release flags. The new Project Symbols tests use `TEST_CHECK` and do not rely on assertion side effects.

## AMD64 and ARM64 package evidence

Architecture builds are staged separately under `D:\dev\TOOLS10\package-amd64` and `D:\dev\TOOLS10\package-arm64`. The hosted UI uses the assembled package at `D:\dev\TOOLS10\package-multiarch\Apps\DeveloperStudio`. Its `app.json` manifest declares both architectures and resolves to an ELF file for each target.

| Target | ELF header machine | Size | SHA256 |
| --- | --- | ---: | --- |
| AMD64 | `Advanced Micro Devices X86-64` (ELF64) | 1,258,724 bytes | `598F8958A3A106620B8E9086C68D2BDB982D3FD515A1EE2E4E854565BD74FC30` |
| ARM64 | `AArch64` (ELF64) | 1,445,692 bytes | `5B0BB66DB17E05737997A262FB78118C7DF156DD563CF0EDFB609D6B85E07C1C` |

## Qualification boundary and TOOLS11 recommendation

TOOLS10 qualification is hosted-first. No QEMU guest was launched. Guest qualification remains pending, and .NET Support retains primary QEMU qualification priority. No Server storage or ABI code, Compilation or Debugging worktree, production package, or Replace Apply behavior is changed.

If a future increment is requested, keep this lexical navigator stable and evaluate any parser-backed or semantic indexing as a separately bounded feature with its own resource and ABI review. TOOLS11 is not started by this change.
