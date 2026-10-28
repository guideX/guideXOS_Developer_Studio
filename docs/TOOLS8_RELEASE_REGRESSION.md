# TOOLS8 Release Regression Investigation

**Outcome: A — `TOOLS8_RELEASE_VERIFIED`.** The three Release-only failures were caused by disabled test assertions and fixture setup, not by optimization-sensitive defects in Developer Studio models. Release and Debug CTest pass with assertions active in the test executables. Guest acceptance remains separate.

## Repository state and provenance

| Repository | Branch | Starting HEAD |
| --- | --- | --- |
| Developer Studio Tools source | `DEVELOPER_STUDIO_TOOLS` | `508a8204f6fe99147c27b89e66fa0ff8eebd244e` |
| Server integration | `DEVELOPER_STUDIO_TOOLS` | `a13768dea9e5808baadb3c2908a8fca463505f16` |

The Server executable used for hosted UI validation is `D:\dev\guideXOS_DeveloperStudio_Tools\guideXOSServer.experimental.exe`, SHA256 `95905244D2685EA17F64E405591590C471CDF76B63804400FFEC81089C84FADC`. It was built from Server commit `a5613cdcdb9e4b954bef53264201beaaa21de1cc`. The only change from that Server commit to the supplied Server HEAD is `docs/TOOLS7_HOSTED_UI_QUALIFICATION.md`; Server code is unchanged. The executable and production Server ELF paths were left untouched.

Fresh build directories were used: `out/tools8-debug` and `out/tools8-release`. Existing build directories and qualification artifacts were preserved.

## Reproduction and root causes

Environment: Windows, CMake 4.3.1, Ninja 1.13.2, MinGW-w64 GNU C++ 15.2.0. Release test objects originally compiled with `-O3 -DNDEBUG`; Debug test objects compiled with `-g`.

The individual reproduction commands were:

```powershell
cmake -S . -B out/tools8-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/tools8-release --target developer_studio_file_workflow_test developer_studio_project_search_test developer_studio_debugger_conditional_breakpoints_test
ctest --test-dir out/tools8-release -R "^developer_studio_file_workflow_test$" --output-on-failure
ctest --test-dir out/tools8-release -R "^developer_studio_project_search_test$" --output-on-failure
ctest --test-dir out/tools8-release -R "^developer_studio_debugger_conditional_breakpoints_test$" --output-on-failure

cmake -S . -B out/tools8-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/tools8-debug --target developer_studio_file_workflow_test developer_studio_project_search_test developer_studio_debugger_conditional_breakpoints_test
```

Before the correction, each Release test failed in five of five repeated runs. Each test passed in five of five isolated Debug runs.

| Test | Original Release result | Evidence and cause | Ownership |
| --- | --- | --- | --- |
| `developer_studio_file_workflow_test` | 5/5 segmentation faults; CTest exit 8 | `assert(...)` removed workspace setup and operations. A symbolized optimized run stopped in `tests/workspace_file_workflow_test.cpp:262` while copying into `helperDirty.data`, which had never been initialized because its setup calls were inside assertions. | Tools test harness |
| `developer_studio_project_search_test` | 5/5 segmentation faults; CTest exit 8 | `assert(...)` removed project-search start/poll and result setup. A symbolized optimized run stopped in `tests/project_search_test.cpp:150` while opening a result that had not been produced. | Tools test harness |
| `developer_studio_debugger_conditional_breakpoints_test` | 5/5 assertion failures; CTest exit 8 | `BuildRequestFromProject(...)` at line 328 was called only inside `assert`. With `NDEBUG`, `validBuild()` returned a result with no populated build request; the still-active `TEST_CHECK` then failed at `DebugTargetFromBuild(...)` on line 403. | Tools test harness; no Debugging implementation change required |

These are harness failures rather than optimization-sensitive application defects: the missing operations are removed by `NDEBUG` before the optimized binary runs. No dangling pointer, ownership, integer, or production initialization defect was found in the model code on these paths.

### Baseline comparison and native debugging

The Release build at the TOOLS7 parent `4e6425249b304b5a5b3dfaf1598ab35e659e2491` reproduced all three failures in an isolated source snapshot. The affected tests and the relevant model/debugger sources are unchanged between that parent and the TOOLS7 source HEAD. The conditional-breakpoint baseline used its existing generated, ignored DWARF fixture ELF.

GDB was available. With symbols added to a separate `-O3 -DNDEBUG` baseline build, it located the two crashes at the test source lines above; because optimization inlined the test body, each stack contained only `main()`. The debugger test printed its exact failing `TEST_CHECK` expression and source line. MinGW supplied neither the AddressSanitizer nor UndefinedBehaviorSanitizer runtime libraries, so sanitizer runs were unavailable in this environment.

## Correction

`CMakeLists.txt` now undefines `NDEBUG` for every `developer_studio_*_test` executable (`-UNDEBUG`, or `/UNDEBUG` for MSVC). CMake’s normal Release optimization and `NDEBUG` remain in effect for `developer_studio_models` and other production targets. The Release compile command confirms the test-only order `-O3 -DNDEBUG ... -UNDEBUG`.

