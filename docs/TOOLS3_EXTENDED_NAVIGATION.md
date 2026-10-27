# TOOLS3 Extended Project File Navigation

## Result

**Outcome A — `TOOLS3_HOSTED_VERIFIED`.** Extended Go to File is implemented;
the hosted CMake build, focused regressions, full CTest suite, and AMD64 and
ARM64 staged package audits passed. Live UI acceptance was not attempted
because the required Server executable was absent. QAC guest acceptance is
pending because `qac.exe` was unavailable.

The implementation starts from source repository baseline
`92944b3519a44a9533a3976fbb99d0ca79b18ad3`. The Server integration checkout
was verified at `a5613cdcdb9e4b954bef53264201beaaa21de1cc` and was not modified.

## Architecture audit

The TOOLS2 Go to File implementation searched the active project's existing
`SymbolDatabase` documents. That index contains C/C++ source and header paths.
The editor's project open API already accepted additional plain-text formats,
but Go to File did not discover them.

Build and Run use the separate project source inventory:
`WorkspaceControllerEnumerateProjectSources` and `BuildRequestFromProject`.
The new `WorkspaceControllerEnumerateProjectNavigationFiles` API is only used
by Go to File. It does not change project metadata, source enumeration,
`BuildRequest`, Run requests, or debugger source handling. The existing C/C++
Go to File paths still come from the symbol index.

The file-open path reuses `WorkspaceControllerOpenDocumentAtLocation` and
`WorkspaceControllerOpenDocument`. They enforce project-relative containment,
regular-file status, the editor's allowlist, the editable-file size cap, and
content checks before changing the active document. A failed open leaves the
active document, caret, selection, and dirty state unchanged.

The Server Native ELF host maps VFS symlinks and other special kinds to
`Unknown`. The new inventory follows only entries classified as directories
and reads only entries classified as regular files. Unknown link-like entries
are skipped and are never traversed.

## Editor formats and exclusions

The format audit found these existing project-editor text formats before the
navigation change: C/C++ source and headers, `.txt`, `.md`, `.json`, `.xml`,
`.css`, `.html`, `.htm`, `.js`, `.ts`, `.ps1`, `.cmd`, `.bat`, `.cmake`, `.ini`,
`.cfg`, `.log`, `.mk`, `.yml`, and `.yaml`, plus the exact `guidexos.project`
metadata name. `CMakeLists.txt` is supported through `.txt`.

TOOLS3 adds plain-text editor support and Go to File discovery for `.toml`,
`.conf`, `.config`, `.properties`, the exact `.env` filename, and the exact
`.editorconfig` filename. Go to File searches the existing C/C++ index and
project text assets accepted by `IsSupportedTextPath`; it does not add syntax
highlighters, parsers, or editors. Non-C/C++ files open as ordinary text.

Files outside that explicit allowlist are not navigable. Examples include
PDFs, images, archives, executables, arbitrary binaries, and extensionless
files other than the exact supported names above. A supported extension does
not guarantee indexing: contents must pass bounded UTF-8 text validation.
NUL bytes, malformed UTF-8, disallowed ASCII controls, DEL, and C1 controls
are rejected. UTF-8 text may use tab, LF, CR, and form-feed control bytes.

## Discovery, matching, and lifecycle

When the panel opens, Go to File combines copied paths from the current C/C++
symbol index with a bounded project-root traversal of supported non-source
text files. Directories and names are sorted deterministically during
enumeration; final results are ordered with filename matches first and then
relative-path matches, using case-insensitive ordering with an exact-case
tie-break. Query matching is a bounded ASCII case-insensitive substring.
Duplicate normalized paths are removed before they count as matches.

The result row and search logic reuse the TOOLS2 Go to File panel, activation,
keyboard dispatch, and `Ctrl+P` shortcut. Go to Line (`Ctrl+G`) and Find in
Files (`Ctrl+Shift+F`) are unchanged. Search results display up to 64 rows;
the status reports `Showing 64 of N matches.` when result rows are truncated.
Inventory limits and skipped files/directories also produce a visible status.

The asset inventory is bound to the panel's captured project ID and project
generation, and is cleared when Go to File closes. Query edits reuse that
bounded snapshot. A selected result is revalidated by the workspace open API.
If a file has been removed, renamed, become unsupported, become binary, or
exceeded the editor size limit, navigation fails without changing editor
state, the text-asset inventory is refreshed, and the error is shown. C/C++
results remain governed by the symbol index generation; the open API still
revalidates each selected path, and a project reindex updates that snapshot.
Files added or changed on disk while the panel is open appear after closing
and reopening Go to File.

