# Debugger Phase 11 — Watches and Bounded Expressions

Developer Studio Watches are read-only expressions evaluated against the
currently selected stopped debugger frame. They reuse the Phase 9 DWARF
variable index and the Phase 10 `DebugDwarfValueNode` tree. Watch evaluation
does not introduce a second type system, source-text value inference, or a
target-side expression runtime.

## Grammar

```text
expression       := comparison
comparison       := postfix (comparison_operator postfix)?
comparison_operator := "==" | "!=" | "<" | "<=" | ">" | ">="
postfix          := unary postfix_suffix*
postfix_suffix   := "." identifier
                  | "->" identifier
                  | "[" expression "]"
unary            := "*" unary
                  | "&" unary
                  | primary
primary          := identifier
                  | integer_literal
                  | "(" expression ")"
```

Supported forms include `local`, `argument`, `object.member`,
`pointer->member`, `array[index]`, `*pointer`, `&variable`, decimal integer
literals, hexadecimal integer literals such as `0x10`, parentheses, and the
bounded scalar comparisons documented in [DEBUGGER_COMPARISONS.md](DEBUGGER_COMPARISONS.md).
Array indexes are bounded integer Watch expressions; general arithmetic is not
accepted.

The tokenizer recognizes only identifiers, integer literals, `.`, `->`, `*`,
`&`, brackets, parentheses, the six comparison operators, and end-of-input.
Function calls, assignments, arithmetic, logical operators, casts, `sizeof`,
pointer arithmetic, comma/ternary expressions, overloaded operators, templates,
methods, and side effects are explicitly unsupported. `operator*`,
`operator->`, and `operator[]` are not invoked; syntax always means builtin
DWARF pointer/member/array navigation.

## Bounds

* expression text: 256 characters;
* tokens: 96;
* identifier text: 63 characters plus terminator;
* numeric literal text: 32 characters;
* AST nodes: 64;
* parser nesting: 16;
* member-chain depth: 16;
* array-index nesting: 8;
* unary nesting: 8;
* Watch entries: 32.

Malformed, excessive, or unsupported input produces a deterministic parse
state and a short diagnostic with the source offset where available. Parsed
ASTs are bounded temporaries; expression text and parse diagnostics persist
in the Watch collection while runtime bindings are rebuilt for each stop.

## Evaluation semantics

Bare identifiers use the same visible local/argument index and shadowing
policy as Locals. The active local in the selected lexical scope wins, then
the formal argument. Arbitrary project globals are not searched.

`.` requires an aggregate. Member identity and address come from DWARF type
metadata and `DW_AT_data_member_location`; no source declaration layout is
parsed. `->` requires a builtin pointer, rejects null before any target read,
validates canonical/readable target memory, then selects the pointed aggregate
member. `[]` requires a DWARF array, validates lower/upper bounds and the
bounded materialized element set, and never clamps an invalid index.

`*` requires a builtin pointer. A scalar pointee is represented by the
existing scalar child; an aggregate pointee is represented by the existing
pointer-backed structured node and can be expanded lazily. `&` requires a
memory-backed lvalue with a real target address. Integer immediates,
register-only values, and unavailable temporaries return `AddressUnavailable`.

Results retain the Watch ID, type/value formatting, optional node/address,
selected frame, session/stop generation, artifact generation, and diagnostic.
Scalar and pointer formatting comes from the existing DWARF value model.
Structured results use the Phase 10 limits: maximum depth 8, maximum 16
children per node, maximum 16 materialized array elements, and maximum 64
materialized nodes in the shared Watch tree.

## Frames and stale state

Watches evaluate in the selected Call Stack frame. Caller-frame register
values are not substituted from frame zero; when a location cannot be
reconstructed the result is `UnavailableInCallerFrame`. Stack-backed caller
values remain available where the existing Phase 9 evaluator proves them.

When the target is running, Watch values become `Running` and no target reads
are performed. Continue, stepping, exit, frame changes, and artifact rebuilds
invalidate the old value tree. Expression text remains, while a later valid
stopped state reevaluates each Watch independently. Old node IDs reject
expansion after a generation change.

The explicit result states include `Available`, `ParseError`,
`UnsupportedExpression`, `UnknownIdentifier`, `TypeMismatch`, `NotPointer`,
`NotAggregate`, `MemberNotFound`, `NullPointer`, `UnreadableTarget`,
`IndexOutOfRange`, `AddressUnavailable`, `UnavailableInCallerFrame`, `Stale`,
`Running`, and `MalformedDebugInfo`.

## UI

The debugger panel has a Watch tab with Expression, Type, and Value columns.
`A` adds a Watch, `E` edits the selected expression, `Delete` removes one, and
Enter/Right expands a structured result. Add/edit parses immediately and
reevaluates when the target is paused. A failing Watch does not prevent other
entries from refreshing. The tab uses the same bounded disclosure nodes as
Locals and Arguments.

## Read-only guarantee

Watch evaluation may perform bounded target memory/register reads while the
target is paused. It never writes target memory or registers, changes
breakpoints, resumes execution, invokes functions, or performs assignments.
Assignment syntax is rejected before any target mutation path is reachable.

## Deferred work

General C++ evaluation, function/method calls, arithmetic expressions,
data breakpoints, casts, pointer arithmetic, dynamic types/RTTI,
STL pretty printers, NatVis, optimized-code reconstruction, and
memory/register editing remain outside Phase 11. Phase 13 adds the narrowly
bounded comparison layer shared by Watches and conditional breakpoints; its
contract is documented in [DEBUGGER_COMPARISONS.md](DEBUGGER_COMPARISONS.md).
