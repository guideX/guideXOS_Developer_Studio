# Declaration–Definition Relationships

Developer Studio now maintains a bounded, project-local declaration–definition
relationship graph in `developer_studio_relationships.*`. The graph is built
from the existing lexical `SymbolDatabase`; it does not invoke Clang, parse a
compiler AST, implement a language server, or scan external headers.

## Identity and normalization

Each eligible symbol is copied into a deterministic `SymbolRelationshipKey`.
The key contains the symbol family, name, qualified name, containing scope,
static/member classification, callable qualifiers, parameter count, and a
bounded normalized signature. Namespaces, macros, typedefs, and `using` aliases
remain in the symbol index but are excluded from declaration–definition pairs.

Callable identity uses qualified name plus normalized parameters. Methods also
retain `const`, `volatile`, `&`, `&&`, and `noexcept` qualifiers when the lexical
index retained them. Constructors use the qualified class identity and
parameters; destructors use the qualified class identity and destructor name.
Types use qualified name and type family. Variables use qualified name and
static/member classification.

Normalization removes bounded whitespace differences, declaration-only
terminators, parameter names when the declarator shape is safe, and default
arguments. Nested parentheses, brackets, braces, angle brackets, strings, and
character literals protect commas and `=` while the parameter list is split.
Function-pointer and array declarators are preserved. An uncertain or bounded
parse sets `signatureComplete` false and `lexicallyApproximate` true; it never
becomes an `Exact` relationship.

## Groups and confidence

`SymbolRelationshipGroup` retains declaration, definition, and forward
declaration endpoints. Duplicate declarations are retained. Multiple
definitions make the group ambiguous and are never silently collapsed. Class
and struct keys use one conservative class-like group; a class/struct-key
mismatch is `Strong` with an explicit reason flag, while exact matching retains
the same key kind.

Namespaces are intentionally not assigned declaration–definition pairs because
namespace blocks are normally repeated namespace parts. Aliases likewise have
no invented definition. Inline in-class method definitions are their own
definition site and are not linked to themselves.

Edges carry `Exact`, `Strong`, `Possible`, or `Ambiguous` confidence, a bounded
rank score, and reason flags. Exact requires the same compatible kind, exact
qualified name, complete normalized signature, and compatible roles. Strong is
used for a unique lexical candidate with approximate identity, class/struct
key variation, include evidence, or path proximity. Possible and Ambiguous
remain visible to the caller and are not presented as semantic facts.

The Include Graph contributes only ranking evidence: a resolved source-to-
header edge and a bounded header/source stem match can increase a score. An
include edge is never required and never proves identity. Deterministic
tie-breaking uses confidence, role, qualified name, signature, relative path,
line, column, and byte offset rather than insertion or filesystem order.

## Build lifecycle and bounds

`SymbolRelationshipGraphService` has one active operation. It collects indexed
symbols, packs bounded candidate groups, matches relationships in work-budgeted
polls, and swaps a completed graph only after the new graph is terminal. The
last completed graph remains available during collection. Cancellation and a
newer build are safe; no build step reads source files. The graph carries the
project generation and symbol-database generation and is stale when either no
longer matches.

Each dirty-document index update advances the symbol-database generation, so a
current graph is rejected before navigation or feature enrichment. The next
build consumes the already-updated in-memory symbol records and does not scan
the source tree. The current bounded service rebuilds its retained graph from
those records; it does not yet maintain a differential reverse-key index for
rebuilding only affected groups.

The model contract allows up to 100,000 keys, 1,000 endpoints per key, and
500,000 edges. The Native ELF shell supplies a smaller retained working set so
that embedded memory remains bounded; storage overflow marks the graph or group
truncated and preserves deterministic ordering.

## Navigation

F12 first checks the relationship graph for a declaration or forward
declaration under the caret and prefers its linked definition. If there is no
relationship, the existing lexical Go To Definition resolver remains the
fallback. F12 on a definition reports `Already at definition.`.

Ctrl+F12 navigates from a definition to declaration candidates. A unique
candidate opens directly; multiple declarations or definitions use the existing
bounded picker, showing role, path, line, signature, and confidence. Alt+F12 is
implemented as `Switch Declaration / Definition`; it uses the relationship
direction implied by the current role. All successful activation goes through
the existing project-relative path checks, stale-range recovery, selection,
and navigation-history push.

The relationship endpoint stores document identity and generation, project-
relative path, byte range, source location, identifier, qualified name, and
normalized signature. Exact range and identifier text are checked first, then
the stored line and a bounded nearby search. If no unique safe range remains,
activation reports a stale relationship instead of selecting unrelated text.

## Integration limits

The symbol index, Outline, Go To Symbol, and Include Graph retain their
existing bounded lexical records. Find All References and Rename Symbol use a
current graph to classify linked declaration/definition endpoints as exact;
their project-wide token scan remains the safe fallback for call sites and
unresolved uses. Signature Help uses the shared normalized identity when
collapsing declaration/definition copies, and Completion uses it to keep
duplicate copies out of overload counts. All four fall back safely when a
relationship graph is unavailable or stale. Relationship identity does not
add call-site overload resolution, template
instantiation, inheritance or virtual-override graphs, macro-generated
symbols, external/system-header relationships, ODR diagnostics, or compiler-
equivalent C++ semantic identity.

Stable model error names include `RELATIONSHIP_GRAPH_STALE`,
`RELATIONSHIP_NORMALIZATION_APPROXIMATE`, `RELATIONSHIP_MULTIPLE_DEFINITIONS`,
`RELATIONSHIP_MULTIPLE_DECLARATIONS`, `RELATIONSHIP_AMBIGUOUS`,
`RELATIONSHIP_LOCATION_STALE`, and `RELATIONSHIP_ACTIVATION_FAILED`.

## Future sequence

The intended future architecture is:

```text
Declaration–Definition Relationships
        ↓
Header/Source Ownership
        ↓
declaration ownership by header
        ↓
lightweight local type hints
        ↓
semantic member resolution
        ↓
semantic reference identity
        ↓
optional language service
```

Only the first stage is implemented here.
