# guideXOS Developer Studio Bounded Run Project Phase Architecture

Status: minimal workspace, source editor, version 1 project creation/loading, hosted Build Project, temporary hosted Run Project, bounded C/C++ lexical syntax highlighting, active-document Find/Replace, bounded project-scoped Find in Files, bounded lexical Document Outline/Project Symbol Index, bounded lexical Go To Definition, and manually invoked lightweight lexical Code Completion

## Integration

Developer Studio is an independently maintained Native ELF application consumed by guideXOS Server's existing App Model. The Developer Studio repository owns the manifest, native source, runtime-neutral models, controller, tests, build script, and documentation. The build stages the ordinary package at `Apps/DeveloperStudio/app.json` plus `bin/amd64/developerstudio.elf`.

The canonical route remains:

```text
com.guidexos.developerstudio
  -> AppRegistry
  -> DesktopService::LaunchApp
  -> NativeElf launch resolver
  -> guidexos-c-abi-v1
  -> gx_main
```

No plugin, extension, package, or loader architecture is introduced. Developer Studio is not a built-in application and is not auto-launched at Server startup.

The active target profile is the one proven route:

```text
id:           guidexos.amd64.hosted.native
architecture: amd64
ABI:          guidexos-c-abi-v1
machine:      Windows hosted guideXOS Server
SDK:          guideXOS Native SDK v1
toolchain:    LLVM/LLD Native ELF
runner:       guideXOS Hosted Native ELF Runtime
maturity:     experimental
```

## Layering

```text
UI shell
  |
Workspace/document controller
  |
Project parser/generator, build controller, run controller, UI-independent Find/Replace, UI-independent ProjectSearchService, UI-independent lexical SymbolDatabase, syntax tokenizer/cache, and workspace/document/text-buffer models
  |
Filesystem abstraction
  |
guideXOS Native ABI
  |
Hosted Server build worker and artifact validator
```

`main.cpp` owns painting and input routing only. `developer_studio_workspace.*` owns workspace/document commands and the filesystem adapter. `developer_studio_models.*` owns normalized paths, bounds, document state, text-buffer mutation, ordering, and error codes. File I/O is supplied through a small callback interface backed by the Native ABI.

## Native ABI audit and decisions

The existing ABI provided window creation, retained rectangle/text drawing, key/mouse events, package-relative reads, and monotonic ticks. It did not provide a Native ABI folder picker, directory enumeration, arbitrary hosted workspace reads/writes, multiline text input, caret/scroll controls, clipboard, or atomic replacement.

The Server checkout now appends four hosted workspace calls to the ABI table:

- `file_stat`;
- `file_read_workspace`;
- `file_list`; and
- `file_write_all`.

They use existing `filesystem.read` and `filesystem.write` manifest permissions and accept an explicit user-entered hosted path. Developer Studio still normalizes the selected root, rejects traversal, and enforces root containment for every document operation. Package-relative `file_read_all` remains unchanged. The ABI has no colored-text call; the syntax renderer uses bounded token background rectangles plus the existing text call, so no Server ABI change is required.

Because there is no reusable Native ABI picker or multiline editor, Developer Studio owns the temporary path-entry dialog, fixed-size text buffer, caret/key/mouse editing, vertical scroll state, tabs, and Save/Discard/Cancel modal. This is intentionally a phase-local editing surface, not a general widget framework.

## Workspace and document limits

- one open workspace root at a time;
- 128 entries in the visible directory listing;
- immediate directory enumeration with deterministic directory-first, case-insensitive ordering;
- 8 maximum open documents;
- 256 KiB maximum editable file size;
- 8 navigated workspace path segments and 768-byte model paths (the hosted ABI accepts up to 240 bytes for an entered host path);
- slash-normalized, case-insensitive path comparison;
- `..` and equivalent traversal outside the selected root are rejected;
- embedded-NUL content is rejected as binary-looking;
- output history is bounded to four concise lines; and
- navigation is one directory at a time, with no unbounded recursive scan.

Supported text extensions are `.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hh`, `.hpp`, `.hxx`, `.txt`, `.md`, `.json`, `.xml`, `.css`, `.html`, `.htm`, `.js`, `.ts`, `.ps1`, `.cmd`, `.bat`, `.cmake`, `.ini`, `.cfg`, `.log`, `.mk`, `.yml`, and `.yaml`.

