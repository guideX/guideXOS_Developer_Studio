# Find in Files

Find in Files is a read-only, project-scoped search service. It is separate
from active-document Find/Replace and does not implement Replace in Files,
regular expressions, semantic search, symbol indexing, or arbitrary-folder
search.

## Architecture and trust boundary

```text
Ctrl+Shift+F
  -> bounded query/options draft
  -> ProjectSearchRequest snapshot
  -> incremental ProjectSearchService polling
  -> grouped ProjectSearchFileGroup/ProjectSearchMatch records
  -> validated document activation and byte-range selection
```

`developer_studio_project_search.*` contains no UI pointers and retains no raw
project, workspace, document, or filesystem handles. It copies project
identity/generation, options, and dirty-document text snapshots at start. It
uses the existing hosted `stat`, `list`, and full-file `read` callbacks, so no
Server ABI was required.

The hosted filesystem exposes synchronous full-file reads capped at 256 KiB
and bounded directory listing with a truncation flag. The service polls one
directory batch or one file scan at a time. While active, the UI event wait is
shortened so input, painting, Build, and Run remain serviceable between
checkpoints.

Only the active project root is searched. Every enumerated path is normalized,
checked against the normalized root, and converted to a normalized
project-relative path before retention. Traversal, absolute/drive-relative,
malformed, and outside-root paths are rejected. The search panel cannot supply
an arbitrary host root.

The current hosted ABI does not expose symlink or junction metadata. The
Server rejects symlink components, and the Studio accepts only regular files
and directories returned by the ABI. Unknown types are skipped, so symlink
entries are not followed and directory symlinks are never traversed.

Default excluded directory components are matched exactly,
case-insensitively:

```text
.git .vs .idea out build bin obj dist node_modules
.guidexos .developerstudio .gxdeploy .gxbuild
developerstudio-tmp guidexos-tmp
```

Names containing `build` are not excluded unless the complete component is
`build`; `rebuild_source` remains eligible.

## Scope, patterns, and matching

The default scope is C/C++ source and header files: `.c`, `.h`, `.cc`, `.cpp`,
`.cxx`, `.hh`, `.hpp`, and `.hxx`. A non-empty include pattern additionally
allows the tightly scoped project text extensions `.gxproj`, `.json`, `.md`,
`.txt`, and `.cmake`. Every candidate still passes size and binary checks.

Include and exclude patterns apply to the complete normalized relative path.
The narrow grammar supports literal path characters, `/`, `*`, and `?`, with
comma or semicolon separators. `*` matches zero or more path characters,
including separators; `?` matches one non-separator character. Patterns are
anchored to the complete path and use deterministic ASCII-insensitive matching
independent of host filesystem case rules. Empty include selects default C/C++
scope; empty exclude adds no pattern beyond default directory exclusions.

Examples:

```text
*.cpp
*.h
src/*
src/*.cpp
src/*.cpp;include/*.h
third_party/*;generated/*
```

Each pattern field is capped at 2,048 bytes, has at most 64 patterns, and each
pattern is at most 256 bytes. Empty patterns, traversal, absolute or
drive-relative patterns, controls, and unsupported separators fail safely with
stable error codes. No regex or unrestricted glob semantics are implemented.

The query is a literal byte sequence and reuses active-document Find's shared
matcher: ASCII-only case folding, optional match case, optional whole-word,
and non-overlapping source-order matches. Whole-word bytes are `A-Z`, `a-z`,
`0-9`, and `_`. This is not complete Unicode case folding. Line and column are
1-based UTF-8 byte positions, consistent with editor and Problems navigation.
Malformed UTF-8 is tolerated as bytes; embedded NUL classifies a file as
binary and skips it.

## Bounds and previews

| Resource | Limit |
|---|---:|
| Query | 1,024 bytes |
| Directories visited | 4,096 |
| Pending directories | 512 |
| Directory depth | 64 |
| Eligible files enumerated | 100,000 |
| Searchable file | 256 KiB |
| Total bytes scanned | 256 MiB |
| Result files | 256 |
| Total matches | 4,096 |
| Matches per file | 512 |
| Preview | 512 bytes |
| Operation duration | 5 minutes |

Files over the limit, unreadable files, unsupported files, and binary files
are skipped without one UI row per skip. Directory-list truncation and
directory/file/result/match/byte/time limits set `truncated` and retain only
bounded partial results. One bounded notice is exposed:
`Search results truncated.`

Each match stores a byte offset, line, column, length, and containing-line
preview. Tabs and control bytes become spaces; CRLF's CR is omitted. Long lines
are clipped around the match to 512 bytes with left/right clipping flags. A
query spanning a newline is selected as a range; its preview span is limited
to the containing line.

## Lifecycle, dirty documents, and staleness

The operation states are `Idle`, `Enumerating`, `Searching`, `Cancelling`,
`Completed`, `Failed`, and `Cancelled`. Operation IDs are monotonic. Only one
operation is retained. A new start supersedes the prior operation safely;
stale IDs cannot poll or release the replacement. Release is terminal-only and
clears result storage.

Escape while idle closes the panel. Escape during search requests cancellation
first; partial groups remain visible with a cancelled status. Cancellation is
non-blocking and reaches exactly one terminal state at the next checkpoint.

`WorkspaceModelSetRoot` assigns a generation, and project metadata reload
advances it. A project switch, close, or reload cancels an active operation.
Results retain the generation and hashed project identity; activation compares
the exact project ID and revalidates containment.

If a project document is open and dirty, its copied immutable editor snapshot
overrides disk contents. Clean/open documents use disk contents. A snapshot
retains only relative path, bounded bytes, document ID, and text generation.
Activation selects the already-open dirty document. If content changed after
search, activation validates the query at the stored byte offset, tries a
bounded nearby match on the stored line, and otherwise clamps to the stored
line/column with `Search result may be stale`.

## Results, UI, and Output

`Ctrl+Shift+F` opens the dedicated panel. Query initialization is selected
single-line text, then the previous project-search query, then active-document
Find query, then empty. Include/exclude fields start with the C/C++ patterns
and common output-directory patterns. The panel provides Match case, Whole
word, Search, Cancel, progress, empty/cancelled/failed/truncated states,
bounded scrolling, keyboard selection, mouse selection, double-click, and
Enter activation.

Groups sort by normalized relative path using case-insensitive ordinal order
with original-byte tie-breaking. Matches within groups remain in source order.
The panel is not a Problems projection. Output Service receives only lifecycle
or exceptional messages, never one record per match.

Bounded markers are:

```text
project_search_begin=PASS
project_search_cancel=REQUESTED
project_search_enumeration=PASS
project_search_file_scan=PASS
project_search_complete=PASS
project_search_complete=TRUNCATED reason=<code>
project_search_complete=CANCELLED
project_search_result_activate=PASS|STALE|FAIL reason=<code>
```

Active-document Ctrl+F/Ctrl+H remains independent: its query, current match,
overlays, replacement state, and lifecycle are not replaced by project search.
Activation changes only the editor selection and viewport.

## Known limitations

The service follows the current full-file hosted read boundary, so it does not
claim chunked overlap scanning for files larger than 256 KiB. The ABI exposes
no timestamps, content fingerprints, or symlink metadata to the guest;
staleness uses project generation, dirty-document generation, current size
where available, and bounded text revalidation. The UI is a compact phase-local
panel, not a general tree widget. Replace in Files remains a separate future
operation.
