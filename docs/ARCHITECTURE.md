# guideXOS Developer Studio Phase 1 Architecture

Status: initial native shell

## Integration decision

Developer Studio is an independently maintained Native ELF application that is consumed by guideXOS Server's existing App Model. This follows the Pac-Man reference path:

1. the Developer Studio repository owns `app/app.json`, native source, runtime-neutral models, the build script, and smoke orchestration;
2. the build script uses the Server checkout's existing `sdk/include/guidexos` headers and direct LLVM/LLD convention;
3. the build output is staged as the ordinary Server app package `Apps/DeveloperStudio/app.json` plus `bin/amd64/developerstudio.elf`;
4. Server's existing `AppRegistry` scans the manifest, `DesktopService` registers it, and the registry-derived All Programs list exposes it; and
5. `DesktopService::LaunchApp` resolves the manifest's `NativeElf` strategy and hands it to the existing experimental hosted Native ELF pipeline.

No new plugin, extension, package, or loader architecture is introduced. The staged package is a normal App Model package, not a special Developer Studio integration path.

The Server checkout now also prefers its checkout-local `Apps` directory when that directory exists, falling back to the hosted `/Apps` root otherwise. This keeps the normal package layout testable from the documented local checkout without changing the production-style `/Apps` fallback or touching the separate `D:\Apps` volume.

## Reference audit

Pac-Man is the closest existing reference because it is a self-contained `Apps/PacMan` package with an `app.json`, an amd64 Native ELF entry, C ABI entry point `gx_main`, resource files, and the standard `NativeElf` launch route. HelloWorld and ResourceViewer provide the smaller SDK build convention and host-call examples. Built-in applications remain useful references for UI composition and App Model metadata, but Developer Studio is not a built-in application and is not added to `built_in_app_metadata.h`.

The active hosted path is native C++ and uses:

- `app_manifest_loader.cpp` and `app_manifest_validator.cpp` for `app.json`;
- `app_registry.cpp` for manifest discovery and identity lookup;
- `app_launch_resolver.cpp` for `NativeElf` strategy selection;
- `desktop_service.cpp` for registry-to-desktop registration and normal launch dispatch;
- `native_app_runtime.cpp` and the Native ELF pipeline for ABI/window/event hosting; and
- `compositor.cpp` for hosted window creation, retained draw operations, and registry-derived All Programs entries.

The existing hosted runtime requires the experimental build for actual Native ELF execution and currently proves only same-host amd64 static ET_EXEC images with `guidexos-c-abi-v1`.

## Repository boundary

Developer Studio repository:

- source and static shell UI;
- Workspace, Project, Target Profile, Capability, and maturity types;
- source manifest;
- LLVM/LLD staging build;
- model unit test and hosted integration smoke test;
- architecture and phase documentation.

guideXOS Server repository:

- existing SDK headers and Native ELF host ABI;
- existing manifest loader, registry, launch resolver, compositor, and runtime;
- staged `Apps/DeveloperStudio` package output;
- one small generic package-source fallback in `app_registry.cpp`.

The Developer Studio source does not copy Server runtime or SDK implementation files. It has no direct dependency on UI widgets beyond the public `guidexos/ui.h` ABI consumed by the native shell.

## Phase 1 domain boundary

The UI consumes small data-only domain records:

```text
Developer Studio UI
        ↓
Workspace / Project / TargetProfile / Capability
        ↓
future Build and Runner interfaces
        ↓
guideXOS SDK, toolchains, and execution environments
```

The current target profile is the one proven route only:

```text
id:          guidexos.amd64.hosted.native
architecture: amd64
ABI:         guidexos-c-abi-v1
machine:     Windows hosted guideXOS Server
SDK:         guideXOS Native SDK v1
toolchain:   LLVM/LLD Native ELF
runner:      guideXOS Hosted Native ELF Runtime
maturity:    experimental
```

It records available capabilities and maturity without introducing policy or architecture-selection machinery. Build integration is explicitly unavailable in the Phase 1 profile.

## Diagnostics and smoke proof

Existing Server markers prove manifest registration and dispatch, including the registered app line, launch-target resolution, and native launch pipeline messages. The native shell adds bounded host-log markers for application construction, main window creation, initial render, and clean close. It does not log per-frame state.

The smoke test covers manifest identity and fields, model invariants, registry presence, canonical launch resolution, no-auto-launch startup, normal display-name dispatch, window title/ownership through the native process/window diagnostics, shell markers, and clean close through the regular compositor close path.

## Deferred by design

There is no editable buffer, syntax highlighting, completion, language server, compiler invocation, build command, debugger, project wizard, visual designer, Git integration, website preview, game engine, package marketplace, extension loader, driver pipeline, container runner, remote deployer, .NET frontend, or multi-architecture farm. Future target profiles can add architecture, ABI, machine, SDK, toolchain, runner, capability, and maturity data without changing the UI into an architecture-string switchboard.
