# Developer Studio Phase 28P — Debugger Hover / Data Tips

Phase 28P adds read-only source-editor data tips to the existing paused
debugger UI. The feature is intentionally narrow: it recognizes one scalar
identifier under the pointer, asks the existing debugger/evaluator contract
for the selected inspection frame, and renders a bounded popup. It does not
add an evaluator, write memory, set variables, or change the debugger ABI.

## Scope and semantics

- Tips are considered only while the debugger is paused and the editor is in
  the active source view.
- The lexical target is a single identifier. C/C++-style identifier starts are
  letters or `_`; continuation characters also include digits.
- Syntax spans reject comments, strings, character literals, preprocessor
  text, keywords, type keywords, and other non-code spans before the bounded
  lexical fallback is considered.
- The popup displays only signed integer and pointer results. Unknown,
  unavailable, stale, not-live, unsupported, dirty-source, and source-mismatch
  cases stay hidden.
- The selected inspection frame is part of the tip identity. The execution
  marker remains frame zero; selecting a caller changes the tip authority but
  does not move execution.
- The identity binds session generation, stop generation, selected frame,
  project, source path, document id/generation, line, token span, and name.
  Any run/stop, frame change, document edit, source switch, or pointer leave
  invalidates the popup.
- Popup placement is clamped to the editor rectangle. A 260x76 popup is used
  by the production shell and cannot escape the source-editor bounds.

## Implementation

The reusable model is in:

- `src/developer_studio_debug_tips.h`
- `src/developer_studio_debug_tips.cpp`
- `tests/debugger_data_tips_test.cpp`

The production wiring is in `src/main.cpp`. It reuses the already existing
`development_debug_evaluate_expression` path first. If that path reports that
the identifier is not evaluable, the fallback consumes only the current ABI
variable snapshot or the current controller variable view, and only when the
record is available, live, and value-valid for the same session/stop/frame.

The feature is linked into both host tests and the freestanding native ELF.
The SDK headers, compiler object ABI version, and debugger API version were not
changed.

## Host coverage

`debugger_data_tips_test` covers identifier extraction, underscore/digit
continuations, keyword rejection, comments/strings/chars/preprocessor
exclusion, bounded end-of-line probing, identity changes by stop/frame, popup
clamping, and invalidation. The existing debug editor, debugger, variables,
watches, stack, source-step, ABI-layout, and native ELF host tests were also
run through the normal standalone build path.

## Diagnostic proof path

The Phase 28P diagnostic is opt-in through
`/Apps/DeveloperStudio/.phase28p-diagnostic` containing
`guideXOS-phase28p`. It reuses the Phase 28O `ESP/P28O` fixture and adds
markers at the token, mapper, controller, evaluator, frame-selection, and
popup lifecycle boundaries. The focused server smoke entry point is:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-compiler-bootstrap.ps1 `
  -Phase28POnly -BootCount 1 -TimeoutSeconds 300
```

The script stages the package and sentinel into a fresh QEMU filesystem and
preserves every boot log under `%TEMP%\guidexos-phase28g-*`. Three independent
one-boot invocations were used for the final proof so that each QEMU guest had
its own filesystem/evidence directory. The harness runs with QEMU
`-display none`; it does not use physical mouse automation, but it calls the
production pointer-coordinate hit-test with the editor coordinates for the
target source token.

## Boundary finding and fix

The last failing trace was classified as boundary D, request to backend
evaluator. The earlier trace already showed:

- `DEVELOPER_STUDIO_PHASE28P_MAPPER_READY` and `TOKEN_HITTEST_PASS`;
- controller identity, session, stop, and selected-frame matches; and
- a real controller/backend response with `VARIABLE_NOT_LIVE`.

The diagnostic was probing `value` at the original frame-0 stop before the
assignment executed. That was an expected evaluator rejection for a non-live
variable, not a mapper, controller-routing, ABI, or evaluator implementation
defect. A temporary Step Over/breakpoint experiment was also rejected by the
existing stale-breakpoint-session contract and was not retained.

The final diagnostic uses a live frame-0 argument, `tail_input=28` at
`src/helper.cpp:3`, for the primary hover proof. The controller returns a
signed integer with matching session/stop/frame identity, and the existing
assertion emits `INITIAL_HOVER_PASS` and `HOVER_FRAME0_PASS` only after the
data-tip model contains the expected value and selected frame. The real UI
Call Stack keyboard path then selects frame 1 and hovers `value=14` at
`src/helper.cpp:11`, after assignment; `CALLER_HOVER_PASS` and
`LOCALS_WATCH_EQUIVALENCE_PASS` confirm the selected-frame result.

