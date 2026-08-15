# Conditional Breakpoints

Developer Studio conditional breakpoints are debugger-owned filters on the existing software-breakpoint path. A breakpoint with no condition retains the ordinary breakpoint behavior. A conditional hit is evaluated only after the target is stopped, using the natural frame at the breakpoint location (normally frame 0), and the evaluator never calls target code or writes target state.

## Supported condition language

Conditions reuse the bounded Phase 11 Watch parser, AST, DWARF lookup, lvalue/value materialization, pointer validation, and memory-read machinery. The exact grammar is:

- identifiers
- bounded decimal integers
- bounded hexadecimal integers
- member access with `.`
- pointer-member access with `->`
- array indexing with `[]`
- unary `*`
- unary `&`
- parentheses
- scalar comparisons: `==`, `!=`, `<`, `<=`, `>`, `>=`

The limits are unchanged: 256 expression bytes, 96 tokens, 64 AST nodes, and parser depth 16. The debugger supports at most 128 condition-text storage slots, one per configured breakpoint capacity slot. Condition text is bounded by the Phase 11 expression limit.

This phase intentionally does not add a general C/C++ expression evaluator. Function calls, assignments, arithmetic, casts, `sizeof`, pointer arithmetic, templates, overloads, and other unsupported C++ syntax are rejected. Comparisons are the bounded Phase 13 scalar layer documented in [DEBUGGER_COMPARISONS.md](DEBUGGER_COMPARISONS.md); `inspect()` remains an invalid condition.

## Truth conversion

Only already-supported scalar values can be converted to a condition result:

- integer or Boolean zero is false; nonzero is true
- a null pointer is false
- a non-null canonical pointer is true
- structured aggregates and arrays are not implicitly truthy

A noncanonical pointer or any non-scalar/unsupported value produces a condition evaluation error. Pointer comparisons support equality/inequality, including comparison with literal zero; relational pointer comparisons remain unsupported.

## Hit behavior and errors

An unconditional hit is surfaced normally. A true condition is surfaced normally. A false condition is recorded as an internal filtered hit and uses the existing restore, single-step, reinsertion, and resume sequence; the breakpoint remains installed for later hits. The UI labels this transient state as a filtered breakpoint run.

If parsing or evaluation cannot be completed safely—for example because of an unknown identifier, stale frame/session context, malformed DWARF, unreadable memory, invalid pointer dereference, out-of-range index, or unsupported truth conversion—the debugger remains stopped and records a bounded `ConditionError` diagnostic with the condition text. It never silently resumes on an evaluation failure.

Breakpoint condition editing parses immediately. Invalid text remains attached to the breakpoint as an explicit invalid condition state, so the underlying breakpoint is not removed or corrupted. Conditions survive normal breakpoint refresh/re-resolution and are cleared independently from enable/disable state.