This keeps existing assertions and their checks/fixture setup active in Release instead of weakening or filtering tests. No production behavior, Debugging subsystem, Compilation subsystem, Server filesystem, or Server ABI was changed.

After the correction, each formerly failing Release test passed five of five repeated runs. The full Release and Debug suites also passed with assertions active.

## Regression matrix

Debug build and CTest:

```powershell
cmake -S . -B out/tools8-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/tools8-debug
ctest --test-dir out/tools8-debug --output-on-failure
```

Result: **38/38 passed**.

Release build with the real hosted Server UI integration test enabled:

```powershell
cmake -S . -B out/tools8-release -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DGUIDEXOS_ENABLE_TASK_LIST_HOSTED_UI_TEST=ON `
  -DGUIDEXOS_TASK_LIST_UI_SERVER_ROOT=D:/dev/guideXOS_DeveloperStudio_Tools `
  -DGUIDEXOS_TASK_LIST_UI_APP_PACKAGE=D:/dev/guideXOS_DeveloperStudio_Tools_Source/out/validation/tools8-release-regression/Apps/DeveloperStudio
cmake --build out/tools8-release
ctest --test-dir out/tools8-release --output-on-failure
```

Result: **39/39 passed**, including the 38 model tests and the hosted UI integration test (129.28 seconds). Before enabling the hosted test, the complete Release model suite also passed **38/38**.

The full suite covers File workflow, Project Search/Find in Files, Go to File/Line models, Replace preview, Build/Run, Task List, and debugger models including conditional breakpoints. The hosted integration log confirms actual Server/compositor dispatch, Developer Studio initial render and clean close, Task List scans/navigation, and opening Find in Files (`Ctrl+Shift+F`), Go to File (`Ctrl+P`), Go to Line (`Ctrl+G`), and Replace preview (`Ctrl+Shift+H`). It records the expected restriction `replace_preview_apply=DISABLED reason=atomic_replace_and_durable_sync_unavailable`.

Key generated evidence:

- `out/tools8-release/tools8-release-hosted-full-ctest.log`
- `out/tools8-debug/tools8-full-ctest.log`
- `out/tools8-release/tools8-fixed-release-repro.log`
- `out/tools8-release/tools7-hosted-ui-evidence/task-list-hosted-ui.log`
- `out/tools8-release/tools8-original-release-repro.log` (pre-correction)
- `out/tools8-parent-release/tools8-parent-release-repro.log` (pre-TOOLS7 baseline)

## Staged package audit

Both packages were rebuilt into the isolated TOOLS8 staging root with `build.ps1`; no production Server ELF was replaced. The audit command was:

```powershell
.\build.ps1 -ServerRoot D:\dev\guideXOS_DeveloperStudio_Tools `
  -StagingRoot D:\dev\guideXOS_DeveloperStudio_Tools_Source\out\validation\tools8-release-regression `
  -BuildRoot D:\dev\guideXOS_DeveloperStudio_Tools_Source\out\tools8-package-amd64 `
  -TargetArchitecture amd64
.\build.ps1 -ServerRoot D:\dev\guideXOS_DeveloperStudio_Tools `
  -StagingRoot D:\dev\guideXOS_DeveloperStudio_Tools_Source\out\validation\tools8-release-regression `
  -BuildRoot D:\dev\guideXOS_DeveloperStudio_Tools_Source\out\tools8-package-arm64 `
  -TargetArchitecture arm64

pwsh -NoProfile -File .\tests\validate-developer-studio-package.ps1 `
  -ServerRoot D:\dev\guideXOS_DeveloperStudio_Tools `
  -PackageRoot D:\dev\guideXOS_DeveloperStudio_Tools_Source\out\validation\tools8-release-regression\Apps\DeveloperStudio
```

The package contains exactly `app.json`, `bin/amd64/developerstudio.elf`, and `bin/arm64/developerstudio.elf`. The manifest identifies `com.guidexos.developerstudio`, schema version 1, kind `NativeElf`, supported architectures `amd64` and `arm64`; each entry uses `gx_main`, ABI `guidexos-c-abi-v1`, and runtime `native-elf`. Its declared permissions remain `log`, `window`, `draw`, `filesystem.read`, and `filesystem.write`.

| Architecture | Size | ELF validation | Entry point | SHA256 |
| --- | ---: | --- | --- | --- |
| AMD64 | 1,217,172 bytes | ELF64 little-endian, ET_EXEC; no section metadata or debug sections | `0x500cda10` | `265BA3F5E16470674CFD36CC499E5BC1D7FD12EB00299CCDBFC935E585DF97F9` |
| ARM64 | 1,396,548 bytes | ELF64 little-endian, ET_EXEC; no section metadata or debug sections | `0x500ef170` | `42E98813E65C5060A6FDCDED9B1F179F8F2AA755009E21D520D2E66DE521D0B7` |

Package manifest/content audit: **PASS**. The built ELFs match the prior TOOLS7 hashes because the correction changes only the host test-target compile options.

## Remaining acceptance

Guest acceptance is not part of this host Release regression closeout. QEMU was not started, and no guest session was launched. Guest installation and interaction remain a separate qualification. No cross-track or environment blocker remains for the host Debug/Release regression objective.
