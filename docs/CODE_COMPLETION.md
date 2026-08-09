# Code Completion and Type-Aware Member Completion

Developer Studio provides a bounded C/C++ completion session. Generic
completion remains a predictable lexical editor aid opened with `Ctrl+Space`;
the same session also provides Type-Aware Member Completion for truthful,
bounded direct members after `.` and `->`. This is not full IntelliSense, a
compiler front end, a type checker, or a language-server client.

## Data flow

```text
active document
  -> syntax-cache token spans and lexical context
  -> CompletionContext
  -> shared keyword list, SymbolDatabase, document-word cache, or
     Lightweight Type Intelligence direct-member index
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

For `identifier.` and `identifier->`, a bounded receiver extractor retains one
identifier and asks Lightweight Type Intelligence for its current type. The
operator is checked against pointer depth, then the owner-type bucket supplies
direct fields and methods. Unknown, ambiguous, stale, wrong-operator,
unsupported-pointer-depth, chained, and arbitrary-expression receivers do not
receive unrelated project-wide symbols; the popup may be empty with a modest
status reason.

Supported first-pass forms include local objects, pointer locals, parameters,
lvalue references, aliases and pointer aliases, and conservative `auto` values
whose initializer is a unique known function return. `const` receivers may
list known members; complete const-callability filtering is not modeled. Only
direct members are listed. Inheritance, access-control filtering, templates,
overload resolution, conversions, `Type::` static access, and arbitrary
chained expressions remain deferred. Access metadata is not guessed, so all
known direct members may be shown.

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
ABI call. For a proven member context, only the type-aware direct-member pass
is used. Member candidates are self-contained copies of the bounded name,
owner, kind, type/signature, declaration location, and project/document
generations. The member index is built during Type Intelligence indexing, so a
`.` lookup uses an owner bucket rather than scanning all declarations.

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
actions dismiss it. Typing `.` after a resolvable receiver or completing `->`
opens the same popup automatically; typing the member prefix refreshes it.
`Ctrl+Space` remains the manual trigger and no new shortcut is assigned.

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

The package uses fixed storage for a 1,024-byte prefix/insertion, 256-byte
single receiver, 192-byte owner type, 2,048-byte qualifier, 2,048-byte scope,
8 KiB context scan, 512 indexed member references scanned per request, 256
member candidates, 5,000 generic collected candidates, 1,000 contextual
candidates, 1,000 retained candidates, 100 visible candidates, 8 MiB of
document-word scanning, and 20,000 document words. Display, signature, and
detail strings have separate limits. The document-word pass maintains a
bounded 64-level conditional stack and scans each source line once. Owner
buckets are capped at 512 and member data is generation-bound; index
truncation is retained and reported as truncated.

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

This phase intentionally does not perform include expansion, macro expansion,
template instantiation, overload resolution, conversion ranking, inheritance
lookup, access-control filtering, static `Type::` completion, arbitrary
expression evaluation, completion snippets, or a language-server protocol. It
does not add a background indexer or server-side completion service. Signature
Help remains responsible for callable signatures after an inserted method and
Quick Type Info continues to use the shared Type Intelligence path. Future
work can extend the bounded provider behind the same session, staleness,
popup, acceptance, and marker contracts.
