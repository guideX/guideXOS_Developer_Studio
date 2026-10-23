# Developer Studio COMP3R1 — Compiler Recursion Regression Follow-up

**Outcome A — COMP3R1 stabilization accepted.** The struct-pointer recursion failure was a stale test expectation after an earlier, intentional stack-policy increase. The compiler's runtime recursion guard remains bounded and works at the current limit. COMP3's separate guest-responsiveness dependency remains open.

## Stale-prompt and repository gate

- Studio started on `main` at `bce64a0acc39b0cbaa4e1898f49d4e165520e7e1`, matching the requested COMP3 HEAD; its tracked worktree was clean and it was four commits ahead of `origin/main`.
- Server/compiler started on `v0.5_DEVELOPER_STUDIO` at `5718de5728e9589887242336e79a35529877542e`, matching the requested COMP3 HEAD; its tracked worktree was clean and it was two commits ahead of `origin/v0.5_DEVELOPER_STUDIO`.
- The COMP3 report was read. No COMP3R1 report/result or authoritative `.phase` marker existed. A prior report mentions a Phase 28Q diagnostic fixture marker; that is not a phase marker and is absent from the current Server checkout.
- COMP1 and COMP2 remain Outcome B with guest qualification at 0/3. Phase 30B remains Outcome B at 0/5, and its latest report requires an exclusive shared QEMU reservation before another run. COMP3 remains Outcome B, `COMP3_GUEST_RESPONSIVENESS_DEPENDENCY`.
- The Server's preexisting `out/comp2-validation/`, `out/comp3-validation/`, and `out/validation/` artifacts were preserved. Reproduction and validation output is isolated under `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\out\comp3r1-validation\`.
- No branch or worktree was created or switched. Server v0.3 was not read or changed. No QAC reservation was touched and no QEMU was launched.

## Original failure and reproduction

The test compiles source in memory, emits AMD64 code, and invokes it from the host test process. Its failing input was:

```c
struct Point { int x; };
int recurse(struct Point* p, int n) {
    if (n == 0) return p->x;
    return recurse(p, n - 1);
}
int gx_main(gx_app_context* ctx) {
    struct Point p;
    p.x = 42;
    return recurse(&p, 76);
}
```

The original assertion expected the call to return `0` with low status `COMPILER_RUNTIME_STATUS_CALL_DEPTH` (`1`). The actual parser and code generator succeeded; execution returned `42` with status `0`. No compiler diagnostic or linker diagnostic was produced. The host test printed `FAIL: struct pointer recursion guard is preserved` and exited `1`.

The original host-test source is identified by Git blob `b48891b146be4245ff91c22c66a0180352925abd`; its preserved snapshot has SHA-256 `996843E5ED47C0B1AC2CD10AF40BF9177EF464E619D2997970163715CAD8ABE9`. The reproduced executable is `out/comp3r1-validation/compiler-structs-host-test.exe`, SHA-256 `875C7992D0FFEF2DC3DD5A1E7519239988E70D18BBC5AE1679F0CC4A656A3117`.

Reproduction command:

```powershell
powershell -File D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\out\comp3r1-validation\replay-original-failure.ps1
```

That script uses the preserved original test source and current compiler sources, writes a separate replay executable, and confirms the same assertion failure. The underlying build command used `C:\mingw64\bin\g++.exe` 15.2.0, target `x86_64-w64-mingw32`, on Windows 11 Pro build 26200, with `-std=c++14 -Wall -Wextra -O2 -DGXOS_BARE_METAL` and the same source list as `run-compiler-structs-host-test.ps1`. Build exit was `0`; test exit was `1`.

The observed result was `value=42 status=0 low_status=0`, versus the obsolete expected `value=0 low_status=1`. This is a runtime assertion mismatch, not an error-code, category, message, source-location, or output-format mismatch. There was no generated object or ELF for this in-memory fixture; the retained host-test executable and logs are the artifacts.

