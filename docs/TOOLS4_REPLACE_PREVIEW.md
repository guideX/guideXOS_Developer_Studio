# TOOLS4 — Replace in Files Preview

## Scope

TOOLS4 adds a project-wide, literal replacement preview to the Tools menu and
`Ctrl+Shift+H`. It is preview-only. There is no Apply or Replace All control,
and the preview path has no filesystem write, create-directory, or remove-path
callback. Opening the tool repeatedly keeps the current panel and inputs;
`Esc` cancels an active scan and closes an idle panel. The panel's Cancel/Clear
control cancels scanning or clears the retained plan.

The inputs accept an empty replacement to preview deletion. An empty Find value
is rejected. Case-insensitive matching uses the existing ASCII byte-folding
literal matcher; regular expressions, Unicode normalization, and
language-aware transformations are not performed.

## Reused architecture

The preview reuses `ProjectSearchService` for bounded project traversal, file
reads, dirty-document snapshots, binary detection, path containment, and
cancellation. Its scan visitor receives one file's bytes at a time and builds a
fixed-capacity `ReplacePreviewPlan`; it does not duplicate directory scanning
or retain whole source files. Explicit search includes use the same
`IsSupportedTextPath` allowlist as the TOOLS3 editor/navigation path. The
project source inventory used by Build and Run remains separate.

The supported filename allowlist is `.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hh`,
`.hpp`, `.hxx`, `.txt`, `.md`, `.json`, `.xml`, `.css`, `.html`, `.htm`, `.js`,
`.ts`, `.ps1`, `.cmd`, `.bat`, `.cmake`, `.ini`, `.cfg`, `.conf`, `.config`,
`.properties`, `.toml`, `.log`, `.mk`, `.yml`, and `.yaml`. The exact supported
names `guidexos.project`, `.env`, and `.editorconfig` are also included. A file
with a supported name is still skipped if its bytes are oversized, unreadable,
or binary-looking.

The scan is rooted at the active project root. The shared scanner rejects
traversal entries and paths outside that root. It excludes `.git`, `.hg`,
`.svn`, `.vs`, `.idea`, `.cache`, `build`, `out`, `bin`, `obj`, `dist`,
`target`, `node_modules`, `bazel-out`, `cmake-build-*`, and the existing
guideXOS/Developer Studio generated directories.

Dirty editor documents are searched from captured document bytes. Disk-backed
matches retain project-relative paths, source sizes, and content fingerprints.
Before a complete plan is shown, each affected file is checked against its
captured source. While the panel remains open, affected files are rechecked in
bounded steps. A project switch, input/case change, workspace refresh, affected
file removal/change, or a changed/disappeared file during scanning invalidates
the plan. Unreadable or oversized supported text files and scan/capacity limits
leave it incomplete. Binary-looking files are excluded by the shared scanner
and shown in the skip count.

The file metadata API provides kind and size but no modification timestamp, so
same-size content changes are checked with a 64-bit FNV-1a fingerprint. This is
a change detector, not a cryptographic integrity proof. Newly added files are
noticed after workspace refresh and a new preview; TOOLS4 does not retain an
unbounded inventory of every nonmatching file. A future Apply operation must
perform its own immediate full revalidation.

## Preview semantics

Matches are literal and non-overlapping. The scanner chooses the leftmost
available occurrence, advances by the Find length, and retains adjacent
occurrences. For example, searching `aa` in `aaaaa` previews byte offsets 0
and 2; the overlapping candidate at offset 1 is not selected. Results are
ordered by project-relative path (ASCII case-insensitive, then exact-case
tie-break) and byte offset. Distinct paths that differ only by case remain
distinct.

Each row shows path, one-based line and byte-column, byte offset, and Before
and After excerpts. Before displays the original line context. After shows the
proposed context for this one occurrence; other occurrences on that line stay
unchanged in that row. It is not a complete post-replacement file preview.
Each excerpt retains at most 96 printable bytes, escapes control characters,
and marks left/right truncation with ellipses. A truncated excerpt never claims
to show the full replacement result. File bytes, line endings, indentation,
and encoding are read and compared as-is; planning never normalizes or writes
them.

The panel shows retained match rows, affected-file count, and scan skips. It
labels the match count `at least` when match capacity is exceeded; if affected-
file capacity is exceeded, the listed file count is the retained lower bound
and the plan is explicitly incomplete. The panel displays up to 8 rows at once
and allows scrolling through retained rows. A complete zero-match scan reports
no matches; a limited or skipped-required-file scan is explicitly incomplete.

## Fixed bounds

