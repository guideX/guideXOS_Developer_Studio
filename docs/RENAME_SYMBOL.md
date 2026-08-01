# Rename Symbol

Developer Studio provides a bounded, preview-first lexical Rename Symbol
operation for indexed C and C++ project symbols. `F2` starts it only when the
source editor has focus. File, folder, project-tree, and desktop rename paths
are not handled by this command.

## Scope and architecture

Rename reuses the same identifier extraction and definition resolution path as
`F12` and the same `ReferenceSearchService` as `Shift+F12`:

```text
caret/selection
  -> indexed target resolution
  -> identifier validation
  -> bounded reference scan
  -> exact/likely/ambiguous classification
  -> grouped preview
  -> pre-apply revalidation
  -> reverse-offset file/document edits
  -> symbol/index refresh
  -> one workspace undo record
```

The operation is lexical. It does not claim complete C++ semantic correctness,
overload binding, type inference, macro expansion, or language-server safety.

## Supported targets

The initial supported indexed kinds are Namespace, Class, Struct, Union, Enum,
Function, Method, Constructor, Destructor, Global Variable, Static Variable,
Typedef, and Using Alias. Local variables and parameters are not indexed as
rename targets. Macros, operators, external/system headers, generated files,
templates, virtual dispatch, and cross-project symbols are unsupported.

An unresolved identifier does not fall back to blind lexical replacement. The
stable failure is `Rename requires an indexed symbol.` Multiple indexed targets
show a bounded picker; overloads are not silently merged.

Class targets keep constructor and destructor declaration records coupled when
the lexical index can identify that relationship. A destructor target is shown
as `Renderer` in the editable name field; the stored `~Renderer` spelling is
maintained in declarations and definitions where the reference scanner marks it
as exact. Alias rename changes the alias only, never its underlying type.

## Name validation and conflicts

The replacement must be an ASCII C/C++ identifier: `[A-Za-z_][A-Za-z0-9_]*`, no
longer than 1,024 bytes. Empty, whitespace, punctuation, qualified names,
member expressions, path separators, non-ASCII bytes, embedded NULs, and the
C/C++ keyword set used by syntax highlighting are rejected. Operator renaming
is deferred. A same-name request produces `RENAME_UNCHANGED`; case-only names
are accepted because identifiers are case-sensitive, subject to the normal
content and filesystem checks.

Conflict detection is intentionally lexical and incomplete. An exact same
qualified name and compatible signature is blocking. A different signature in
the same function scope, nearby same-name symbol, or possible overload is a
warning shown before Apply. Warnings do not select uncertain references by
default.

## Candidates and preview

Each reference is a bounded immutable candidate with a project-relative path,
byte offset, line/column, expected text, replacement text, reference kind,
confidence, dirty-document origin, generation, and explicit selection state.
Results are deterministic and grouped by file. Comments, strings, character
literals, raw strings, inactive preprocessor regions, and binary files are
excluded by the shared syntax/token scanner.

Selection defaults are:

| Confidence | Default |
| --- | --- |
| Exact, including declarations and definitions | Selected |
| Likely | Unselected |
| Ambiguous | Unselected and warned |
| Lexical-only | Disabled; never a Rename Symbol edit |

The preview shows before/after snippets, confidence, reference kind, dirty
origin, stale state, and per-file/per-confidence counts. The name field,
keyboard navigation, mouse selection, file scrolling, `Select Exact`, `Clear`,
Apply, and Cancel are available. No source is mutated while the preview is
open.

## Plan, containment, and transaction

The immutable plan is grouped by project-relative file. Paths are normalized and
checked with the existing workspace containment helper immediately before
mutation. Selected edits must be in range, non-overlapping, unique, and still
contain their expected old text. Each file is applied from the greatest byte
offset to the smallest so earlier offsets remain valid.

Immediately before Apply, the project generation, open-document identity and
generation, file size, content hash, path containment, and every selected old
text are revalidated. A stale selected candidate aborts the entire operation
before mutation. Candidate storage, reference results, preview snippets,
transaction edits, files, and undo records reuse the existing bounded limits:

* 256 affected files;
* 4,096 total edits and 512 edits per file;
* 256 KiB source/output file size;
* 64 MiB total validation snapshot accounting;
* 512-byte preview snippets;
* 16 compact rename undo records.

Open documents are edited in memory and remain dirty; Rename never saves them
automatically. Closed project files are read, edited, and written through the
existing `WorkspaceFileSystem` boundary. The current Server filesystem contract
has no atomic replacement primitive, so the implementation preserves unrelated
bytes and attempts bounded inverse-edit rollback on a write failure; it does not
claim crash-safe atomic replacement. A rollback failure is reported as
`RENAME_ROLLBACK_FAILED`, never as partial success.

## Undo

One successful rename produces one logical workspace undo record. It stores
compact inverse edit locations, before/after hashes, document generations,
dirty state, caret, and selection metadata. `Ctrl+Z` validates every affected
open document and closed file before changing any of them. Later user edits,
file changes, project switches, document replacement, or changed hashes cause
`RENAME_UNDO_STALE`; the undo never overwrites later work. Redo is not
implemented.

Syntax, symbol, and search state are refreshed after Apply and Undo. Save and
Save All are not invoked by either operation.

## Lifecycle and markers

The operation may be cancelled during target picking or the asynchronous
reference scan. Cancel before Apply leaves all source bytes unchanged. The
bounded lifecycle emits markers including `rename_begin`, `rename_target`,
`rename_references_begin`, `rename_references_complete`, `rename_preview`,
`rename_apply_begin`, `rename_revalidate`, `rename_apply`, `rename_undo`, and
stable `RENAME_*` error codes.

## Limitations and future path

The current lexical implementation cannot prove overload resolution, template
relationships, include visibility, inheritance, virtual dispatch, macro
expansion, local-scope identity, or all name conflicts. Future semantic work can
follow this path without changing the preview-first boundary:

```text
lexical preview-first Rename
  -> declaration/definition relationship graph
  -> include graph
  -> local type hints
  -> semantic symbol identity
  -> semantic Rename Symbol
  -> optional language service
```