## History and root cause

The test was added in commit `5d9d34f76023058b12b18e4a4a9e1b8b757ac4e` on 2026-09-06. At that point the shared stack policy used a 64 KiB NativeElf application stack and `COMPILER_MAX_RUNTIME_CALL_DEPTH == 75`; a fixed recursive request of 76 correctly exceeded the limit.

Commit `b52e48a7d3e59cc61998c58e3041b62ad477e6c3` on 2026-09-14 increased the application stack to 512 KiB and the derived call-depth limit from 75 to 679. The test kept its literal `76`, so it moved from beyond the old bound to a valid request under the new policy. COMP3's Server commit `5718de5728e9589887242336e79a35529877542e` changed compiler-driver and build-service diagnostics; the recursion test, backend guard, and stack-policy files are identical to its parent.

**Classification:** stale test expectation caused by the pre-COMP3 stack-policy increase. This was not introduced by COMP3, not a compiler error-classification issue, and not a weakening of recursion safety.

## Recursion and pointer semantics

The failing test covers **recursive function calls through a `struct Point*` parameter**. It does not cover a recursive struct type. Phase 27M permits direct and mutual recursive functions under the runtime bound. The AMD64 backend initializes the generated activation depth to one, checks the shared limit before each source-defined call, and on overflow returns zero while preserving a sticky call-depth status and the limit in the upper status bits.

This compiler's struct parser only accepts `int` fields. It does not support struct pointer members, directly embedded struct fields, incomplete struct declarations, or self/mutually recursive struct types. Those constructs remain rejected. Ordinary structs, pointer parameters, pointer-to-field access with provenance checks, and recursive functions carrying a struct pointer remain supported within the existing subset.

## Focused repair and regression coverage

Only the host test and its runner changed; no compiler, linker, parser, IR, ABI, or kernel production source changed.

- `tests/compiler_structs_host_test.cpp` now imports the shared stack policy and derives its boundary inputs from `COMPILER_MAX_RUNTIME_CALL_DEPTH`.
- The former 76-call case now verifies successful return `42` and status `0`.
- A request of `limit - 2` reaches the maximum activation depth and succeeds with `42/status 0`.
- A request of `limit - 1` asks for one more activation and returns `0` with status `173825` (`0x0002A701`: depth 679, call-depth status 1).
- A valid struct-pointer recursion succeeds after that overflow result. A valid struct also compiles after rejected recursive/incomplete type declarations.
- The parser tests retain explicit rejection coverage for self-referential pointer members, directly embedded recursive fields, mutual recursive pointer-member declarations, and incomplete struct declarations. Existing tests cover valid ordinary structs, pointer parameters, field pointers, pointer provenance, and pointer type mismatches.
- The first host attempt at the maximum test hit Windows `STATUS_STACK_OVERFLOW` (`0xC00000FD`) with the executable's default stack reserve. The structs host runner now links its **host test executable** with an 8 MiB stack reserve so it can execute the full policy boundary. This does not change the 512 KiB NativeElf stack or the generated-code limit.

The broader requested regression cases map to the implemented subset as follows:

| Scenario | Validation |
|---|---|
| Ordinary struct and supported pointer use | Existing struct layout, struct-pointer parameter/arrow access, and pointer-to-field provenance checks pass. |
| Self-recursive pointer form | Recursive functions receiving a valid struct pointer are supported and tested. Recursive struct pointer members are outside this subset and are explicitly rejected. |
| Direct and mutual recursive struct fields; incomplete declarations | Explicit parser rejection assertions pass. Mutual recursive **functions** remain covered by the compiler-functions/Phase 27M tests. |
| Unsupported pointer construct | Existing invalid arrow, pointer provenance, and type-mismatch checks pass. |
| Maximum depth and excessive recursion | At the current limit, value/status are `42/0`; one activation beyond returns `0/173825`. |
| Error classification and source location | The compiler-driver diagnostic host test passes its compiler/linker classification and mapped source-location checks. |
| Bounded memory and recovery | Fixed-size test buffers and the generated depth guard bound execution; successful compilation/execution follows both a depth failure and rejected type inputs. |

