# Debugger Phase 13 — Bounded Comparisons

Phase 13 extends the Phase 11 Watch expression engine, which is also reused by
conditional breakpoints. The grammar adds one comparison layer above the
existing unary, postfix, and primary expressions:

`==`, `!=`, `<`, `<=`, `>`, and `>=`.

Comparison results are debugger Boolean scalars displayed as `true` or `false`
and are consumed by conditional breakpoints through the existing Phase 12
truth-conversion path. A comparison Watch has no expandable value node.

Only integral scalars, Boolean/integer-like scalars, pointer equality, and
pointer/null-literal equality are supported. Relational pointer comparisons,
aggregates, arrays as values, floating-point and opaque values are rejected
with a bounded type diagnostic. Pointer equality validates non-null canonical
addresses but never dereferences them; comparing a pointer with literal `0`
never reads address zero.

Signedness and width come from the canonical DWARF type DIE. Signed/signed and
unsigned/unsigned comparisons use their respective mathematical 64-bit host
representations after bounded DWARF-width sign extension. For a mixed signed /
unsigned comparison, a negative signed value is below every unsigned value;
otherwise the non-negative signed value is compared as `uint64_t`. This rule
is explicit and does not depend on host compiler promotions.

Unparenthesized comparison chains such as `a < b < c` and `a == b == c` are
rejected as `UnsupportedExpression`. Parentheses make nested comparisons
explicit, so `(a == b) == 1` is accepted when its scalar types are compatible.

The existing limits remain unchanged: 256 expression bytes, 96 tokens, 64 AST
nodes, parser depth 16, 32 Watches, and 96-byte diagnostics. Arithmetic,
logical operators, assignment, calls, casts, pointer arithmetic, and unary
minus remain unsupported.
