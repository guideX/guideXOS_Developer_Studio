# guideXOS Developer Studio

guideXOS Developer Studio is the early native development-environment shell for guideXOS Server.

Phase 1 proves the smallest useful foundation:

- a manifest-driven Native ELF application with the canonical App Model ID `com.guidexos.developerstudio`;
- a native, read-only IDE shell with explorer, welcome document, diagnostics, and status regions;
- runtime-neutral Workspace, Project, Target Profile, Capability, and maturity models;
- a truthful AMD64 hosted guideXOS target profile; and
- deterministic model and hosted App Model smoke coverage.

This phase deliberately does not implement editing, compilation, debugging, language services, designers, packaging, or multi-architecture orchestration.

## Build

From this repository, with the guideXOS Server checkout at its documented sibling path:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

The script uses the Server checkout's existing Native SDK headers and stages the application package under `Server\Apps\DeveloperStudio`. It writes no generated binaries into this repository.

## Smoke test

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\smoke-developer-studio.ps1
```

The smoke test checks the manifest, model invariants, registry identity, target resolution, no-auto-launch startup behavior, native dispatch, window ownership/title, render markers, and clean close.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the integration audit, boundaries, and intentionally deferred work.