The path-entry dialog is temporary. Save uses direct truncate/write/flush because the ABI has no atomic replacement operation. The write result and byte count are verified; dirty state is retained on failure. Atomicity is not claimed.

## Diagnostics and tests

Stable app markers include:

```text
workspace_open=PASS|FAIL reason=<code>
document_open=PASS|FAIL reason=<code>
document_dirty=TRUE
document_save=PASS|FAIL reason=<code>
document_close=PASS
unsaved_close=SAVE|DISCARD|CANCEL
application_close=PASS
```

Document contents and full paths are not emitted by Developer Studio markers. The model test covers normalization, traversal, ordering, duplicate documents, active switching, dirty transitions, open/document/file-size limits, binary detection, and save success/failure transitions. `tests/smoke-workspace.ps1` creates only its own temporary fixture and proves the real host-filesystem controller workflow.

The hosted validation split is deliberate: model and filesystem-contract tests are automated; the real App Model launch is marker-asserted through the Server shell; keyboard and mouse editing in the compositor are additionally manual validation until a bounded input-driving harness exists. The current hosted proof covers package discovery, AMD64 ELF validation, ABI negotiation, `gx_main`, construction, target profile, window creation, initial render, and clean teardown.

## Project creation and loading slice

`developer_studio_projects.*` is UI-independent. It owns the bounded JSON cursor, version 1 metadata model, identity/path rules, deterministic serializer, Native GUI Application template text, manifest consistency checks, project creation, and project loading. The workspace controller adapts the service to `WorkspaceFileSystem`; `main.cpp` only collects bounded dialog fields, reports markers, and opens the resulting project.

The project contract is the `guidexos.project` file with these required fields in this serialized order:

```text
formatVersion:          integer 1
projectId:              lowercase reverse-domain application ID
displayName:            bounded printable human-readable name
projectKind:            native-gui-application
sourceRoot:             relative src path
applicationManifest:    relative app/app.json path
defaultTargetProfile:   guidexos.amd64.hosted.native
entryPoint:             gx_main
abi:                    guidexos-c-abi-v1
architecture:           amd64
outputName:             bounded lowercase package output stem
```

The parser accepts at most 16 KiB, rejects malformed JSON, duplicate or unknown fields, missing fields, unsupported versions, absolute or traversal paths, overlong strings, invalid IDs, invalid entry points, ABI/architecture mismatches, and unknown targets. It accepts only JSON string escapes used by the deterministic serializer; Unicode escape sequences are intentionally not part of this version 1 policy. The `com.guidexos` application namespace is reserved for guideXOS-owned applications. The target registry is currently the single proven experimental hosted AMD64 profile.

Creation validates all user input and the parent directory before writing. A destination that is a non-empty directory or any existing non-directory is rejected. The service creates `guidexos.project`, `CMakeLists.txt`, `build.ps1`, `README.md`, `app/app.json`, `src/main.cpp`, and `src/freestanding_memory.cpp`, then checks every expected file, reparses the metadata, and validates the manifest identity. New files and directories are recorded in creation order; on failure, rollback removes only those files and then those empty directories. A pre-existing empty project root is never removed.

Opening accepts a project root or the metadata path. It resolves all relative paths against the root and verifies root containment, required files, the source directory, the manifest, and the manifest entry identity. It does not write or upgrade project files. Saving `guidexos.project` writes the document first, then explicitly reloads and validates it. If validation fails, the current valid in-memory project remains active while the UI reports `project_metadata_parse=FAIL`.

The project model records format version, project ID, display name, kind, root path, source root, manifest path, target profile, entry point, ABI, architecture, output name, load state, validation state, and a bounded project error code. `WorkspaceModel` retains `hasProject=false` for workspace-only operation.

Stable project markers include:

```text
project_create_request=PASS
project_create_validation=PASS
project_create=PASS
project_create=FAIL reason=<code>
project_metadata_parse=PASS
project_metadata_parse=FAIL reason=<code>
project_open=PASS
project_open=FAIL reason=<code>
project_target=guidexos.amd64.hosted.native
project_template=native-gui-application
project_rollback=PASS
```

