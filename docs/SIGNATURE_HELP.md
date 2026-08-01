# Lightweight Signature Help

Developer Studio provides bounded, lexical Signature Help from the source
editor with `Ctrl+Shift+Space`. It consumes the existing C/C++ syntax cache,
dirty-document generation, and project `SymbolDatabase`. It does not perform
compiler overload resolution, argument-type matching, conversion ranking,
template deduction, receiver-type inference, or documentation lookup.

## Architecture

```text
caret position
  -> bounded backward invocation scan
  -> nearest unmatched opening parenthesis
  -> callable and qualifier extraction
  -> bounded top-level comma count
  -> existing project symbol lookup
  -> deterministic candidate ranking and deduplication
  -> generation-safe SignatureHelpSession
  -> one bounded editor popup
```

`developer_studio_signature.*` is UI-independent. The model retains copied
strings, stable project/document IDs, generations, byte offsets, fixed-size
candidate storage, and parameter ranges. It owns no editor, UI, filesystem,
or Server handles. `main.cpp` owns popup placement, input routing, status text,
and deterministic markers.

## Manual trigger and session lifecycle

Signature Help is manual only. Typing `(` or `,` does not open a popup. The
user places the caret inside a call and presses `Ctrl+Shift+Space`. Once open,
the session is refreshed after ordinary text insertion, Backspace, Delete, or
Enter while the caret remains in the same call context. Caret navigation,
document switching, project/workspace changes, Build, Run, close, and Escape
dismiss it. The feature never mutates source text and never creates an undo
record.

Only one code-intelligence popup is visible. Opening Signature Help dismisses
Completion. Opening Completion with `Ctrl+Space` dismisses Signature Help.
Completion insertion continues through its existing guarded mutation and undo
path; Signature Help is not silently reactivated after insertion.

## Active-call discovery

The model scans backward at most 64 KiB from the caret. It uses a bounded
delimiter stack with a maximum nesting depth of 256. Parentheses, brackets,
and braces inside comments, strings, character literals, raw strings, and
preprocessor spans are ignored using the active document's syntax-cache spans.
The nearest unmatched `(` is selected, so a caret after a nested call's
closing parenthesis can still belong to its outer call, while a caret after
the outer closing parenthesis has no active call.

The source syntax cache must be valid for the current document generation. A
stale or unavailable cache fails with `SIGNATURE_INDEX_NOT_READY` rather than
guessing through comments or literals. `#if 0` regions are recognized by a
small bounded conditional scan for documents within the context bound; an
inactive region is rejected as `SIGNATURE_IN_PREPROCESSOR`.

The extracted callable includes the unqualified identifier and, for
`guideXOS::BuildProject(` or `Renderer::Create(`, the explicit `::` qualifier.
For `renderer.Draw(`, `renderer->Draw(`, and `this->Draw(`, the model records a
bounded receiver hint but leaves `receiverTypeResolved=false`. A function
pointer/callable-object form such as `(*callback)(` is unsupported. Control
keywords such as `if`, `for`, `while`, `switch`, `sizeof`, and casts are not
treated as callable symbols.

The active argument is the zero-based count of top-level commas between the
opening parenthesis and caret. Nested parentheses, brackets, braces,
function-pointer declarators, strings, comments, and raw strings do not count
their commas. Angle brackets use a conservative bounded heuristic: `<` is
considered template-like only after an identifier or `>` when a plausible `>`
appears soon and obvious expression operators are absent. Ambiguous angle
contexts set `parameterPositionApproximate`; they do not claim C++ template
correctness.

## Candidates and ranking

The model reads only `Function`, `Method`, and `Constructor` symbols from the
existing index. Lookup prefers an exact qualified name, then same scope,
same document, and exact unqualified name. Case-insensitive fallback is used
only when no exact-case callable exists. Member calls gather methods by name
and are explicitly marked lexical and potentially ambiguous; no receiver type
is inferred.

Scores are deterministic and do not use argument types. Exact qualified name,
exact case, same scope, explicit qualifier parent, same document, declaration
role, invocation-kind match, and parameter-position compatibility contribute
bounded scores. Unresolved member receivers are penalized. Ties use qualified
name, display signature, declaration role, path, line, column, and byte offset.

Declaration/definition duplicates are merged only when the normalized stored
signature, qualified name, and kind agree and the roles identify a declaration
versus definition (or the source location is identical). Distinct same-role
symbols are retained when the lexical scanner cannot preserve qualifiers
reliably. Overloads remain separate. At most 2,000 matches are gathered and
256 candidates are retained.

