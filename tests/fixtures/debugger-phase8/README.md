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

The hosted overlap proof uses the same fixture and the real source mapper. Set
the initial breakpoint at `src/main.cpp:9` inside `level3`, then set the user
breakpoint at `src/main.cpp:15` in `level2`. The line-15 instruction is the
actual saved return destination after the `level3` call, while
`returnAddress - 1` maps the completed Step Out stop to line 14. The hosted
controller must share one physical binding for the user and temporary owners,
publish the overlap as a user `Breakpoint` stop at line 15, remove only its
internal owner, and re-hit that same user breakpoint after Continue.
