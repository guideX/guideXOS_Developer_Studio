# Include Graph

Include Graph is a project-local, lexical dependency view for C and C++ source and header files. It is intentionally separate from the symbol index and does not add a compiler front end, macro expansion, language-server client, or Server ABI slot.

## User workflow

With a project open, press `Ctrl+Shift+I` to open the graph for the active C/C++ document. The panel provides six deterministic views:

- Direct Includes
- Included By
- Transitive Includes
- Transitive Included By
- Cycles
- Unresolved

`Tab` or the left/right arrows switch views. `Up`, `Down`, `Page Up`, `Page Down`, `Home`, and `End` move through results. `Enter` opens the selected location; a double-click does the same. `R` starts a fresh build, `C` requests cancellation, and `Escape` closes the panel. When an include resolves to multiple case-folded project paths, activation opens a bounded candidate picker.

`F12` first checks whether the caret is on an include directive. A resolved include opens its target and pushes the current caret onto the existing navigation history. `Alt+Left` returns to the origin. Ambiguous includes use the candidate picker; missing, external, macro, invalid, and outside-project includes report their explicit status instead of guessing.

## Scan contract

The scanner accepts only line-start preprocessor directives after whitespace and a UTF-8 BOM. It recognizes `#include`, `# include`, quoted includes, angled includes, and bounded backslash continuations. It ignores comments, strings, character literals, and bounded C++ raw strings. It records the directive byte offset, include-text offset, line, column, delimiter kind, source document identity/generation, and conditional state.

Conditional handling is lexical and conservative:

- `#if 0` includes are retained as `InactiveIfZero` edges but are excluded from traversal and cycles;
- `#if 1` includes are `Active`;
- `#if` expressions other than the exact `0`/`1` form, `#ifdef`, and `#ifndef` are `ConditionalUnknown`;
- unknown conditional edges are included in the default cycle view and can be excluded from transitive traversal by the model API; and
- malformed directives and macro includes are retained as diagnostics rather than expanded.

## Resolution contract

Resolution never leaves the selected project root. A quoted include searches the source directory, configured include roots, then the project root. An angled include searches configured include roots, then the project root. Candidate paths are slash-normalized and reject absolute forms and traversal outside the root. Exact spelling wins; a single case-folded match resolves with a `INCLUDE_GRAPH_CASE_MISMATCH` status; multiple matches are `Ambiguous` and retain a bounded sorted candidate list.

Resolution states are `Resolved`, `Missing`, `ExternalUnresolved`, `Ambiguous`, `OutsideProject`, `UnsupportedMacro`, `InvalidPath`, and `Unreadable`. These states are visible in the Unresolved view and in the status text. Project files are the only resolution candidates; system and SDK headers remain explicit external/unresolved results unless they are present in the project-local include roots.

## Lifecycle and dirty documents

The model exposes an asynchronous bounded operation with the phases `Enumerating`, `Scanning`, `Resolving`, `BuildingReverseEdges`, and `DetectingCycles`. Each poll consumes a small work budget. The operation records an ID, project identity/generation, progress counters, cancellation state, truncation, timeout, and terminal error. The UI polls it from the event loop at a short interval while active, so the shell remains responsive. Cancellation preserves the last completed graph.

At build start, open dirty documents are copied into bounded snapshots and used in place of disk reads. Editing the active document while the panel is open performs a generation-bound incremental rescan of that document, then rebuilds resolution, reverse edges, and cycles without rescanning unrelated files. A project/workspace change cancels the active operation and invalidates the completed graph through the project generation check.

The model keeps two graph generations so a new build cannot corrupt the last completed result. Current production bounds are 1,024 eligible files/nodes, 4,096 edges, 4,096 directories, 256 KiB per scanned file, 256 MiB total scanned bytes, 4,096 directives per file, 64 continuation lines, 64 include-conditional nesting levels, 256 ambiguous candidates per edge, a five-minute operation timeout, and bounded traversal/cycle storage. A graph can complete with `truncated=true`; the UI marks that state rather than presenting it as complete coverage.

## Diagnostics and tests

Build markers include:

```text
include_graph_begin=PASS|FAIL
include_graph_enumeration=PASS files=<n>
include_graph_scan=PASS files=<n> directives=<n>
include_graph_resolution=PASS resolved=<n> unresolved=<n>
include_graph_reverse_edges=PASS
include_graph_cycles=PASS
include_graph_incremental_update=PASS
include_graph_cancelled=PASS
include_graph_activation=PASS|FAIL
include_graph_picker_activation=PASS
```

The model test covers comment/string/raw-string exclusion, quoted/angled parsing, macro and conditional states, project-root resolution, reverse edges, traversal, cycles, asynchronous enumeration/scanning/resolution, and deterministic bounded storage. The native build compiles the same model into the packaged `developerstudio.elf`; no Server ABI change is required.
