# Debugger Phase 7 fixture

This is a deterministic hosted Native GUI application used to validate the
Phase 7 call-stack path. The DebugSymbols recipe uses `-O0` and
`-fno-omit-frame-pointer`, and the three no-inline functions
`gx_phase7_level1 -> gx_phase7_level2 -> gx_phase7_level3` produce a real
AMD64 RBP chain while `gx_phase7_level3` is stopped.

The intended proof breakpoint is the `g_debugValue = value + 1` statement in
`src/main.cpp`. The fixture contains no C++ runtime, unwind tables, or dynamic
linking; the debugger must read the stopped target stack through the hosted
debug boundary and map saved return addresses with the ELF symbol/DWARF data.
