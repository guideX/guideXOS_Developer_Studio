# TOOLS6 — Project TODO/FIXME Navigator

**Outcome A — `TOOLS6_HOSTED_VERIFIED`.** The Tools menu includes a read-only
Task List for project TODO/FIXME comments. Hosted regressions, clean hosted
build, AMD64 package build, and ARM64 package build pass. Both architecture
packages are staged under the dedicated TOOLS6 output directory. Guest UI
qualification is pending: `qac.exe` and the checkout's hosted Server UI
executable are unavailable, and no QEMU guest was started.

## Scope and behavior

Open **Tools → Task List** while a project is active. Choose **Scan / Refresh**
to scan project C and C++ source and header files. The list shows the annotation,
relative file path, line number, and a bounded comment excerpt, along with TODO,
FIXME, total-result, and files-scanned counts. Select a result with the mouse or
Up/Down; Enter or a double-click opens the source file at the reported line in
the existing editor. The editor's ordinary dirty-document snapshot is used for
the scan, so unsaved edits are reflected.

Use **Cancel** or Escape during a scan. Cancellation leaves the partial results
available and reports their state. Escape closes the panel after the scan ends.
Use **R** or the Scan button to replace the current list with a fresh scan.
Close ends the panel. Switching projects cancels an active scan; stale project
results are cleared when the Task List is activated for another project. Before
opening a result, the tool rechecks project identity, relative path containment,
and file availability, then delegates navigation to the workspace controller.
The tool never edits or writes project files.

## Recognition rules and bounds

The scanner recognizes `TODO` and `FIXME` case-insensitively as standalone
ASCII identifier tokens in C/C++ `//` and `/* ... */` comments. It skips
ordinary string and character literals and C++ raw strings. Line and block
comments, including block comments spanning lines, are handled by a bounded
lexical scanner; this is not a compiler parser and does not infer annotations
from other languages, generated metadata, or arbitrary prose files. A comment
excerpt is capped at 128 bytes and sanitized for display. Results sort by
case-insensitive relative path, then line, then annotation.

Only `.c`, `.h`, `.cc`, `.cpp`, `.cxx`, `.hh`, `.hpp`, and `.hxx` files are
included. The shared Project Search service excludes `.git`, `.vs`, `.idea`,
`out`, `build`, `bin`, `obj`, `dist`, `node_modules`, `.hg`, `.svn`, `.cache`,
`target`, `bazel-out`, `.guidexos`, `.developerstudio`, `.gxdeploy`, `.gxbuild`,
`developerstudio-tmp`, `guidexos-tmp`, and `cmake-build-*` directories. It does
not follow symlinks. Existing workspace filesystem access and file-size checks
remain in force.

The fixed-capacity task model retains at most 128 results on hosted builds and
32 on bare-metal builds. Reaching this cap stops the traversal and marks the
results truncated. The reused search service also bounds directory depth to 64,
directories to 4,096 hosted / 128 bare-metal, queued directories to 512 / 32,
files to 100,000 / 256, directory listings to 128 / 32 entries, each file to
256 KiB / 16 KiB, aggregate scanned bytes to 256 MiB, elapsed time to five
minutes, and directory entries processed per poll to 16. The lexer scans a
file in place without allocating a line buffer, so the maximum physical line
length is also bounded by the per-file limit; display excerpts remain capped
at 128 bytes. On bare metal the per-file and file-count caps limit the scan to
at most 4 MiB of file contents. Dirty editor snapshots are read from the
existing bounded snapshot buffer. A limit or cancellation can leave a useful
partial list; the UI preserves the corresponding terminal status. The task
model uses fixed arrays: at most about 114 KiB hosted or 12.4 KiB bare metal,
including the path, description, and per-task metadata in each retained result.

## Implementation and lifecycle

`developer_studio_task_list.{h,cpp}` owns the fixed-capacity result model and
comment lexer. The UI passes a visitor to the existing `ProjectSearchService`,
which provides bounded traversal, exclusions, file validation, timeout,
cancellation, and project-generation checks. The panel is a single active
Developer Tool; activating another registered tool closes it through the
registry lifecycle. Project-session reset clears its operation and results.
Opening a result uses `WorkspaceControllerOpenDocumentAtLocation`, preserving
the normal editor path and caret behavior.

Task List was added as the fifth registered Developer Tool. Apply Selected
File remains disabled in the TOOLS4 Replace Preview. TOOLS5's guarded-write
blocker is unchanged. This phase makes no change to the Server filesystem or
ABI, Build/Run, Compilation, or Debugging contracts.

## Validation and staged packages

- Clean hosted CMake configure/build: `build-tools6-hosted`.
- Full hosted CTest: 38/38 passed, including the Task List scanner and shared
  navigation, project search, Build/Run, and debugger regressions.
- Focused Task List, tool-registry, project-search, project-navigation,
  navigation, and Find regressions: 6/6 passed.
- `build.ps1` AMD64 and ARM64 package builds passed. They staged only inside
  `out/tools6-package/verified-multiarch/Apps/DeveloperStudio`, leaving the
  Server checkout and its production package untouched. Its `NativeElf`
  manifest declares both supported architectures and both referenced ELF
  files are present.
- Scanner regression coverage includes lexical comment/literal handling,
  token boundaries, case, line endings, bounded descriptions, supported file
  types, path rejection, sorting, capacity truncation through the shared scan,
  exclusions, binary files, rescan after content changes, cancellation,
  project-switch identity mismatch, editor line navigation, deleted files, and
  zero filesystem writes.

The audited package has App ID `com.guidexos.developerstudio`, kind `NativeElf`,
and two manifest entries. `readelf` confirmed each ELF is 64-bit, little-endian,
and has the expected machine:

| Target | Package file | Size (bytes) | ELF machine | SHA-256 |
| --- | --- | ---: | --- | --- |
| AMD64 | `bin/amd64/developerstudio.elf` | 1,215,492 | Advanced Micro Devices X86-64 | `2EBCF38A725D527C51C635A1EEFB1A66D5B187C91DF81CB719AB193995ABE843` |
| ARM64 | `bin/arm64/developerstudio.elf` | 1,394,172 | AArch64 | `18902052A1E97D5E8526AD9C3A4310177502AA58145188FD489F13DC0A170634` |

Hosted automated validation is complete. QAC and the configured hosted Server
UI were unavailable, and no guest VM was started. Live acceptance remains
pending.
