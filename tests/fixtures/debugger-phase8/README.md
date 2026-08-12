# Debugger Phase 8 fixture

This deterministic hosted Native GUI application exercises real Step Out from
`level3` through `level2`, `level1`, and `gx_main`. Build it with the existing
DebugSymbols recipe (`-O0 -fno-omit-frame-pointer -g`), stop on the assignment
inside `level3`, and use Shift+F11. The expected result is a real return-address
INT3 trap in the immediate caller with one fewer physical frame.

The fixture intentionally uses no C++ runtime, unwind tables, or dynamic
linking. The debugger must read the stopped target stack and use the raw saved
return address for the temporary breakpoint; `returnAddress - 1` is only for
caller source and symbol attribution.
