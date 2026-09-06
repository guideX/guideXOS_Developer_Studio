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

## Validation closure: Phase 27T-V (2026-09-06)

The original Phase 27T result was **Outcome B** because the implementation had
host, linker, object, and Developer Studio coverage, but the final proof had
not started QEMU and had no externally audited ELF emitted by the in-guest
compiler. Phase 27T-V adds that evidence; it does not broaden the language.

### Build diagnosis and bounded repairs

The reported `Microsoft.Build.Utilities.FileTracker`/
`UnauthorizedAccessException` did not reproduce in the live checkout. The
unchanged focused command completed the ordinary kernel and bootloader build
path and launched QEMU, so no elevation workaround, tracking disablement, or
permission change was made. The initial host audit found no relevant stale
MSBuild, compiler, or QEMU process holding the build outputs.

The separate standalone-build failure was an absent repository-relative
`third_party/mbedtls` tree. The focused smoke build intentionally passes an
empty `MBEDTLS_GUIDEXOS_IMPORT_STATE_DEPS` override for this compiler proof;
the Navigator/TLS path still reports its independent missing official Mbed TLS
4.1.0/TF-PSA source dependency. No arbitrary dependency copy was downloaded
or vendored.

QEMU exposed one genuine Phase 27T compiler defect: global struct address
emission left the absolute address in `RDX`, while struct field lowering expects
the base in `RAX`. The AMD64 backend now copies `RDX` to `RAX` before applying
the field displacement, and the host struct test checks the emitted bytes.
The remaining closure repairs are bounded validation infrastructure: smoke
fixture filenames now fit the FAT 8.3 path limit, the negative cache fixture
separates function-signature and field-order failures, the Studio smoke app
selects `/P27T`, and the fixture includes the `CMakeLists.txt` required by the
Studio project loader. The existing global-struct image is also exported for
the external RW-segment audit.

### Host and package validation

All eight required host suites passed: structs, objects, multi-file, pointers,
arrays, bootstrap, functions, and globals. The current Studio package command
also passed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-phase27t.ps1 `
  -ServerRoot D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO
```

It regenerated `Apps/DS27T/bin/amd64/p27t.elf`, 125656 bytes,
SHA-256 `46EBB93AA75723D8FED1F3E035D8F3CA02923964739035691FC165E1B6903D0D`.
The final kernel, bootloader, and OVMF inputs were rebuilt by the smoke script;
their SHA-256 values were respectively
`7FBDBB932AB69AE8A6F04FC3CA3BFC3B676EF9634EF3FFC0B86E7785455AB697`,
`A53687FEFAE5B4DBBCD38B324E863FA8AE500C28BAF9EB0DC7B2DEE66E56E38C`, and
`2DF617A6FD1BAE41EC7C0E3BCECA96C928A5A88D8DC198468D34733393AB2792`.

### Guest proof

The exact focused diagnostic command passed with one actual guest boot:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\scripts\smoke-compiler-bootstrap.ps1 `
  -BootCount 1 -TimeoutSeconds 600 -Phase27TOnly
```

The final command passed with three independent fresh QEMU launches and three
guest boots:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\scripts\smoke-compiler-bootstrap.ps1 `
  -BootCount 3 -TimeoutSeconds 600 -Phase27TOnly
```

All three boots reported every existing Phase 27T marker as `PASS`, including
`phase27t_object_version_migration=PASS`, `phase27t=PASS`, and
`phase27t_kernel_survival=PASS`; there were zero Phase 27T `FAIL` markers.
The guest log contains the expected `Struct field execution completed.` and
return value 42. Cold build compiled all three project translation units;
warm build reported three cache hits and zero compilations, then returned 42;
the one-file edit compiled one unit with two cache hits and returned 41; source
restore returned 42. The cached incompatible signature and reordered layout
both blocked the link, and the restored link returned 42. The adjacent-field
escape reported `invalid pointer dereference` with teardown complete, followed
by valid struct execution. Struct-pointer recursion returned 42; excessive
depth reported `recursive call depth limit exceeded` with teardown complete.
The final Studio backend/project/document/build/artifact-survival path all
passed inside the guest.

One earlier three-boot attempt had boots 1 and 2 pass but an intermittent boot
3 VFS write miss after valid ELF construction. It was retained as a failed
attempt rather than hidden; the repeated final three-boot command passed 3/3.

