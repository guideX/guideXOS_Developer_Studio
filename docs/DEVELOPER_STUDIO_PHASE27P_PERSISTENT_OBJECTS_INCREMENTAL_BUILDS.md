# Developer Studio Phase 27P: persistent relocatable objects and incremental builds

Phase 27P adds a bounded, relocatable object cache to the bare-metal Developer
Studio build path. A source edit recompiles only the affected translation unit;
the linker still consumes every validated module and produces a fresh final ELF
on every successful build.

## Scope and invariants

The implementation is deliberately limited to the current bootstrap compiler:

- target architecture: AMD64;
- target ABI: `guideXOS C v1`;
- maximum source file size: 64 KiB;
- maximum object file size: 128 KiB;
- maximum project translation units: 16;
- final link: always full-project, never an incremental link;
- cache format: guideXOS `.gxo`, not ELF `.o` and not a host compiler object;
- source discovery: regular `.c` and `.cpp` files directly under the configured
  source root; header dependency tracking is not implemented yet.

The cache is an optimization only. A missing, truncated, corrupt, incompatible,
or semantically invalid object is treated as a cache miss. The source is then
compiled and the replacement object is published only after it has been
serialized and deserialized successfully.

## Object identity and location

For a project rooted at `/P27P`, the object for `src/math.cpp` is:

```text
/P27P/build/obj/amd64/src/math.gxo
```

The relative source path is part of the object identity and is retained in the
object. Keeping source subdirectories in the cache avoids collisions such as
`src/math.cpp` versus `tests/math.cpp`. Objects whose source is removed are
ignored because the linker receives the current source selection only; they are
orphaned cache files and do not become link inputs.

A cache hit requires all of the following to match:

1. the normalized project-relative source path;
2. the current source byte count;
3. the current source hash;
4. the serialized object's magic, format version, architecture, target ABI,
   compiler-object ABI version, bounds, checksum, and payload structure.

The source hash and object checksum use the compiler's deterministic FNV-1a
64-bit implementation. The checksum is an integrity check over the entire
object with its checksum field treated as zero; it is not a cryptographic
signature.

## `.gxo` wire format

All integer fields are explicitly little-endian. No C/C++ structure is written
directly, so host pointer size, alignment, and padding cannot change the file.
The fixed header is 100 bytes:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | magic: `GXO1` |
| 4 | 2 | format version: `1` |
| 6 | 2 | header size: `100` |
| 8 | 4 | target architecture: AMD64 (`1`) |
| 12 | 4 | target ABI: guideXOS C v1 (`1`) |
| 16 | 4 | compiler-object ABI version: `1` |
| 20 | 4 | flags: entry, host-log, return-constant-valid |
| 24 | 8 | source hash |
| 32 | 4 | source byte count |
| 36 | 4 | source-path byte count |
| 40 | 4 | code byte count |
| 44 | 4 | read-only data byte count |
| 48 | 4 | writable data byte count |
| 52 | 4 | export count |
| 56 | 4 | import count |
| 60 | 4 | relocation count |
| 64 | 4 | function count |
| 68 | 4 | global count |
| 72 | 4 | call-graph edge count |
| 76 | 2 | recursive SCC count |
| 78 | 2 | reserved, currently zero |
| 80 | 4 | entry code offset |
| 84 | 4 | payload byte count |
| 88 | 4 | complete object byte count |
| 92 | 8 | FNV-1a object checksum |

The payload is canonical and ordered as follows:

```text
source path bytes
token count, return constant
per-function recursion flags
function call-graph matrix
code, read-only data, writable data
exports
imports
relocations
```

Names are fixed-width, NUL-terminated fields inside the symbol records.
Deserialization validates every enum, name, count, offset, relocation width,
symbol signature, data range, entry offset, and exact payload consumption before
returning a module to the linker.

Changing serialized semantics requires incrementing the format or object ABI
version. Either change invalidates old cache entries. A target or ABI mismatch
also invalidates the entry before any linker state is populated.

## Build behavior and output

The bare-metal build service creates `build`, `build/bin/amd64`, `build/obj`,
and `build/obj/amd64` as needed. For each selected source it emits one of:

```text
Compiling src/math.cpp
Using cached object src/math.cpp
```

It then emits a full-link line. The build snapshot exposes append-only counters:

- `sourceFileCount`: current source selection;
- `compiledModuleCount`: translation units compiled during this build;
- `cachedModuleCount`: validated objects reused during this build;
- `linkedModuleCount`: modules passed to the final linker.

The final ELF is removed before a build starts and is written only after compile,
link, ELF validation, reopen/readback, and artifact checks succeed. Therefore a
compile or link failure cannot leave the previous successful artifact runnable.
Object publication uses a validated temporary file. Because the guest FAT/VFS
rename primitive does not provide replace-existing semantics, the final object
is overwritten only after the temporary object has passed round-trip validation.

## IDE proof sequence

The `dev.guidexos.phase27p` proof app and `/P27P` fixture exercise the complete
Developer Studio workflow:

1. cold build: three modules compile, three modules link, and the app returns
   42;
2. warm build: zero modules compile, three cache hits link, and the ELF bytes
   remain identical;
3. same-size edit to `src/math.cpp`: exactly one module recompiles and the app
   returns 41;
4. restore: the original result and ELF bytes return;
5. delete the math `.gxo`: the missing object is rebuilt while the other two
   modules remain cached;
6. introduce a syntax error: the build fails and Run is blocked;
7. introduce an undefined global: full relink fails and Run remains blocked;
8. restore, add/remove a source, and rename a source: source enumeration and
   cache paths follow the current project state, with stale objects ignored;
9. restore the original project and verify the final cached build returns 42.

The smoke output also includes markers for object header identity, deterministic
serialization, round-trip deserialization, target/version rejection, global
data, recursion metadata, segment permissions, and kernel survival. The host
object test independently mutates magic, checksum, version, target, ABI, and
relocation data and requires rejection.

## Validation and known limitations

The checked-in host tests cover deterministic serialize/deserialize, link parity
between fresh and persisted modules, and malformed-object rejection. The native
compiler syntax check covers the object, driver, and build-service translation
units.

This phase intentionally does not provide header dependency scanning, compiler
flag fingerprints beyond the fixed target/object ABI constants, cross-target
objects, persistent linker state, parallel compilation, or content-addressed
global cache sharing. It also does not claim cryptographic cache authenticity;
the cache is local build state and is defensively validated before use.

## Reproduction

From the Developer Studio repository, build the proof application with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-phase27p.ps1
```

From the server repository, the focused QEMU proof is:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/smoke-compiler-bootstrap.ps1 -Phase27POnly -BootCount 3
```

The focused host object-format proof is:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run-compiler-object-host-test.ps1
```
