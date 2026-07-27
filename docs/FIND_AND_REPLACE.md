# Bounded Find and Replace

Find and Replace is an active-document editor service. It is literal, bounded,
generation-safe, and independent of the Native UI. It does not implement regex,
Find in Files, semantic rename, symbol search, language-server services, or
IntelliSense.

## Model and lifecycle

`developer_studio_find.*` owns `FindOptions`, `FindMatch`, `FindDocumentState`,
and `FindSession`. The session stores the query, replacement, options, retained
sorted matches, current index, wrap status, truncation state, and error code.
It identifies a document with its monotonically assigned `documentId` and the
editor's mutation `generation`; it never stores a `Document*` or `TextBuffer*`.

Each `Document` retains its last query, replacement, and options. Opening a Find
bar rebinds the session to the active document. Switching documents saves the
old state and loads the new document's state. Closing a document or workspace
cannot leave a raw search pointer behind. An external reload or any generation
change recomputes the retained list and clears the current match so an old byte
offset cannot replace an unrelated occurrence.

All offsets are UTF-8 byte offsets, matching the editor buffer and syntax cache.
Retained ranges are sorted, non-overlapping, and use literal non-overlapping
matching. For example, `aa` in `aaaa` produces `[0,2]` and `[2,2]`.

## Limits and fallback

| Item | Limit |
| --- | ---: |
| query | 1,024 bytes |
| replacement | 16,384 bytes |
| searchable document | 8 MiB |
| retained matches | 20,000 |
| Replace All operations | 20,000 retained matches |
| visible overlay indices | 256 per visible line |

An empty query produces no matches. Oversized queries, replacements, or
documents produce a stable error and no replacement. When the retained match
limit is reached, the session sets `truncated` and the UI displays exactly:
`Search results truncated.` Replace All is disabled and its model operation is
rejected while results are truncated; it never claims that every occurrence was
replaced.

## Matching rules

Matching is literal and byte-oriented. With Match case enabled, bytes must
match exactly. With Match case disabled, only ASCII A-Z is folded; non-ASCII
bytes remain exact and are never interpreted as Unicode characters.

With Match whole word enabled, a match is rejected when the adjacent byte is an
ASCII word byte: `A-Z`, `a-z`, `0-9`, or `_`. Thus `cat` does not match
`catalog`, `my_cat`, or `cat2`, but does match `cat()`, `cat-cat`, and the
standalone first word in ordinary punctuation-separated text.

## Navigation

Find Next selects the next retained match in ascending offset order. Find
Previous selects the previous match. With Wrap around enabled, moving forward
from the last match selects the first and reports `Wrapped to beginning`; moving
backward from the first selects the last and reports `Wrapped to end`. With
wrapping disabled, navigation stops at the boundary without moving the caret.
When the query has no matches, the status is `No matches` and the caret is not
moved. Current navigation selects the editor range and scrolls it into view.

When Ctrl+F opens the bar, a non-empty, single-line selection within the query
limit initializes the query. Otherwise the active document's previous query is
reused. The first match at or after the caret is selected; if needed, wrapping
selects the first match.

## UI and shortcuts

Ctrl+F opens the compact Find bar:

```text
Find: [query]   Previous  Next  [ ]Case  [ ]Word  [x]Wrap  Close   current / total
```

Ctrl+H opens the same bar with the replacement row and `Replace` and
`Replace All` actions. The query and replacement fields accept ordinary editor
key input; Tab switches fields, Enter runs the forward action, and Shift+Enter
runs the backward action while a search field is focused.

| Shortcut | Action |
| --- | --- |
| Ctrl+F | Open Find |
| Ctrl+H | Open Replace |
| F3 | Find Next |
| Shift+F3 | Find Previous |
| Enter | Find Next in a search field |
| Shift+Enter | Find Previous in a search field |
| Escape | Close the bar and return focus to the editor |

Ctrl+S, Ctrl+Shift+B, F5, Problems navigation, tab switching, and ordinary
editor navigation remain routed through their existing paths.

## Replace Current

Replace Current validates the session's document ID and generation, validates
the retained range, and verifies that the current document bytes still match
the query and whole-word/case options. If the session is stale, results are
recomputed and the current match is cleared; no unrelated range is replaced.

The replacement may be longer, shorter, empty, multiline, identical to the
query, or contain the query itself. The editor range primitive updates the
NUL-terminated buffer, caret/selection, generation, dirty state, and mutation
metadata. `DocumentUpdateSyntax` then updates the syntax cache from the earliest
affected line. Matches are recomputed and the next deterministic match is
selected.

## Replace All

Replace All uses the stable retained snapshot and applies it from the end of the
document toward the beginning. Newly inserted text is never searched during the
same operation, so a replacement containing the query is not recursively
replaced. It reports a deterministic retained-match count, updates line
indexing, marks the document dirty, and forces the syntax cache's safe full
rebuild path because several disjoint edits cannot be described by one
incremental edit record. No new undo transaction system is introduced in this
phase.

## Match overlays and syntax integration

Retained matches are an overlay over the syntax renderer, not syntax-token
data. The current match uses a warm highlight and other retained matches use a
cool highlight. Only matches overlapping a visible line are looked up, using a
binary-search entry point over the sorted retained ranges; off-screen matches
are not scanned for every rendered line. Overlay text is redrawn after its
background so it remains readable over keywords, comments, strings, and the
current editor selection.

The editor primitives are reusable and UI-independent:

```text
GetSelectedText
GetCaretOffset
SetCaretOffset
SelectTextRange
ValidateTextRange
ReplaceTextRange
ReplaceTextRanges
OffsetToLineColumn
LineColumnToOffset
```

They use the same byte-offset convention as syntax highlighting and diagnostic
navigation. Save, Build, Problems navigation, Run, and document close gates
continue to use the existing dirty flag and workspace controller.

## Unicode and future work

The current search service is safe for arbitrary non-NUL bytes but does not
perform Unicode case folding, grapheme segmentation, locale-aware word
classification, normalization, or regex matching. A future Find in Files
feature should build a separate workspace-scoped service with explicit file
enumeration, cancellation, result limits, and per-file generation checks; it
should not expand this active-document session's scope.
