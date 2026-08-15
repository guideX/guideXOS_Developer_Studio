# Debugger Phase 10 fixture

This deterministic Native GUI fixture exercises the bounded structured-value
proof: nested structures, a fixed scalar array, an explicitly expanded
`Config*`, and a self-referential `Node*`. Build it with the DebugSymbols
recipe (`-O0 -fno-omit-frame-pointer -g`) and stop on the assignment in
`inspect`.

The fixture is also a complete Developer Studio native project. Its App Model
manifest is under `app/app.json`; the generated ELF is
`build/bin/amd64/debugger-phase10.elf`.

The expected return value is 264. The fixture has no source-layout contract;
the debugger must obtain member offsets, array bounds, and type sizes from
DWARF and read values from the stopped target.
