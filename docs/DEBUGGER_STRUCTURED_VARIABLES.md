# Debugger Phase 10 — Structured Variables

Phase 10 adds bounded, read-only object inspection on top of the Phase 9
DWARF variable evaluator. It is a stopped-state snapshot; it does not add a
Watch expression parser or memory editing.

## Data flow

```text
selected frame -> DebugDwarfVariable root -> DebugDwarfValueNode
               -> DWARF type/member metadata -> validated target address
               -> owner-bound target memory read -> lazy child nodes
```

The mapper retains stable DIE offsets, parent indices, type references, byte
sizes, member offsets, array bounds, accessibility, declaration, and bit-field
metadata. A member identity is its parent type DIE plus its member DIE offset;
names are not identities.

## Supported types

The first bounded pass supports ordinary structures, classes represented by
ordinary data-member DIEs, unions as inactive-member listings, nested
aggregates, fixed-size arrays, pointers, qualifiers, typedefs, scalar members,
and recursive pointer types. The current Clang fixture emits DWARF v5
`DW_TAG_structure_type`, `DW_TAG_member`, `DW_TAG_array_type`,
`DW_TAG_subrange_type`, and `DW_TAG_pointer_type` records. Its member layout is
constant `DW_FORM_data1` `DW_AT_data_member_location`; array size is emitted as
`DW_AT_count` with `DW_FORM_data1`.

Member layout is never inferred from source order, padding, alignment, or ABI
rules. `memberAddress = aggregateAddress + DW_AT_data_member_location` is used
only after signed-offset, overflow, aggregate-size, member-size, canonical
address, and target-read validation.

## Lazy expansion and bounds

Inspection creates root nodes only. A disclosure action materializes children
for the selected stopped frame. The current hard limits are:

* maximum value depth: 8;
* maximum children per node: 16;
* maximum materialized nodes per view: 64;
* maximum array elements materialized per expansion: 16.

Excess children receive an explicit truncation diagnostic or a `<truncated>`
marker on the parent row when the per-node child slot is already full. Unsupported member
expressions, bit fields, incomplete types, malformed references, and register-
backed aggregates are represented as diagnostics such as `<unsupported layout>`,
`<incomplete type>`, or `<aggregate location unsupported>` rather than guessed
values. Aggregate and array byte ranges are checked before child addresses are
formed; unsigned address overflow is rejected rather than clamped.

## Pointers and arrays

Pointers remain scalar address values until explicitly expanded. Null pointers
display `nullptr`, are not read, and are not expandable. A non-null pointer is
required to have a canonical AMD64 address before it receives a disclosure
marker, then is probed through the existing
session/process/thread/stop-generation-bound memory callback. Failed probes
display `<unreadable>`.

Pointer expansion uses the pointed-to DWARF type as the target object base. A
pointer-to-structure shows its data members; a pointer-to-array shows bounded
elements; a pointer-to-scalar shows a `*` child. Pointers are never chased
implicitly. Array element addresses use the DWARF lower bound and element size,
with overflow checks; character arrays remain arrays and are not strings.

Recursive pointers are safe because the active path tracks target address plus
canonical type DIE. Re-entering the same pair produces `<cycle>`. The global
depth limit applies equally to nested aggregates and pointer dereferences.

## Snapshot ownership and stale state

Each value view and node carries session generation, stop generation, selected
frame identity, owner process/runtime/thread identity, and mapper artifact
generation. Expansion validates those values plus the mapper SHA-256 before
reading memory. Continue, stepping, frame changes, exit, and artifact
replacement clear or invalidate the view. Actual target values are never
cached across stops; only immutable DWARF metadata is reused for the exact
artifact.

Collapse retains the materialized children for the same stopped generation.
Re-expansion only reveals that snapshot again, so it does not reread target
memory or consume additional node budget. A new stop rebuilds roots and resets
the child tree.

## Current limitations

Not implemented: Watch expressions, general C++ expression evaluation, STL or
NatVis visualizers, strings, dynamic types, RTTI/downcasts, full inheritance,
virtual bases, optimized aggregate reconstruction, arbitrary location lists,
`DW_OP_piece` composites, bit-field editing, memory/register editing, static
member storage calculation, and automatic pointer chasing.

The Phase 10 model and real-DWARF memory harness are tested. Hosted UI proof
remains separate from that evidence because the existing hosted smoke can
stall before `debug_start`.
