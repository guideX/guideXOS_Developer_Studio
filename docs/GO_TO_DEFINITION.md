# Lexical Go To Definition

Developer Studio's `F12` command provides bounded lexical C/C++ navigation. It
uses the existing `SymbolDatabase`; it is not a compiler frontend, language
server, or semantic name resolver.

## Architecture

```text
editor caret or selection
  -> bounded identifier extraction
  -> qualifier and lexical-scope capture
  -> existing project SymbolDatabase lookup
  -> deterministic candidate scoring
  -> direct activation or bounded picker
  -> NavigationHistory
```

`developer_studio_navigation.*` contains the UI-independent query, candidate,
resolution, and history models. The native shell only owns F12 input, picker
painting, activation, and editor focus. Candidates contain project-relative
paths, symbol locations, document identity, and text generations; they do not
contain editor pointers, filesystem handles, or mutable document pointers.

## Symbol identity and declaration roles

Canonical `DocumentSymbol` records retain the name, lexical container,
qualified name, kind, signature hint, document identity, text generation,
line, column, byte range, and declaration role. Qualified names use `::` and
are bounded to 512 bytes. Anonymous namespaces retain the existing lexical
name `anonymous` for Outline compatibility.

Roles are classified lexically as `Definition`, `Declaration`,
`ForwardDeclaration`, `Alias`, or `Unknown`:

- a type with a body is a definition; `class X;` is a forward declaration;
- a function with a body is a definition; a prototype is a declaration;
- `extern` namespace variables are declarations and other indexed namespace
  variables are definitions;
- `typedef` and `using Name = ...` are aliases.

Malformed declarations, function-pointer syntax, templates, macros, and
complex declarator relationships can remain unknown or be omitted. These
roles are ranking hints, not proof of C++ semantic equivalence.

## Identifier and context capture

An active single-line selection is used when it is one ASCII C/C++ identifier.
Otherwise the byte under or immediately before the caret is expanded to the
identifier token. Caret-at-start, middle, end, EOF-adjacent, underscore, and
digits-after-the-first-character are supported. `~Destructor` is retained as
one lexical name. Ordinary identifiers are limited to 1,024 bytes; invalid
selection, whitespace, punctuation-only positions, and an identifier beginning
with a digit are rejected. UTF-8 bytes outside the ASCII token are preserved
as surrounding text but are not interpreted as Unicode identifier characters.

The query captures an explicit qualifier before `::`, the containing
namespace/class/struct lexical scope, current project/document identity and
generation, relative path, and a weak symbol-kind hint. Expressions before `.`
and `->` are not type-resolved and are never treated as proof of an object's
class.

## Lookup and ranking

The database is searched without a full-project rescan. The active dirty
document is synchronously rescanned within the existing document bound before
lookup. Lookup prefers exact-case names; case-insensitive fallback is used only
when no exact-case name exists. Qualified identity is assembled from the
explicit qualifier or containing lexical scope.

The current score factors are:

```text
+1000 exact qualified name
+700  same containing scope
+500  definition
+300  exact case
+200  same document
+150  nearby namespace scope
+125  nearby class/scope hint
+100   matching lexical kind hint
+75   same leading path component
+50   declaration
+10   project-wide name match
-100  forward declaration when a definition exists
-200  stale candidate
```

Scores are sorted descending. Ties use role priority, qualified name,
project-relative path, line, column, byte offset, and symbol kind. No hash-map
or filesystem enumeration order participates. Duplicate records at the same
path/range/role are collapsed; overloads and distinct declarations remain
separate.

## Direct navigation and picker

One candidate navigates directly. Multiple candidates navigate directly only
when the top score exceeds the next score by at least 150 points and the
result is not truncated. A definition therefore beats matching declaration
copies, while equal or near-equal overloads remain ambiguous. A unique
declaration or alias can be activated directly with an explanatory status.
Already being on the unique definition does not add a history entry.

The picker retains at most 1,000 candidates and displays at most 100 with
visible-range rendering. It shows kind, qualified name, relative path,
line/column, declaration role, and a bounded signature. `Up`, `Down`, `Page
Up`, `Page Down`, `Home`, `End`, `Enter`, `Escape`, mouse wheel, and
double-click are supported. No fuzzy matching is performed.

No identifier reports `No symbol under caret.`; no match reports
`Definition not found.`; declaration-only results report that declarations are
being shown; and retained-result truncation reports a truncation warning.
Routine candidate activity is not written to Output.

## Activation and stale locations

Activation validates project identity, project-relative containment, the
candidate path, and the existing workspace navigation path. An already-open
dirty document remains in memory; opening a candidate never overwrites it.
The exact stored byte range is checked first. If it no longer matches, the
expected line is searched, followed by a bounded 16 KiB nearby search. A
nearby identifier is selected only when exactly one match exists. Otherwise
the caret is clamped to the stored line/column without selecting unrelated
text, and `Definition location may be stale.` is shown.

## Navigation history

`Alt+Left` moves back and `Alt+Right` moves forward. A new definition jump
pushes its origin, avoids an adjacent duplicate, clears forward history, and
retains at most 256 entries in each direction. Locations use project-relative
paths, project/document generations, byte offsets, selections, line/column,
and viewport top line. Closed documents are reopened through the workspace
controller; dirty open tabs remain untouched. A history location from another
project or project generation is rejected as stale.

Find/Replace query state, Find in Files state, Outline state, and Ctrl+T state
are separate from Go To Definition. The canonical symbol records and existing
source activation are shared, but query sessions are not.

## Stable markers and limits

Markers use the `GUIDEXOS_DEVELOPER_STUDIO_MARKER` prefix and include
`goto_definition_begin`, `goto_definition_lookup`,
`goto_definition_resolution`, `goto_definition_activate`, `navigation_back`,
and `navigation_forward`. Status codes include
`GOTO_DEFINITION_NO_IDENTIFIER`, `GOTO_DEFINITION_NO_CANDIDATES`,
`GOTO_DEFINITION_DECLARATIONS_ONLY`, `GOTO_DEFINITION_CANDIDATES_TRUNCATED`,
`GOTO_DEFINITION_PROJECT_STALE`, `GOTO_DEFINITION_PATH_OUTSIDE_PROJECT`,
`GOTO_DEFINITION_LOCATION_STALE`, and `GOTO_DEFINITION_NAVIGATION_FAILED`.

The active source file remains bounded by the existing 256 KiB editor limit;
the identifier is 1,024 bytes, qualifier 2,048 bytes, candidate retention
1,000, visible results 100, signatures 256 bytes, qualified-name display 512
bytes, nearby stale recovery 16 KiB, and each history direction 256 entries.

## Known limitations and future path

This phase does not implement overload or argument-type resolution, type
inference, inheritance, templates, macro expansion, include expansion,
system/external header indexing, cross-project references, rename, references,
peek definition, Clang, or LSP. `.` and `->` expressions are weak lexical
hints only. The future path is:

```text
lexical navigation
  -> include graph
  -> declaration/definition relationships
  -> lightweight type hints
  -> semantic index
  -> optional language service
```
