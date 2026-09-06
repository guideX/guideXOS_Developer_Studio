# Developer Studio Phase 27S — Provenance-Preserving Pointer Arithmetic

Phase 27S extends the Phase 27R typed `int*` descriptor model so pointers can
traverse bounded integer arrays without becoming arbitrary raw addresses.

## Semantics

The only pointer type in this phase is `int*`, whose element size is four
bytes. The supported additive forms are `int + int`, `int - int`, `int* +
int`, `int + int*`, and `int* - int`. Pointer-plus-pointer, multiplication,
division, casts, and pointer-distance subtraction remain invalid.

Pointer arithmetic scales a signed 32-bit element offset by four, checks the
scaled result and address addition for overflow, and requires the new current
address to be within `[base, base + extent]`. The upper endpoint is the
representable one-past position; it is never dereferenceable.

## Descriptor and provenance

Each descriptor remains 32 bytes and eight-byte aligned:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 8 | current address |
| 8 | 8 | provenance base |
| 16 | 4 | extent in bytes |
| 20 | 4 | element size (`4`) |
| 24 | 4 | provenance kind/storage identity |
| 28 | 4 | validity flags |

`&array[index]` records the array base and full array extent, not an extent
starting at the selected element. Controlled decay of a bare `int[N]` name
produces the same descriptor as `&array[0]`. Descriptor copies are value
copies, so advancing a callee parameter or a copied local does not mutate the
caller’s current position. Equality compares current, base, extent, element
size, provenance kind, and validity flags; unrelated objects therefore do not
become equal merely because storage is adjacent.

## Backend and runtime safety

The AMD64 backend emits sign extension, four-byte scaling, signed overflow
guards, carry/underflow checks, and unsigned base/one-past comparisons. A
descriptor is revalidated before arithmetic, and the existing dereference guard
still requires `current < base + extent - elementSize`, rejecting one-past.
Pointer arithmetic failures use `NativeRuntimeStatus::PointerOutOfBounds`, SDK
error `GX_DEVELOPMENT_RUN_ERROR_POINTER_OUT_OF_BOUNDS`, and the diagnostic:

`ELF Loader: Application terminated: pointer arithmetic moved outside its object.`

The status is reset by the normal NativeElf teardown path, so a failed run can
be followed by a valid run in the same session. W^X segment policy remains
unchanged: code is R-X, read-only data is R--, and mutable data is RW-.

## Persistence and Developer Studio

The `.gxo` compiler/object ABI is version 4 so cached objects from the prior
pointer semantics are rejected instead of being interpreted under new rules.
Function import/export signatures continue to encode `int` versus `int*`.
Source-hash validation, checksum validation, incremental recompilation, and
full relinking remain active for pointer projects and cross-file global-array
relocations.

The Phase 27S fixture is a three-file Developer Studio project:

* `state.cpp` owns `int values[4]`.
* `math.cpp` fills the global array and walks it through `sum_pointer(int*)`.
* `main.cpp` calls the filler and walker and logs “Pointer traversal completed.”

The focused smoke performs cold, warm, partial-edit, pointer-failure,
signature-failure, recovery, and NativeElf artifact checks. Host tests cover
one-past, before-beginning, beyond-one-past, scalar and array extents,
middle-element provenance, descriptor copies, pointer parameters, recursion,
adjacent-object isolation, arithmetic overflow, equality, `.gxo` round trips,
cache invalidation, and deterministic outputs.

## Limits and exclusions

Phase 27S remains AMD64-only, uses bounded arrays and bounded recursion, and
supports only `int*`. It does not add unrestricted pointers, integer-pointer
casts, `void*`, `char*`, function pointers, pointer-returning functions,
pointer arrays, multidimensional arrays, structs, field addressing, heap
allocation, or general C casts. The Developer Studio close/freeze issue from
earlier phases is a separate known issue and is not changed here.

> Phase 27S does not turn pointers into arbitrary raw addresses. Pointer
> arithmetic remains constrained by the provenance and extent of the
> originating object.

The recommended next bounded phase is **structs and field addressing**.