The hosted filesystem ABI appends `file_create_directory` and `file_remove` after the existing workspace extensions. They are exact-path, non-recursive operations protected by `filesystem.write`; the guest service still controls the set of paths it requests and performs the rollback accounting. The build calls are appended after those extensions and are hosted-development only. The five Run Project calls are appended after the build calls and are limited to the owner-bound temporary deployment path.

## Build Project vertical slice

The first build slice is deliberately derived rather than metadata-defined:

```text
Build Project / Ctrl+Shift+B
  -> dirty Save All / Cancel gate
  -> fixed Native Host Call Table request
  -> Server validation and SDK/toolchain resolution
  -> worker-thread PowerShell recipe
  -> bounded merged output and diagnostics
  -> ELF64 AMD64 ET_EXEC + gx_main validation
```

The controller derives `guidexos-native-build-script-v1`, `build.ps1`, `Debug`, and `build/bin/amd64/<outputName>.elf` from the active project. The Server accepts only the fixed project kind, target, script, configuration, and artifact shape; it does not execute arbitrary command metadata. It rejects symlink components, requires the bounded recipe marker, removes only the exact expected artifact before starting, and validates the new artifact before reporting success.

The appended ABI slots are `build_project_start`, `build_project_poll`, and `build_project_release`. They use fixed-size versioned request, output-line, and snapshot structures. The Server owns the process handle, worker, timeout, output cap, job cleanup, and runtime ownership check. The compositor remains responsive while the worker runs. Project/workspace/application close is blocked during an active build because this slice has no user-facing cancellation command.

The complete state, resolution, limits, diagnostics, and outcome matrix are documented in [BUILD_PROJECT.md](BUILD_PROJECT.md).

## Output and Diagnostics

BuildController and RunController publish to the shared bounded
`OutputService`, which retains structured records and exposes channel filters
and a Problems projection to the UI. The service is independent of widgets;
the parser stores only project-relative diagnostic paths after containment
validation. See [OUTPUT_AND_DIAGNOSTICS.md](OUTPUT_AND_DIAGNOSTICS.md) for the
record schema, limits, lifecycle, supported formats, and navigation policy.

## Syntax highlighting

The editor has a language-neutral lexical tokenizer and per-document bounded
syntax cache. It consumes UTF-8-compatible document bytes, stores UTF-8 byte
offset spans, carries block-comment and bounded C++ raw-string state between
lines, and propagates edits only until lexical-state/token-span convergence.
The cache and renderer are described in [SYNTAX_HIGHLIGHTING.md](SYNTAX_HIGHLIGHTING.md).
The pre-existing ABI only provides uncolored text and rectangle drawing, so the
renderer uses centralized token background colors and does not require a Server
ABI change.

## Lightweight Code Completion

Code Completion is a source-editor-only, manually invoked `Ctrl+Space` feature.
It is implemented by `developer_studio_completion.*` as a UI-independent,
generation-bound lexical session. The session consumes the shared syntax-cache
keyword/type list, the active document's lexical context and bounded document
word cache, and the existing project `SymbolDatabase`. It does not introduce a
second project index, background worker, Server ABI slot, compiler front end,
language-server client, or semantic type resolver.

The model extracts identifier, qualifier, member-like, comment/string,
preprocessor, inactive-branch, replacement-range, and lexical-scope context.
It collects keywords, current-scope/document/project symbols, and filtered
document words, then applies deterministic match tiers, source/kind scoring,
deduplication, overload collapsing, and stable tie-breaks. The model has fixed
candidate, text, scan, and visible-result bounds; document-word indexing keeps a
bounded conditional stack and scans each line once.

`main.cpp` owns popup placement and input routing. The popup clamps to the
editor viewport, accepts with `Enter` or `Tab`, supports bounded keyboard/mouse
selection, and dismisses on incompatible edits or focus/document changes.
Acceptance validates document/project generations, caret, replacement range, and
expected text before reusing the existing text-buffer, syntax, symbol, word,
and dirty-state paths. One bounded completion snapshot is available through the
existing `Ctrl+Z` path. See [CODE_COMPLETION.md](CODE_COMPLETION.md) for the
context grammar, sources, ranking, limits, markers, and deferred semantic work.

