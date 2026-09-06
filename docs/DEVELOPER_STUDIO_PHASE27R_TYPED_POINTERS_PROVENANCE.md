# Developer Studio Phase 27R: typed pointers and bounded provenance

Phase 27R adds a deliberately small, provenance-carrying `int*` model to the
bootstrap language. The implementation is split between the Server compiler,
linker/runtime, and the Developer Studio proof app.

## Supported language

The type grammar is only `int` and `int*`:

```cpp
int value = 40;
int* p = &value;
*p = *p + 2;
```

Supported addressable expressions are scalar locals, scalar globals, and
indexed elements of fixed integer arrays: `&value`, `&globalValue`, and
`&values[index]`. Pointer locals and parameters may be initialized, copied,
assigned, passed between functions, loaded through, and stored through.

The following remain outside Phase 27R: `void*`, function pointers, pointer
arrays, pointer-to-pointer types, pointer arithmetic/subtraction/comparison,
integer/address casts, raw address literals, heap allocation, structs, and
pointer return values. Bare arrays do not decay to pointers; an index is
required. Global pointer variables and pointer arrays are rejected.

## Runtime representation

Each pointer local is a 32-byte, 8-byte-aligned descriptor:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | current address (`uint64_t`) |
| 8 | 8 | provenance base address (`uint64_t`) |
| 16 | 4 | accessible extent in bytes (`uint32_t`) |
| 20 | 4 | pointee size (`uint32_t`, currently 4) |
| 24 | 4 | provenance kind |
| 28 | 4 | flags (`1` means valid) |

Pointers are never lowered to plain 32-bit integers. Pointer parameters are
copied into a callee-owned descriptor, and pointer arguments use caller-owned
temporary descriptors, so metadata remains available across calls and
recursion. The frame layout reserves aligned descriptor slots, pointer
temporaries, and validation scratch storage; oversized legal frames are
rejected by the existing activation policy.

Provenance kinds distinguish local scalars, global scalars, local/global array
elements, and pointer parameters. The current Phase 27R array-element lowering
uses a one-element bounded descriptor (`base == current`, `extent == 4`), which
is safe and intentionally does not imply pointer arithmetic.

Before every indirect load/store, NativeElf validates: non-null current/base,
valid flags, 4-byte pointee size, nonzero extent, current-at-or-after-base,
4-byte alignment, and `current - base <= extent - 4`. Failure reports runtime
status `InvalidPointerDereference`; the Developer Studio run layer exposes this
as `invalid_pointer_dereference`.

An uninitialized pointer local is a zeroed invalid descriptor. It is legal to
declare, but dereferencing it fails safely at runtime and never performs an
unchecked memory access. This is the selected Phase 27R policy for the
otherwise implementation-defined null/default-pointer case.

## ABI, objects, and incremental builds

Pointer parameter kinds are part of function declarations, exports, imports,
call sites, and linker signature checks. A pointer/int mismatch is rejected as
a conflicting function declaration. The `.gxo` object format records the
parameter-kind bytes and uses compiler object ABI version 3, so pre-Phase-27R
objects are rejected and rebuilt. Object serialization is deterministic and
incremental builds preserve valid unchanged pointer-aware modules while
recompiling edited or corrupted objects.

## Developer Studio proof

The proof project is staged at `/P27R` and the NativeElf proof app is
`/Apps/DS27R`. Its build script is
[`scripts/build-phase27r.ps1`](../scripts/build-phase27r.ps1), and the fixture
project is under the Server repository’s
[`scripts/fixtures/phase27r`](../../guideXOSServerV0.5_DEVELOPER_STUDIO/scripts/fixtures/phase27r).

The IDE proof opens all three translation units, runs a cold build, repeats a
warm cached build, edits only the pointer-parameter module, exercises a safe
invalid-pointer runtime failure, verifies recovery, and verifies a pointer
signature failure blocks execution and then recovers. The Server smoke harness
also checks local/global/array-element pointers, dynamic indexing, copy and
assignment, recursion, relocation, descriptor opcodes, deterministic object
round-trips, stale-object invalidation, runtime cleanup, and kernel survival.

Run the focused proof from the Server repository with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-compiler-bootstrap.ps1 -Phase27ROnly
```

The focused harness emits `phase27r=PASS` and
`ELF Loader: Phase 27R typed pointer smoke PASS` only after the compiler,
runtime, object/linker, IDE build/run, and recovery checks succeed.
