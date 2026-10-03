# Debugger Phase 15 fixture

This deterministic hosted Native ELF fixture combines the Phase 10 structured
value shapes with a repeated breakpoint and a caller frame. The breakpoint is
inside `debugLoop` and observes `counter` values 0, 1, and 2, so the hosted
condition `counter == 2` proves false, false, true filtering. `debugCaller`
retains `outerValue` for selected-frame and frame-sensitive Watch checks.

At the stopped breakpoint, `rect.origin.x` is 10, `values[2]` is 3,
`rectPtr` points at the aggregate, and `node.next` is a self-cycle.