| Resource | Hosted preview | Native ELF preview |
|---|---:|---:|
| Find text | 128 bytes | 128 bytes |
| Replacement text | 256 bytes | 256 bytes |
| Affected files retained | 32 | 8 |
| Match rows retained | 128 | 64 |
| Before/After excerpt | 96 bytes each | 96 bytes each |
| Rows displayed at once | 8 | 8 |
| Path buffer | 768 bytes | 256 bytes |
| File read limit | 256 KiB | 16 KiB |
| Total project bytes scanned | 256 MiB | 256 MiB |
| Files enumerated | 100,000 | 256 |
| Directories visited | 4,096 | 128 |
| Traversal depth | 64 | 64 |
| Entries processed per poll | 16 | 16 |
| Scan timeout | 5 minutes | 5 minutes |

The plan is fixed storage: 56,960 bytes with the hosted bounds and 17,856 bytes
with the Native ELF bounds on the current C++11 toolchain layout. The 128
hosted rows' two excerpt buffers account for 24,832 bytes of that total. The
existing search service separately owns its bounded read buffer, traversal
queue, and dirty-document snapshots.

## Stale-result safety and future writes

Input text and case changes cancel any active scan and mark its plan stale.
Project identity and generation are checked while the panel is active; the
workspace refresh action invalidates the plan. Each affected dirty document is
revalidated by document ID, generation, path, and bytes. Each affected disk
file is re-statted, reread within the file-size cap, and fingerprinted. A
missing, resized, or changed affected file makes the plan stale. A file removed
or changed during the initial scan is stale; unreadable/oversized supported
files prevent a complete plan. Capacity limits never produce an applicable
plan.

`WorkspaceControllerSaveDocument` writes a whole document through the
single-file `WorkspaceFileSystem.write` callback. That API does not promise
atomic replacement, group commit, rollback, or recovery across multiple files.
The existing F2 rename workflow has operation-specific preview/revalidation
and one-operation undo behavior; it is not a reusable multi-file transaction.
The editor has no general durable multi-file undo log.

A later Apply phase must at minimum:

1. Recheck project identity, inventory state, every document generation, and
   every affected file's current bytes immediately before editing.
2. Reject any changed, missing, unreadable, oversized, or newly dirty input and
   rebuild the preview from the new contents.
3. Preserve original byte sequences, encoding, line endings, and file metadata
   unless the user explicitly chooses a conversion.
4. Define a per-file atomic temp-write/replace strategy where the filesystem
   supports it, and never describe a multi-file set as atomic without a
   filesystem transaction guarantee.
5. Keep bounded before-images or a durable recovery journal, report exactly
   which files committed if a later write fails, and provide recovery/undo for
   partial completion.
6. Verify written contents and metadata after each commit, and retain backups
   until the recovery decision is resolved.

## Hosted verification and staged packages

The clean Debug CMake build completed 108/108 build steps. CTest passed 37/37
suites, including the replacement preview, existing project-search, Tools,
Build/Run inventory, editor, and debugger regression suites. Debug is used for
CTest because this repository's tests execute some setup inside `assert`;
Release defines `NDEBUG` and is not a valid run mode for those tests.

Both Native ELF packages were built with `build.ps1` using the Server SDK and
the existing fixed-address, sectionless package recipe. `-StagingRoot` directed
the package trees under `out/tools4-packages/<architecture>` and `-BuildRoot`
kept compiler objects and temporary test executables under
`out/tools4-build/<architecture>`. The Server checkout was read-only during
these builds; no production Server ELF was replaced.

| Architecture | ELF | SHA256 | Size |
|---|---|---|---:|
| AMD64 | `out/tools4-packages/amd64/Apps/DeveloperStudio/bin/amd64/developerstudio.elf` | `7843A99D83A5C4BFB8A154F63DD6A011CBE334CFF91B18B36C85522AD93A1B53` | 1,201,460 bytes |
| ARM64 | `out/tools4-packages/arm64/Apps/DeveloperStudio/bin/arm64/developerstudio.elf` | `70E1E3AF61F59C971D245FB5C0F1B0D482C9E01D2CBD21546D562D59428EE0B4` | 1,376,644 bytes |

Each tree contains `Apps/DeveloperStudio/app.json` and its architecture-specific
ELF. `readelf -h` identified both as ELF64 `ET_EXEC`, with AMD64
`Advanced Micro Devices X86-64` and ARM64 `AArch64` machines. Manifest SHA256:
`47A9C855AE32A372CC7674350A9E7255BB58927FF943798CC929F6EDFCBCF1BE` in both
staged trees. The program headers begin at the required `0x50000000` image base;
`readelf -S` reports no section table in either file.

Live hosted-window UI qualification was not run. QAC guest acceptance remains
pending; no QEMU session was launched.