The source changes are diagnostic wiring and fixture sequencing only. Normal
production hover semantics, the debugger ABI, the evaluator contract, and
memory-safety restrictions are unchanged. The smoke harness now requires the
existing value-checked `HOVER_FRAME0_PASS` marker rather than adding a new
runtime marker that would alter the freestanding package layout.

The boundary markers are emitted by the in-guest Developer Studio process:

```text
MAPPER_CALL -> MAPPER_READY -> MAPPER_RETURN -> TOKEN_HITTEST_PASS
-> CONTROLLER_ACCEPTED -> CONTROLLER_CALL_OK -> CONTROLLER_STATUS_SUCCESS
-> CONTROLLER_KIND_INTEGER -> IDENTITY_MATCH -> SESSION_MATCH
-> STOP_MATCH -> FRAME_MATCH -> INITIAL_HOVER_PASS -> HOVER_FRAME0_PASS
-> TIP_INVALIDATION_PASS -> CALLER_HOVER_PASS
-> LOCALS_WATCH_EQUIVALENCE_PASS -> PHASE28O_REGRESSION_PASS -> CLEANUP_PASS -> PASS
```

The independent native evaluator tests additionally cover the same-name
fixture (`value=33` in frame 0 and `value=14` in frame 1), invalid/dead
variables, generation mismatch, and watch re-evaluation. The in-guest Phase 28P
assertion covers the live frame-0 argument and the caller-frame value; the
existing Phase 28O/N lifecycle markers cover running dismissal, generation
reset, terminal cleanup, and frame-change invalidation.

## Validation status

Repository provenance was preserved: the standalone validation began at
`a955a8a6d81d037a5170e883736815e50026c39e` on branch
`phase28p-debugger-data-tips`, with `main` still at
`33c37e56df6dd70e0963b2caca824e100f5e3d7e`; the server validation began at
`f3a48444ac364689f64ba760808dcb21b8d0a98a` on branch
`v0.5_DEVELOPER_STUDIO`. No merge, rebase, push, or history rewrite was used.

Standalone host/model and native packaging passed for both architectures. The
final sectionless ELF package audit was:

| image | size | SHA-256 |
|---|---:|---|
| AMD64 | 928392 bytes | `B1F8F41FFAC227DFF5A140B8C45D8B084EB88A10CC378A166AD9F356EC0EA2DD` |
| ARM64 | 1091376 bytes | `19AB79BED6854F98155506D16C6171B4EC2CDC58851F3966F0FD8BA1008075F2` |

Both `Apps/DeveloperStudio` and `ESP/Apps/DeveloperStudio` copies match these
final hashes. `readelf` reports ELF64, x86-64/AArch64 respectively, with zero
section headers.

Three fresh isolated final-package QEMU boots passed with the same AMD64 hash:

- `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-8a9756562d8b4855960dd914c6866f1a`
- `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-253c994c5a1f46d8a91c17505f47329e`
- `C:\Users\guideX\AppData\Local\Temp\guidexos-phase28g-5f76b4c7f3ba4866878e824227087aad`

Each `boot1.serial.log` contains the complete marker sequence above and
`Phase 28P debugger data-tip proof completed across 1 fresh boot(s).` Earlier
failed/stalled attempts remain preserved in their original evidence folders;
they were not counted as final passes.

Focused regressions passed: Developer Studio model/package tests, debugger
data-tip/editor/variables/watches/stack/source-step tests, native debugger
runtime, call-stack controller, source-step controller, breakpoint manager,
debug policy/output contracts, ABI layout, Native ELF runtime/validator/
development-App-Model/trampoline tests, and the native filesystem contract.

## Non-goals and limitations

- No arbitrary expressions, pointer dereferences, aggregates, memory reads,
  variable writes, or set-value UI are introduced.
- No tooltip is shown for lexical text that is not a supported scalar local or
  argument in the selected paused frame.
- A dirty buffer is treated as stale until the source identity is trustworthy.
- The current Phase 28P proof fixture uses the existing Phase 28O helper
  program and is deliberately diagnostic-only; it is not a general evaluator
  integration test.