## Bounds and filesystem behavior

| Bound | Hosted | Bare-metal model |
| --- | ---: | ---: |
| Text-asset paths retained | 256 | 32 |
| Directories visited, including root | 64 | 16 |
| Entries examined | 4,096 | 512 |
| Directory depth below project root | 8 | 6 |
| Combined Go to File candidate paths | 2,048 | 32 |
| Displayed results | 64 | 64 |
| Query | 128 bytes | 128 bytes |
| Path storage | 768 bytes | 256 bytes |
| Editable file size | 256 KiB | 16 KiB |

The traversal uses one bounded directory listing at a time, a fixed directory
queue, and the workspace's existing single-file read buffer. The retained
text-asset paths use about 192 KiB hosted or 8 KiB bare-metal; file contents
are not retained in Go to File results. An overfull directory listing, entry
budget, directory queue, depth, asset list, source index, or combined path
list is reported as truncated or capped.

The traversal skips `.git`, `.hg`, `.svn`, `.cache`, `build`, `out`, `dist`,
`target`, `node_modules`, `bazel-out`, and directory names beginning with
`cmake-build-`. It reports how many such directories it skipped. These names
are treated as generated output or dependency trees at any project depth.
The traversal does not exclude generic names such as `vendor`, `third_party`,
`src`, `docs`, or `assets`.

Each path component from the filesystem list is checked for empty, traversal,
separator, drive-prefix, and control-byte forms. Paths are joined against the
active project root and checked again at selection time. Files are statted as
regular files before reading. Unsupported types, binary text, oversized
files, unreadable files, and unknown link-like entries are omitted and
reported through the panel status.

## Automated qualification

The new `developer_studio_project_navigation_inventory_test` covers mixed
C/C++ and text projects; Markdown, JSON, YAML, TOML, environment config, and
CMake discovery; filename and relative-path matching; duplicate elimination;
deterministic ordering; file and directory limits; unsupported, binary,
malformed UTF-8, and oversized files; generated/dependency exclusions;
unknown link entries; traversal containment; deleted files; and failed-open
editor-state preservation. It also verifies that the project source
enumerator, Build request, and Run request continue to use only the registered
C/C++ source set.

`developer_studio_tools_test` covers filename-first ranking, relative-path
matching, duplicate paths, result order and limits, and the existing tool
shortcut mappings. Existing navigation and search tests cover workspace
opening, path containment, stale/deleted results, Go to Line, and Find in
Files.

- Focused Go to File, navigation, Find in Files, and Go to Line regressions:
  **5/5 passed**.
- Full hosted CTest suite: **36/36 passed**.
- Clean hosted CMake build: **PASS**, all configured targets built.
- AMD64 and ARM64 native staging package builds: **PASS**.
- Package manifest, file set, ELF64 class, endianness, executable type,
  architecture, and sectionless-image audit: **PASS** for both packages.

The package files are in `out/tools3-package/Apps/DeveloperStudio/`.

| Architecture | Size | ELF machine | SHA256 |
| --- | ---: | --- | --- |
| AMD64 | 1,182,196 bytes | ELF64 x86-64 | `70D9F3D1B2CD6DBC40E03F9A8F04B1CBFB452D7D9F0234C2BCEDB89A60E146A5` |
| ARM64 | 1,353,828 bytes | ELF64 AArch64 | `09C3CD174F7EC38C16019B89361C353C0575853314D4C85AAC05E74DCDA07D0C` |

Both package builds used the Server SDK headers from the separately verified
Server checkout and wrote only to the TOOLS3 staging directory. No production
Server ELF was overwritten.

## UI and guest qualification

Live hosted UI qualification was skipped because
`D:\dev\guideXOS_DeveloperStudio_Tools\guideXOSServer.experimental.exe` was
not present. QAC-managed guest acceptance is **PENDING** because `qac.exe` was
unavailable. No unmanaged QEMU instance was launched.

For the TOOLS2 baseline and its historical package evidence, see
[TOOLS2_FILE_NAVIGATION.md](TOOLS2_FILE_NAVIGATION.md).
