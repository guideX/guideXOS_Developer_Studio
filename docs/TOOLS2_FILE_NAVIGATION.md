# TOOLS2 File Navigation

## Result

**Outcome A — `TOOLS2_HOSTED_VERIFIED`.** Go to File and Go to Line are
registered Developer Studio tools. The full hosted CTest suite, the default
CMake build, and staged AMD64 and ARM64 Native ELF package builds passed.
Live hosted UI and QAC guest qualification remain pending; the required
Server executable and QAC CLI were unavailable, and a QEMU process was already
active.

## Architecture and reused APIs

The visible Tools menu now contains **Go to File**, **Go to Line**, and the
existing **Find in Files** command. All three use the fixed-capacity
`DeveloperToolRegistry`. The registry holds three of its four command slots;
it remains a static command table, not a dynamic plugin system.

Go to File reuses the active project's existing `SymbolDatabase` file index.
That index is built by the workspace controller and contains project C/C++
source and header paths. Go to File does not recursively scan the filesystem
or read file contents. The current inventory does not include arbitrary text
assets such as JSON or Markdown, so those files are outside Go to File's
search scope.

Selecting a result passes its copied project-relative path and the captured
project ID to `WorkspaceControllerOpenDocumentAtLocation`. That existing
workspace API validates the active project, path containment, file type,
readability, binary content, and editor size limit before activating a
document. No file contents or document pointers are retained by the result
list.

Go to Line operates on the current editable document buffer. It does not
reopen, rewrite, or save the file. A successful move uses the existing
editor caret and `keepCaretVisible` viewport handling.

## Go to File

Open it from **Tools → Go to File** or press **Ctrl+P**. Enter a bounded,
ASCII case-insensitive substring. Filename matches sort before relative-path
matches; each group sorts by relative path. Up/Down moves the selection,
Enter opens it, a double-click opens it, and Escape closes the panel.

The result paths are copied out of the symbol database and bound to the
project ID, project generation, and symbol-index generation captured when the
panel is opened. Project changes close the tool. A changed index refreshes
results and requires a new selection. A removed or renamed file is rejected by
the workspace open API without replacing the active document.

## Go to Line

Open it from **Tools → Go to Line** or press **Ctrl+G**. Enter a positive,
1-based decimal line number and press Enter. The parser accepts at most ten
digits and checks `uint32_t` overflow. The requested line must exist in the
currently active document.

The caret moves to the first byte of the requested line, the selection clears,
and the editor scrolls that line into view. LF and CRLF are both handled by
the existing line model; the caret starts after the line-feed byte. An empty
document has one valid line, whose start is byte offset zero. Invalid input,
an out-of-range line, or a changed active document leaves the caret and
selection unchanged.

## Shortcuts and lifecycle

| Command | Shortcut | Dispatch |
| --- | --- | --- |
| Go to File | `Ctrl+P` | Shared registry activation callback |
| Go to Line | `Ctrl+G` | Shared registry activation callback |
| Find in Files | `Ctrl+Shift+F` | Existing shared registry activation callback |

The existing shortcut audit found no `Ctrl+P` or `Ctrl+G` command to replace.
`Ctrl+Shift+F` remains assigned to Find in Files. Menu selection and shortcuts
call the same command wrappers.

The registry closes the active utility before activating a different one.
Repeated activation keeps an already-open Go to File query and selection, or
an already-open Go to Line input for the same document. Closing/cancelling
discards only utility state. Project switches close the active registry tool;
document identity is checked again before Go to Line moves the caret.

## Bounds and safety

- Search input: 128 bytes.
- Indexed candidates: at most 2,048 hosted entries (32 in the bare-metal
  model configuration).
- Displayed search results: at most 64, with a visible truncation count.
- Stored paths: at most `kMaxPathBytes - 1` bytes; displayed paths are clipped
  to a bounded UI buffer.
- Line input: at most 10 decimal digits, checked against `uint32_t` overflow
  and the actual document line count.
- Absolute paths are converted only when they are contained within the active
  project root; empty components, traversal, drive prefixes, and control
  characters are rejected.
- A selected file is revalidated by the workspace controller at open time.
  Filesystem symlink metadata is not exposed by the hosted ABI; the Server
  rejects symlink path components and unknown file kinds are not indexed.

## Hosted regression and package evidence

`tests/tools_test.cpp` covers filename-first matching, relative-path matching,
empty results, oversized queries, result limits, project identity changes,
shortcut-to-command mapping for all three Tools, positive decimal parsing,
integer overflow, LF/CRLF line starts, empty documents, out-of-range lines,
and selection preservation on failure.
`tests/navigation_test.cpp` exercises a searched file through the existing
workspace open API and covers missing files, traversal rejection, project
mismatch, and failure-state preservation. The existing Find in Files and
navigation tests remain in the full suite.

- CMake build: **103/103 build steps passed** using
  `cmake --build out/tools2-cmake --parallel`.
- Hosted CTest: **35/35 passed**.
- AMD64 package: **PASS**, ELF64 x86-64, 1,175,684 bytes,
  SHA256 `C2C69CD2F645CE065DAC5280308BCC05F238EA48A62C33694A35C0A3069EACAA`.
- ARM64 package: **PASS**, ELF64 AArch64, 1,345,428 bytes,
  SHA256 `99F7CEA1CE207F66CE5407417AACC1BC8F0D8884A88210797D9C7BCFC94BA9E7`.

Both package builds used the existing Server SDK headers and the TOOLS2-only
staging root `out/tools2-package`. The production Server package was not
written.

## Qualification status and limitations

Live hosted UI acceptance is **PENDING** because
`D:\dev\guideXOS_DeveloperStudio_Tools\guideXOSServer.experimental.exe` was
absent. QAC guest acceptance is **PENDING** because `qac.exe` was unavailable;
at the initial audit an unrelated QEMU system process was active, and no QEMU
instance was started or modified.

Go to File currently searches only the C/C++ source/header inventory maintained
by the symbol index. It does not search project metadata, documentation,
configuration files, or arbitrary text assets. Its indexed candidate and
display-result capacities are intentionally bounded.
