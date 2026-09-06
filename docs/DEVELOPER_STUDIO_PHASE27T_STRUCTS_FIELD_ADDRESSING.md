# Developer Studio Phase 27T — Structs and field addressing

Phase 27T adds named, bounded structs with deterministic layout to the
bootstrap compiler. It introduces compound typed objects without introducing
general aggregate-value semantics.

## Implemented language

The supported grammar is intentionally narrow:

```text
struct-definition := struct identifier { int identifier ; }+ ;
struct-object     := struct identifier identifier ;
struct-pointer    := struct identifier * identifier ;
field-expression  := identifier . identifier
                   | identifier -> identifier
```

Fields are scalar `int` only. Struct arrays and array fields are deferred.
Aggregate initializers, whole-struct assignment, by-value parameters, and
struct returns are rejected with diagnostics.

## Type table and layout

Each translation unit has a bounded table of 8 struct types, each type has at
most 16 fields, and a complete struct is at most 256 bytes. `StructTypeIR`
retains the type name, field count, size, alignment, identity, and field table.
Each `StructFieldIR` retains the field name, kind, element count and size,
offset, size in bytes, and alignment.

The current layout policy is declaration order, four-byte alignment, no hidden
padding, and a four-byte size for every `int` field. Thus:

```text
struct Point { int x; int y; };
x offset = 0, y offset = 4, sizeof(Point) = 8, align = 4
```

The layout is computed once by the parser and consumed by the backend; it is
not recomputed from source spelling during code generation.

## Storage and lowering

Struct locals are contiguous frame-resident objects. Their bytes count against
the existing 256-byte per-function local-object budget, so recursion uses the
same bounded activation accounting as arrays and pointer descriptors. Global
structs are zero-initialized in mutable RW data and retain their struct size
and identity in the module symbol.

For `p.x`, the backend evaluates the struct-object address and emits a fixed
field displacement. For `p->x`, it validates the struct-pointer descriptor,
loads its current address, and applies the same fixed displacement. Field loads
and stores remain ordinary 32-bit accesses.

Struct pointers are distinct from `int*`. Their descriptor carries the normal
current/base/extent/element-size/provenance fields; the element size is the
complete struct size and the descriptor is checked against the expected named
struct identity before `->` access. Struct-pointer arithmetic is deferred.

## Field pointers and provenance

`&p.x` produces an `int*` descriptor whose base and current address are the
field address, extent is exactly 4 bytes, element size is 4, and provenance is
`FieldSubobject`. Therefore `&p.a + 1` can represent one-past `a`, but cannot
dereference `p.b` even when the numeric address is adjacent. This preserves the
Phase 27S object-provenance rule for subobjects.

`&p` instead produces a struct-pointer descriptor covering exactly the whole
struct object. Descriptor copies are value copies; the caller's pointer state
is not changed by a callee.

## Cross-file compatibility and `.gxo`

A named struct identity hashes the name, declaration-ordered field names and
kinds, field counts, offsets, sizes, alignment, and complete type size. Two
translation units may repeat an equivalent definition, but a field deletion,
field-order change, or other layout change produces a different identity and
fails function-signature validation at link time.

The `.gxo` object ABI is version 5. Objects retain the complete bounded struct
type table, struct-pointer parameter identities, and struct global metadata.
The ABI bump invalidates older objects instead of interpreting them with the
new layout rules. Object serialization is deterministic and the host proof
checks byte-identical repeated serialization.

## Developer Studio proof path

The Server fixture is `scripts/fixtures/phase27t`: `types.cpp` contains the
shared type, `math.cpp` defines `sum_point(struct Point*)`, and `main.cpp`
creates a local point, writes `20` and `22`, calls the function, logs
`Struct field execution completed.`, and returns 42. The Studio repository
contains `app/phase27t_app.json` and `scripts/build-phase27t.ps1` for the
NativeElf package proof.

The focused host proof is:

```powershell
powershell -NoProfile -File .\scripts\run-compiler-structs-host-test.ps1
```

It covers grammar errors, deterministic layout, local field execution, field
stores, `.`/`->`, field subobject failure, struct pointers, recursion and the
call-depth guard, cross-file signature mismatch, global RW metadata, `.gxo`
round-trip, and deterministic object bytes. The existing arrays, functions,
globals, multi-file, object, and Phase 27S pointer host regressions also pass.

## Limits

Phase 27T remains AMD64-only and bounded. It does not add unions, bitfields,
packing controls, typedefs, classes, pointer fields, arrays of structs, array
fields, recursive by-value structs, heap allocation, function pointers,
general casts, pointer-returning functions, null-pointer semantics, or
struct-valued returns. Builds still perform a bounded internal relink; the
NativeElf runtime remains trusted. The separate Developer Studio close/freeze
issue is outside this phase.
