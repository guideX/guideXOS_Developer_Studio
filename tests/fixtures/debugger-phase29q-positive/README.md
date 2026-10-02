# Phase 29Q positive GXSM fixture

This hosted fixture uses the Server's production AMD64 bootstrap compiler
backend, compiler-object serializer/deserializer, linker, ELF writer, and
GXSM v2 final-image writer. The source file is compiled at build time; no
metadata bytes, frame offsets, scope ranges, or expected watch values are
embedded by the smoke harness.

`debugProbe` gives the debugger a real caller frame. `counter` is assigned
before the line-4 breakpoint, changes during Step Over, and leaves scope when
`debugProbe` returns to `gx_main`.
