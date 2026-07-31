# Document Outline and Project Symbol Index

Developer Studio provides a bounded lexical C/C++ symbol index. It is a
source-structure aid, not a compiler front end: it does not build an AST,
evaluate types, expand macros, resolve templates, or provide language-server
services.

## Architecture

```text
Document bytes
    -> comment/string/preprocessor-aware lexical tokenizer
    -> DocumentSymbol records
    -> path/identity/generation-aware SymbolDatabase
    -> Outline panel or Ctrl+T result list
    -> existing source-location navigation
```

`developer_studio_symbols.*` is UI-independent. The scanner retains byte
offsets, 1-based UTF-8 byte line/column positions, stable document IDs, and
text generations. It has no editor pointers or filesystem handles. The
database receives caller-owned fixed arrays so unit tests can use small
capacities while the Native ELF package uses the full configured bounds.

The database stores documents in normalized path order and symbols in source
order within each document. Project enumeration sorts directories and files
deterministically. Lookup by file, kind, exact name, prefix, and bounded
substring is exposed as model APIs. Each canonical symbol also retains a
bounded qualified name, signature hint, and lexical declaration role
(`Definition`, `Declaration`, `ForwardDeclaration`, `Alias`, or `Unknown`).

## Recognized symbols

The scanner recognizes namespaces, including anonymous and nested namespaces;
classes, structs, enums, unions, and forward declarations; free functions,
methods, constructors, and destructors; namespace-scope global and static
variables; typedef names; and `using Name = Type` aliases. Template-declared
types/functions are recognized when their ordinary declaration tokens are
visible.

Comments, string/character literals, and preprocessor directive lines are
not tokenized as declarations. `#if 0` blocks are skipped, without evaluating
general preprocessor conditions. Macros are not expanded and macro
definitions are not indexed in this phase.

The scanner tolerates malformed C++ and produces only symbols recognizable
from bounded lexical tokens. Ambiguous expressions, function pointers,
local declarations, macro-generated declarations, and semantic aliases may
be omitted or classified conservatively. Duplicate names are retained; there
is no declaration-resolution pass.

## Incremental indexing and dirty documents

Opening a project performs one bounded recursive source-file index using the
existing workspace callbacks. Excluded generated/output directories are not
traversed. If a project document is open and dirty, its in-memory buffer
replaces the disk candidate during the initial index.

After an edit, only that document is rescanned. The previous path entry is
removed and replaced with the new symbol block; the rest of the project is
not rescanned. Save and close restore the corresponding disk snapshot when
needed. Every location carries the document identity and generation used for
the scan, so UI navigation can fall back to a bounded line/name search when a
disk document becomes an open document with a different runtime ID.

## Bounds

| Resource | Full package limit |
|---|---:|
| Project symbols | 100,000 |
| Symbols per document | 20,000 |
| Indexed project documents | 2,048 |
| Search query | 1,024 bytes |
| Visible Go To Symbol results | 100 |
| Editable source file | 256 KiB |
| Lexical token stream | 131,072 tokens |

When storage is exhausted, accepted symbols remain deterministic and the
database sets its truncation state. No unbounded allocation is attempted.
The Go To Symbol matcher checks prefix and substring matches using the same
ASCII case-folding convention as Find in Files; `Ctrl+I` toggles case
sensitivity in the picker.

## Outline and Go To Symbol

The left Outline panel shows symbols for the active document with compact kind
prefixes such as `[f]`, `[c]`, `[s]`, `[e]`, and `[n]`. Clicking an item opens
or activates its source document, scrolls the editor into view, places the
caret at the lexical location, and selects the identifier when the text still
matches.

`Ctrl+T` opens Search Symbols. Typing refreshes the bounded result list;
`Up`/`Down` selects, `Enter` navigates, and `Escape` closes. The picker is
project-wide, while the Outline remains document-scoped.

F12 Go To Definition reuses these same records. It captures a bounded
identifier, explicit qualifier, and containing lexical scope, then ranks
qualified and same-scope definitions ahead of same-name declarations. The
resolver is UI-independent and returns a direct result only when its score
margin is strong; otherwise the native shell displays a bounded candidate
picker. `Alt+Left` returns to the origin through generation-aware navigation
history. This remains lexical: it does not perform type inference, overload
resolution, include expansion, template resolution, macro expansion, or
language-server analysis. See [GO_TO_DEFINITION.md](GO_TO_DEFINITION.md).
