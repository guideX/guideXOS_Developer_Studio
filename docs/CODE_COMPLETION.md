# Lightweight Code Completion

Developer Studio provides a bounded, lexical C/C++ completion session. It is
opened explicitly with `Ctrl+Space` in the source editor. The feature is a
predictable editor aid; it is not IntelliSense, a compiler front end, a type
checker, or a language-server client.

## Data flow

```text
active document
  -> syntax-cache token spans and lexical context
  -> CompletionContext
  -> shared keyword list, SymbolDatabase, document-word cache
  -> bounded CompletionSession
  -> popup rows and guarded text replacement
```

`developer_studio_completion.*` is UI-independent. A session stores copied
text and stable IDs/generations rather than editor pointers, document pointers,
filesystem handles, or UI objects. `main.cpp` owns only the popup, keyboard and
mouse routing, status text, markers, and the one-operation completion undo
snapshot.

## Context extraction

The extractor scans a bounded window around the caret and records:

- the identifier prefix immediately before the caret;
- an explicit `::` qualifier, including nested qualifiers;
- a lexical containing scope inferred from nearby braces and indexed namespace
  or class declarations;
- a replacement range covering the valid identifier selection or the active
  identifier prefix; and
- whether the expression is in a member-like `.` or `->` context.

Comments, character literals, string literals, raw strings, preprocessor lines,
and `#if 0` regions reject completion. A valid identifier selection is replaced
as a whole; a non-identifier selection is treated as an empty-prefix request at
the caret. Qualifiers and prefixes are copied into fixed-size buffers, and an
overlong request fails explicitly instead of growing storage.

Member completion is lexical. The session can use the expression's visible
lexical candidates, but it does not infer the receiver type. When a member
request is unresolved, the popup remains useful as a lexical fallback and marks
the candidate set as ambiguous in its detail text.

## Candidate sources

Candidates are collected in deterministic passes:

1. the shared C/C++ keyword and fundamental-type list used by syntax
   highlighting;
2. symbols in the inferred current lexical scope;
3. symbols from the active document;
4. project symbols from the existing `SymbolDatabase`; and
5. distinct identifier words from the active document's syntax-cache spans.

Document words exclude keywords, comments, strings, character literals,
preprocessor text, and inactive `#if 0` branches. The cache is rebuilt after
an edit and is generation-bound to the document. The project symbol index is
consumed read-only; completion does not add a second project index or a Server
ABI call.

Destructors are not inserted as ordinary names. Constructor/class duplicates are
collapsed by insertion text. Callable overloads are also collapsed by
insertion text, with a bounded overload count retained for display; a
signature is never inserted accidentally.

## Matching and ordering

The matcher prefers, in order, exact matches, case-sensitive prefixes,
case-insensitive prefixes, case-sensitive substrings, and case-insensitive
substrings. Scores then account for source (current scope, document, project,
document word, or keyword), symbol kind, lexical ambiguity, and overload
information. Ties are broken by source priority, kind, insertion text,
qualified name, signature, path, source line/column, and stable candidate ID.

The result is therefore stable for the same document bytes, project index
generation, caret context, and session inputs. Duplicate insertion text is
removed before the retained list is sorted.

## Popup and commands

The popup is anchored near the caret and clamped to the editor viewport. It
shows a bounded set of rows with the selected row highlighted, a compact kind
label, qualified name/signature where available, and detail/status text for
ambiguous or truncated results. It is dismissed when focus leaves the source
editor, the document/project/workspace changes, or the user makes an edit that
cannot be incorporated into the current session.

| Key or action | Behavior |
|---|---|
| `Ctrl+Space` | Open or refresh completion manually |
| `Up` / `Down` | Move selection |
| `Page Up` / `Page Down` | Move by one bounded page |
| `Home` / `End` | Select first/last candidate |
| `Enter` / `Tab` | Accept the selected candidate |
| `Escape` | Dismiss the popup |
| click a row | Select it |
| double-click a row | Accept it |
| wheel over popup | Scroll the visible candidate range |

Typing and backspace update the editor through the normal text-buffer path and
refresh the active session. Left/right movement, delete, and other navigation
actions dismiss it. The popup is not opened automatically by ordinary typing in
this phase.

## Acceptance, dirty state, and staleness

Acceptance validates the project ID, project generation, document ID,
document generation, caret, selection, replacement bounds, and expected source
text captured by the session. Only that replacement range is changed. The
existing syntax-cache incremental update, symbol refresh, document-word cache,
dirty tracking, and caret preservation paths are reused.

If any generation or source-text check fails, no edit is made and the session
reports a stale or mismatch error. The popup never applies a candidate against
a different document or silently overwrites newer text.

Completion acceptance stores one bounded pre-acceptance text snapshot. `Ctrl+Z`
first restores that exact snapshot when available, then runs the normal syntax,
symbol, word-cache, dirty, and caret updates. This is the completion-specific
undo path; it does not claim general multi-level undo or redo.

## Bounds and diagnostics

The package uses fixed storage for a 1,024-byte prefix/insertion, 2,048-byte
qualifier, 2,048-byte scope, 8 KiB context scan, 5,000 collected candidates,
1,000 contextual candidates, 1,000 retained candidates, 100 visible candidates,
8 MiB of document-word scanning, and 20,000 document words. Display, signature,
and detail strings have separate limits. The document-word pass maintains a
bounded 64-level conditional stack and scans each source line once.

The status bar reports concise states such as no results, truncated results,
an expired session, or unavailable lexical contexts. Stable markers include:

```text
completion_begin=PASS|FAIL
completion_context=PASS|FAIL
completion_candidates=PASS|TRUNCATED|FAIL
completion_accept=PASS|STALE|FAIL
completion_undo=PASS
completion_dismiss=PASS
```

Marker values contain bounded reason or count fields where useful; source text
and full paths are not emitted by the completion model.

## Known limitations and future work

This phase intentionally does not perform type inference, include expansion,
macro expansion, template instantiation, overload resolution, conversion
ranking, semantic member lookup, completion snippets, automatic invocation,
signature help, or a language-server protocol. It does not add a background
indexer or server-side completion service. Future work can replace the lexical
candidate provider with a semantic provider behind the same bounded session,
staleness, popup, acceptance, and marker contracts.