The live stack policy remains frame bound 576 bytes, transient bound 128 bytes,
activation cost 760 bytes, and runtime depth 75, as derived by
`native_elf_stack_policy.h` and exercised by the recursion/accounting proofs.

### Guest-generated ELF audit

The monitor retained artifacts emitted over serial by the guest compiler/linker
from the final fresh-boot run:

| artifact | bytes | SHA-256 | entry |
| --- | ---: | --- | --- |
| `t27main.elf` | 8226 | `99CEC03AD2CD2986960762E9347F61362F044BA77A85CB98BF98C5008CE31A33` | `0x10001000` |
| `t27glob.elf` | 8200 | `B90EC072EE338F5A60AABC4F891CAB9E97C00444FA40E1EE7237F18CC428AE5E` | `0x10001000` |

External `readelf -h -lW` validation reported ELF64 AMD64 for both. `t27main`
has `PT_LOAD` ranges `0x0000..0x1460` with `R E` and `0x2000..0x2022` with
`R`; it has no mutable globals. `t27glob` has `R E` code
`0x0000..0x1055` and `RW` mutable struct data `0x2000..0x2008`. Both entry
addresses lie in the executable segment, all file ranges are within the
artifact, and neither image has an RWX segment:

```text
phase27tv_readelf_audit=PASS
phase27tv_wx_artifact_audit=PASS
```

Raw `objdump` disassembly of `t27main.elf` confirmed actual generated field
operations. The `y` store uses `lea rax,[rax+0x4]` at `0x10001128`, then
`mov DWORD PTR [rdx],eax` at `0x100011e4`. The `x` and `y` loads in
`sum_point` use `lea rax,[rdx+0x0]`/`mov eax,DWORD PTR [rax]` and
`lea rax,[rdx+0x4]`/`mov eax,DWORD PTR [rax]` at `0x10001399`/`0x100013ba`
and `0x1000141f`/`0x10001440`. Before each `->` access, the image checks the
descriptor valid bit at `[rax+0x1c]`, the struct extent/stride at
`[rax+0x14]` and `[rax+0x10]`, then current versus base at `[rax]` and
`[rax+0x8]`, alignment, and bounds before applying the fixed offset:

```text
0x10001347  mov ecx,DWORD PTR [rax+0x1c]
0x1000134b  cmp ecx,0x1
0x10001357  mov ecx,DWORD PTR [rax+0x14]
0x1000135b  cmp ecx,0x8
0x10001367  mov r8d,DWORD PTR [rax+0x10]
0x1000136b  cmp r8d,0x8
0x10001378  mov rdx,QWORD PTR [rax+0x0]
0x1000137c  mov rcx,QWORD PTR [rax+0x8]
0x10001380  cmp rdx,rcx
0x10001395  mov rdx,QWORD PTR [rax+0x0]
0x10001399  lea rax,[rdx+0x0]
```

The field-subobject descriptor audit is consistent with the live backend
layout: `&p.x` stores the field address in both base and current, writes
extent 4, element size 4, provenance `FieldSubobject` (enum value 9), and a
valid bit. The guest `field_pointer` and adjacent-field failure/recovery
markers exercise those exact semantics; no permanent verbose tracing was left
behind:

```text
phase27tv_field_descriptor_audit=PASS
```

The guest Build path is self-hosting: Developer Studio calls the bare-metal
build service, which reads the staged sources through VFS, runs the guideXOS
parser/compiler, persists `.gxo`, links, writes NativeElf, validates it, and
the guest loader executes it. No clang, gcc, MSVC, lld, link.exe, or copied
host-built result participates in that in-guest build.

This closes the original proof gap:

```text
phase27t=PASS
phase27tv_qemu_started=PASS
phase27tv_readelf_audit=PASS
phase27tv_field_descriptor_audit=PASS
phase27tv_wx_artifact_audit=PASS
```

Physical hardware was not tested. The historical Phase 27H IDE issue and the
separate Developer Studio close/freeze issue remained outside this closure.

## Limits

Phase 27T remains AMD64-only and bounded. It does not add unions, bitfields,
packing controls, typedefs, classes, pointer fields, arrays of structs, array
fields, recursive by-value structs, heap allocation, function pointers,
general casts, pointer-returning functions, null-pointer semantics, or
struct-valued returns. Builds still perform a bounded internal relink; the
NativeElf runtime remains trusted. The separate Developer Studio close/freeze
issue is outside this phase.