## Stored signatures and parameter parsing

The symbol scanner currently preserves the callable name, qualified name,
parameter-list text through the matching `)`, declaration role, and source
location. This is sufficient for lexical display and parameter ranges. It does
not reliably preserve return types or trailing `const`, `noexcept`, and
ref-qualifiers; Signature Help therefore displays the stored form and does not
invent missing information.

Stored signatures are split only enough to identify parameter display ranges.
The splitter handles nested parentheses, brackets, braces, conservative angle
brackets, function-pointer declarators, default expressions, and retained
string/character text. Parameter metadata contains display text, a lexical
name when evident, type text when evident, default-value and variadic flags,
and a byte range in the displayed signature. `void` lists are treated as
zero-parameter lists. A variadic parameter can receive the active highlight
for later argument positions.

When parsing is uncertain, the full signature remains visible but precise
parameter highlighting is disabled and the popup says:

```text
Parameter highlighting unavailable for this signature.
```

Approximate angle-bracket positions similarly report:

```text
Parameter position is approximate.
```

## Popup behavior

The popup is anchored to the caret and clamped to the editor rectangle. It
shows a bounded overload list, selected-overload highlight, declaration/path
detail, and the active parameter when a precise range exists. `Up`, `Down`,
`Page Up`, `Page Down`, `Home`, and `End` navigate overloads. Mouse wheel and
row clicks are bounded to the popup; a click outside dismisses it.

The session stores the selected candidate identity across a refresh when that
candidate remains available. A document generation change recomputes the
active call and candidate list. If the project or document identity/generation
no longer matches, the session is stale and is dismissed without retaining
pointers into the old document.

## Limits and status codes

The model bounds the backward scan to 64 KiB, delimiter depth to 256, callable
names to 1,024 bytes, qualifiers to 2,048 bytes, receiver hints to 1,024
bytes, collection to 2,000 candidates, retention to 256 candidates, and
parameters per signature to 64. Display and detail strings have independent
limits. No invocation scans the project filesystem or rebuilds the symbol
index.

Stable model codes include:

```text
SIGNATURE_NO_PROJECT
SIGNATURE_NO_DOCUMENT
SIGNATURE_NO_ACTIVE_CALL
SIGNATURE_UNSUPPORTED_CONTEXT
SIGNATURE_IN_COMMENT
SIGNATURE_IN_STRING
SIGNATURE_IN_CHARACTER
SIGNATURE_IN_RAW_STRING
SIGNATURE_IN_PREPROCESSOR
SIGNATURE_CONTEXT_TOO_LARGE
SIGNATURE_NESTING_LIMIT
SIGNATURE_CALLABLE_TOO_LONG
SIGNATURE_QUALIFIER_TOO_LONG
SIGNATURE_INDEX_NOT_READY
SIGNATURE_PROJECT_STALE
SIGNATURE_DOCUMENT_STALE
SIGNATURE_SESSION_STALE
SIGNATURE_NO_CANDIDATES
SIGNATURE_RESULTS_TRUNCATED
SIGNATURE_CANDIDATE_LIMIT
SIGNATURE_PARSE_APPROXIMATE
SIGNATURE_PARAMETER_PARSE_FAILED
SIGNATURE_RECEIVER_UNRESOLVED
SIGNATURE_INTERNAL
```

Routine state stays in the popup or status bar. Exceptional failures can be
reported through the existing DeveloperStudio / CodeIntelligence output path.
Markers are bounded and aggregate-only:

```text
signature_begin=PASS
signature_context=PASS callable=<name> argument=<n>
signature_context=QUALIFIED qualifier=<name>
signature_context=MEMBER_LEXICAL
signature_candidates=PASS total=<n> retained=<n>
signature_candidates=TRUNCATED retained=<n>
signature_parameter=PASS|APPROXIMATE index=<n>
signature_refresh=PASS argument=<n>
signature_dismiss=PASS reason=<code>
signature_fail=FAIL reason=<code>
```

## Known lexical limitations and future progression

Signature Help does not parse macros, expand includes, resolve overloads,
match argument types, evaluate defaults, infer receivers, deduce templates,
resolve virtual dispatch, inspect external/system headers, or claim complete
C++ correctness. Stored signature quality limits return-type and qualifier
display until the symbol scanner grows richer metadata.

The intended progression is:

```text
Lexical Signature Help
        ↓
Include Graph
        ↓
declaration-definition relationship graph
        ↓
lightweight local type hints
        ↓
semantic overload ranking
        ↓
semantic member completion
        ↓
optional language service
```
