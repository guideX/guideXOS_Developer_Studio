# Debugger Phase 9 fixture

This deterministic Native ELF fixture is compiled with `-O0`,
`-fno-omit-frame-pointer`, and DWARF debug information. `calculate(10, 11)`
contains simple arguments, stack locals, a bool, and a pointer. The marked
assignment line is the intended source-breakpoint location for live Locals and
Arguments inspection.