## Find and Replace

The active-document Find/Replace service is UI-independent and owns fixed-size
query/replacement buffers, sorted non-overlapping literal match ranges, current
navigation state, wrap status, and explicit truncation/error state. A session
binds only to a stable document ID and text generation; it never stores a raw
document pointer. Each document retains its previous query, replacement, and
options for safe document switching. Stale generations recompute matches and
clear the current range before replacement.

The editor's reusable range operations update selection, caret, dirty state,
line indexing, and mutation metadata. Replace Current feeds the existing syntax
cache invalidation path from the earliest affected line. Replace All applies a
stable snapshot backwards and requests the syntax cache's safe full rebuild
path because multiple disjoint edits cannot be expressed as one incremental
edit. Match highlights are an independent visible-line overlay; syntax spans
are never modified. See [FIND_AND_REPLACE.md](FIND_AND_REPLACE.md) for the
limits, literal/ASCII matching rules, UI, lifecycle, and replacement policy.

## Find in Files

Find in Files is a read-only, project-scoped operation implemented in
`developer_studio_project_search.*`. The service copies the normalized project
root, project identity/generation, literal options, and bounded dirty-document
snapshots. It recursively enumerates only through the existing `stat`, `list`,
and full-file `read` callbacks. Directory and file work is incrementally polled
from the Native ELF event loop; no Server search ABI or generic filesystem
traversal API was added. See [FIND_IN_FILES.md](FIND_IN_FILES.md) for exact
limits, pattern grammar, symlink policy, lifecycle, staleness, and UI policy.

## Document Outline and Project Symbol Index

`developer_studio_symbols.*` is a UI-independent lexical scanner and bounded
project database. It tokenizes C/C++ source while skipping comments, strings,
preprocessor lines, and `#if 0` blocks. It recognizes namespace/type
declarations, functions and methods, constructors/destructors, namespace-scope
variables, typedefs, and using aliases. It never expands macros or attempts
semantic parsing. The full scanner/database contract, limits, dirty-document
policy, Outline panel, and Ctrl+T picker are documented in
[`SYMBOL_INDEX.md`](SYMBOL_INDEX.md).

Project open performs one deterministic recursive source index. A later edit
calls `SymbolDatabaseIndexDocument` for only the changed document; the old
path block is replaced without rescanning other files. Open dirty buffers are
indexed from memory, and source navigation reuses the existing validated
`WorkspaceControllerOpenDocumentAtLocation` path before selecting the lexical
identifier.

## Lexical Go To Definition

`developer_studio_navigation.*` builds a UI-independent query from the active
caret or a valid identifier selection, captures explicit `::` qualifiers and
bounded lexical scopes, ranks records from the existing SymbolDatabase, and
returns either a direct result or a bounded candidate set. `F12` activates a
strong unique result; ambiguous overloads and same-name symbols remain in the
picker. `Alt+Left` and `Alt+Right` use generation-aware, project-relative,
bounded history. The implementation is intentionally lexical and does not
claim type inference, overload resolution, template or macro expansion,
include search, Clang, or language-server compatibility. See
[`GO_TO_DEFINITION.md`](GO_TO_DEFINITION.md).

## Run Project vertical slice

Run Project is an explicit build-before-run sequence:

```text
F5 / Run Project
  -> active Native GUI project and dirty Save All gate
  -> Build Project
  -> build SHA-256 and artifact revalidation
  -> owner-bound temporary App Model registration
  -> DesktopService -> NativeElf launch pipeline
  -> process/window polling
  -> close request or application exit
  -> owned-window cleanup, temporary unregister, handle release
```

The Studio run controller is runtime-neutral and owns only state, fixed-size
requests, snapshots, and host callbacks. The Server owns deployment slots,
generation handles, project/manifest/artifact validation, AppRegistry
registration, process/window observation, and cleanup. It launches through the
existing App Model and Native ELF runtime instead of adding a second executor.

The Server rejects workspace-only requests, unsupported project kinds/targets,
malformed or mismatched metadata/manifests, missing/changed/oversized/symlinked
artifacts, wrong architecture/ABI/ELF type, missing `gx_main`, installed-ID
collisions, owner mismatches, and stale generations. Temporary records are
in-memory only and never enter recent/pinned/desktop persistence. The generated
permission set is the only accepted set for the temporary app.

