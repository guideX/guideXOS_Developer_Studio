# Find All References

Developer Studio's `Shift+F12` command provides bounded, project-local,
lexical Find All References for C and C++. It is deliberately built on the
existing symbol index, Go To Definition resolver, syntax tokenizer, dirty
document snapshots, project search enumerator, and source-navigation path.
It does not provide compiler binding, Clang, a language server, type
inference, complete overload resolution, macro expansion, or Rename Symbol.

## Workflow and target resolution

The command extracts the identifier under the caret or from a valid
identifier selection, captures the existing lexical qualifier/scope, and
queries `SymbolDatabase` through the same `ResolveDefinition` ranking used by
F12. A strong unique candidate starts the search. Multiple plausible symbols
open a separate session titled **Choose Symbol for Find All References**;
symbols are never silently merged. The picker retains overloads and same-name
symbols as separate candidates.

The UI-independent `ReferenceTarget` contains a deterministic target ID,
project and document generations, identifier, qualified name, containing
scope, signature, symbol kind/role, and declaration location. It owns bounded
copies of all text and retains no editor pointer, UI pointer, file handle, or
mutable document reference. A target ID is based on project identity,
qualified name, signature, kind, and—when no qualified identity exists—the
declaration location.

If the index is ready but no symbol candidate exists, the command starts an
explicit identifier-only fallback. The panel displays:

> No indexed symbol was resolved. Showing lexical identifier matches.

Fallback rows are `LexicalMatch` with `LexicalOnly` confidence. They are not
presented as references bound to a semantic symbol.

## Operation and scanner reuse

`ReferenceSearchService` owns a bounded `ReferenceSearchOperation` with
explicit `ResolvingTarget`, `Enumerating`, `Searching`, `Cancelling`,
`Completed`, `Cancelled`, and `Failed` states. One current operation is
allowed. Starting another operation supersedes the previous operation ID;
polling the old ID is rejected. Cancellation is observed by the shared
`ProjectSearchService` at directory/file checkpoints, and partial results are
retained until the result session is released.

The reference service extends the existing project scanner with a synchronous
per-file scan visitor. The shared scanner still owns:

- project-root validation and path containment;
- deterministic directory/file enumeration and default exclusions;
- symlink-safe `stat`/`list` behavior supplied by the filesystem contract;
- dirty-document snapshot substitution, exactly once per eligible path;
- binary and oversized-file rejection;
- file, directory, byte, timeout, and poll-work bounds; and
- cancellation, supersession, and terminal-state behavior.

The visitor consumes the current file bytes before returning and retains no
pointer to them. It tokenizes the file synchronously within that bounded poll;
the editor event loop never performs a full project scan.

## Lexical token policy

The visitor calls the existing `SyntaxTokenizeLine` stream with its multiline
comment and raw-string state. Only complete ASCII identifier tokens are
compared:

```text
first:      A-Z a-z _
following:  A-Z a-z 0-9 _
```

Malformed UTF-8 is retained safely but is not treated as Unicode identifier
syntax. Prefixes such as `Build`, `Build2`, and `Rebuild` do not match
`BuildProject`.

Identifiers in line comments, block comments, ordinary strings, character
literals, raw strings, and `#include` path text are excluded. Preprocessor
directive lines are not searched. A small directive state suppresses nested
`#if 0` regions and resumes after `#endif`; `#else` of a direct `#if 0` is
treated as active. Other conditional expressions are not evaluated. Macro
expansion, include expansion, macro identity, header search, and system or
external-library indexing are out of scope.

## Classification and confidence

Every matching token starts as a lexical candidate. Bounded context signals
include explicit `::` qualification, tracked namespace/class scope, the
stored declaration role, call-like `(` syntax, type-like context, member
access punctuation, address-of, and simple assignment/increment forms.

Reference kinds are `Definition`, `Declaration`, `ForwardDeclaration`,
`AliasDeclaration`, `FunctionCall`, `MethodCall`, `TypeUse`, `NamespaceUse`,
`MemberAccess`, `VariableUse`, `PossibleWrite`, `PossibleRead`, `AddressUse`,
`LexicalMatch`, and `Unknown`. Read/write labels are possible lexical
classifications; no dataflow claim is made.

Confidence is always visible in the result row:

- `Exact`: an indexed declaration/definition record or explicit qualified
  target match, including a unique matching lexical scope;
- `Likely`: same-name use with compatible local context but no exact record;
- `Ambiguous`: a competing scope, unknown member receiver, or unresolved
  overload relationship; and
- `LexicalOnly`: identifier fallback.