An instrumented copy of the final test recorded `boundary value=42 status=0 limit=679` and `beyond_boundary value=0 status=173825 low_status=1 depth=679`, then exited `0`.

## Validation results

The complete Server compiler host group passed **9/9** on the final test source:

| Script | Result |
|---|---|
| `run-compiler-arrays-host-test.ps1` | PASS |
| `run-compiler-bootstrap-host-test.ps1` | PASS |
| `run-compiler-functions-host-test.ps1` | PASS |
| `run-compiler-globals-host-test.ps1` | PASS |
| `run-compiler-multifile-host-test.ps1` | PASS |
| `run-compiler-object-host-test.ps1` | PASS |
| `run-compiler-pointers-host-test.ps1` | PASS |
| `run-compiler-struct-arrays-host-test.ps1` | PASS |
| `run-compiler-structs-host-test.ps1` | PASS |

Affected follow-up host checks also passed:

- `run-compiler-driver-diagnostics-host-test.ps1`: PASS; compiler/linker classification, source mapping, cache, and artifact-stage checks.
- `run-native-build-service-responsiveness-test.ps1`: PASS; production hosted worker start, output, admission, and completion.
- Object and multi-file compiler/linker scripts: PASS as part of the 9/9 group.
- Studio `cmake --build build/comp2-host --config Debug`: PASS, no work required.
- Studio `ctest --test-dir build/comp2-host --output-on-failure`: **34/34 PASS**. This retains COMP3 coverage for build result distinctions, Build output, Problems navigation, source membership, artifact identity, failed-build Run rejection, Save All gating, and recovery.

No COMP3R1 production object required a freestanding rebuild. COMP3's previously qualified `compiler_driver.cpp` and `compiler_build_service.cpp` AMD64 freestanding objects remain applicable because those production files are unchanged; the earlier COMP3 report records them as passing. This is not a full kernel build. No shared GX ABI or host-call layout changed.

Validation logs, scripts, binaries, and CSV results are retained at `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO\out\comp3r1-validation\`. The original failure logs and binary were preserved separately from the final passing runs.

## Outcome and remaining qualification

COMP3R1 is **Outcome A — stabilization accepted**: the cause is established, the correct bounded behavior passes at and beyond the current policy boundary, all nine Server compiler scripts pass, Studio CTest passes, and the affected hosted regressions pass.

This accepts only the compiler regression follow-up. COMP3 guest responsiveness remains `GUEST_BUILD_RESPONSIVENESS_NOT_SUPPORTED`; `BareMetalBuildService::start` still compiles synchronously under the calling stack. No asynchronous guest work, scheduler change, cancellation change, or QEMU run was introduced. Phase 30B remains 0/5, COMP1 0/3, and COMP2 0/3. A qualified shared QEMU reservation and guest acceptance remain outstanding.

## Commit and next scope

- Server test/runner commit: `0e8e7e343129d44d5c1bddd422dbb5a290879b0d` on `v0.5_DEVELOPER_STUDIO`.
- Studio report is committed on `main`; it is the only COMP3R1 Studio change.
- No push was attempted. Server is three commits ahead and zero behind its upstream; Studio is expected to be five ahead and zero behind after this report commit.
- Server's final tracked worktree is clean; the preexisting `out/comp2-validation/` and `out/comp3-validation/` and new `out/comp3r1-validation/` remain untracked evidence. Studio is clean after the report commit.

Recommended next compiler scope, if separately authorized: keep runtime-bound tests derived from the shared policy, and define bounded semantics for incomplete and recursive struct pointer members before implementing any such language expansion. Do not start COMP4, Phase 30C, or asynchronous guest-build work automatically.