The append-only ABI slots are `development_run_prepare`,
`development_run_start`, `development_run_poll`,
`development_run_request_close`, and `development_run_release` at offsets
192, 200, 208, 216, and 224, with a 232-byte host table. See
[RUN_PROJECT.md](RUN_PROJECT.md) for the state/error contract and validation
procedure.

## Generated project external build

The generated application follows the proven direct LLVM/LLD sibling-application convention: `x86_64-unknown-elf`, freestanding C++11, static `ld.lld`, entry point `gx_main`, and package entry `bin/amd64/<outputName>.elf`. The generated `build.ps1` accepts explicit `-SdkInclude` and `-ToolchainRoot` values and emits the same fixed artifact path used by Build Project. No generated file depends on a Developer Studio checkout path, a Server checkout path, a username, or a machine-specific compiler location. CMake remains an optional external description. Debug, compiler discovery, and target switching remain deferred.

The project parser/generator test covers identity rules, exact serialization, duplicate and oversized metadata, root and metadata-path loading, manifest identity, required layout, deterministic repeated generation, no machine paths, non-empty destination rejection, workspace-only controller behavior, and simulated mid-generation rollback. The external generated-project validation produced an ELF64 AMD64 `ET_EXEC` image and confirmed a global `gx_main` symbol with `readelf`.

The environment created three automatic Developer Studio checkpoint commits during this phase: `d4787670522e4c242f9f012664dcf126944c46dc` (initial repository), `f1708ac0926f0be1541d776e3e85eb1e8f35ffea`, and `6b26336bfb964d5d38b88e47b7a259e9a57166f0`. They were inspected without rewriting history. The Developer Studio `.gitignore` is at the repository root and ignores build output and ELF binaries; none are tracked in this repository.

## Deferred work

Language server, semantic highlighting, semantic navigation beyond this lexical phase, Git integration, terminal, visual designer, website preview, game editor, container tooling, remote deployment, extension loading, theme selection, session restore, crash recovery, binary/hex editing, clipboard, regex, Replace in Files, redo, and generic editor undo remain deferred. Find All References is implemented as the bounded lexical `Shift+F12` phase documented in [FIND_ALL_REFERENCES.md](FIND_ALL_REFERENCES.md), conservative preview-first lexical Rename Symbol is documented in [RENAME_SYMBOL.md](RENAME_SYMBOL.md), and lightweight lexical Code Completion is documented in [CODE_COMPLETION.md](CODE_COMPLETION.md). Semantic binding, safe semantic rename, and language-service integration remain deferred. Project migration, project references, dependencies, multiple configurations, target switching, and all project kinds other than Native GUI Application remain unsupported.

## Hosted Server dependency and path policy

The hosted Server checkout requires `third_party/stb/stb_image.h` for `png_loader.cpp`. The selected header is the identical historically used guideXOS vendor file from `D:\dev\guideXOSServerV0.2\third_party\stb\stb_image.h`: stb_image v2.30, upstream revision `1692fe6e21ce7b7abbc6fcb6d1d3ff6ebe0b8537`, SHA-256 `1F8C1B6B408F26E3B20CBFBBD4758AFB3DC9B837FF1E17C258928F406148A87C`, public-domain license. It is restored at `Server\third_party\stb\stb_image.h`; Server `.gitignore` ignores other vendor content but permits this exact required header. No arbitrary latest header or package manager is used.

The four appended filesystem calls are hosted-development extensions. Their paths are explicit absolute UTF-8 host paths and are permitted only when the manifest grants `filesystem.read` or `filesystem.write`. Server validation rejects empty, relative, traversal, device/UNC, control-byte, invalid-UTF-8, overlong, and symlink-component paths; the Developer Studio model additionally enforces containment under the selected workspace root. The hosted Server does not currently know that selected root, so these calls are not a production sandbox or a bare-metal VFS contract. Limits are 240 path bytes, 256 KiB per workspace read/write, 128 visible Developer Studio entries, and 8 open documents. Reads are full-file and require a sufficiently sized buffer; list order is directory-first and deterministic; writes truncate and flush directly, with no atomic-replacement guarantee.