Ambiguous results are included by default. The panel exposes the declaration
and ambiguous policies as enabled defaults; the service request also carries
these filters so a future UI toggle does not alter the scanner contract.
Declarations, definitions, forward declarations, and aliases are included
by default. Overloads remain separate targets. When a signature is known,
unresolved call sites remain ambiguous rather than being assigned by argument
types. Constructors and destructors are indexed and explicit `~Name` tokens
are handled lexically; implicit lifetime behavior is not inferred.

Classes, structs, enums, unions, typedefs, using aliases, namespaces,
variables, and members use the same identifier-token pipeline. Qualified
names such as `guideXOS::BuildProject` and `Renderer::Draw` receive stronger
confidence than an unrelated unqualified same-name token. An unknown
`renderer.Draw()` receiver remains ambiguous or likely; receiver type is not
inferred.

## Results, limits, and previews

Results are stored as fixed-size `ReferenceMatch` records and grouped by
project-relative file path. Each row contains byte offset, 1-based line and
column, identifier length, kind, confidence, preview, preview-match range,
document ID/generation, dirty-snapshot origin, and stale state. Duplicate
path/offset matches are suppressed. File groups sort case-insensitively by
relative path; matches retain source offset order. The shared scanner and
reference service currently cap project search at 100,000 files, 256 MiB
scanned bytes, 256 result files, 512 matches per file, 4,096 total retained
matches, and five minutes. Reference token inspection is capped at 4,000,000
identifier tokens. File reads remain bounded by the existing 256 KiB editor
file limit. Exceeding a bound sets `truncated` and exposes a stable error
code; it does not silently claim completeness.

Previews reuse `ProjectSearchBuildPreview`: one normalized source line,
bounded to 512 bytes, with tabs/control bytes normalized and left/right
truncation flags. Match offsets remain offsets into the source snapshot.

## Dirty buffers and activation

At operation start, each dirty project document is copied into a bounded
snapshot with its document ID and generation. The scanner substitutes that
snapshot for the disk file, so a dirty path is not searched twice. Later edits
do not mutate retained results. A result is stale when the project generation,
path, document generation, or source text no longer validates.

Activating a row revalidates the project identity/generation, rejects traversal
or absolute paths, and opens the project-relative path through
`WorkspaceControllerOpenDocumentAtLocation`. If the result's byte offset and
generation still identify the target token, that exact identifier is selected.
Otherwise a bounded same-line exact-token recovery is attempted. If recovery
fails, the caret is clamped to the reported location without selecting
unrelated text and the row is marked stale. Dirty tabs continue through the
existing open-document path and are not replaced with disk content.

Successful activation pushes the captured Shift+F12 origin into the existing
`NavigationHistory`; `Alt+Left` returns to it and `Alt+Right` continues to use
the existing forward stack. F12, Ctrl+T, Outline, Find, Find in Files, Save,
Build, Run, Output, and Problems navigation retain separate state.

## Markers and errors

Markers are operation-level and bounded; no marker is emitted per reference.
The implementation uses markers such as:

```text
references_begin=PASS identifier=<bounded-name>
references_target=MULTIPLE candidates=<n>
references_target=LEXICAL_FALLBACK
references_search_begin=PASS
references_complete=PASS files=<n> references=<n>
references_complete=CANCELLED
references_complete=TRUNCATED files=<n> references=<n>
references_complete=FAIL reason=<code>
references_activate=PASS path=<bounded-path> line=<n> column=<n>
references_activate=STALE
references_activate=FAIL reason=<code>
```

Stable service codes include `REFERENCES_NO_PROJECT`,
`REFERENCES_NO_DOCUMENT`, `REFERENCES_NO_IDENTIFIER`,
`REFERENCES_TARGET_NOT_FOUND`, `REFERENCES_TARGET_AMBIGUOUS`,
`REFERENCES_LEXICAL_FALLBACK`, `REFERENCES_PROJECT_STALE`,
`REFERENCES_DOCUMENT_STALE`, `REFERENCES_PATH_OUTSIDE_PROJECT`,
`REFERENCES_FILE_TOO_LARGE`, `REFERENCES_BYTE_LIMIT`,
`REFERENCES_TOKEN_LIMIT`, `REFERENCES_RESULT_LIMIT`,
`REFERENCES_TIMEOUT`, `REFERENCES_CANCELLED`,
`REFERENCES_OPERATION_STALE`, `REFERENCES_NO_RESULTS`,
`REFERENCES_ACTIVATION_STALE`, and `REFERENCES_ACTIVATION_FAILED`.

## Future path

The intended progression remains:

```text
Lexical Find All References
        ↓
declaration-definition relationship graph
        ↓
include graph
        ↓
lightweight local type hints
        ↓
semantic symbol identity
        ↓
safe Rename Symbol
        ↓
optional language service
```

Only the first lexical step is implemented here.
