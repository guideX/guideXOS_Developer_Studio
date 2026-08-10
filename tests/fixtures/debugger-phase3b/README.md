# Debugger Phase 3B fixture

This is a deliberately small hosted Native GUI application used by the
Developer Studio source-breakpoint integration smoke. Its debug build emits
DWARF line information at `-O0`; the smoke arms the executable `draw_rect`
line through F9, starts Debugging with Ctrl+F5, verifies a real owned trap and
source navigation, then closes the session through the supported stop path.
