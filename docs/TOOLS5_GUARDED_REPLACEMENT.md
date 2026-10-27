# TOOLS5 — Guarded File Replacement and Write-Safety Qualification

## Qualification result

**TOOLS5_STORAGE_BLOCKER.** The existing preview remains read-only. The
Developer Studio workspace contract cannot safely replace one selected file
on either hosted storage or the Native ELF filesystem. Apply Selected File is
therefore disabled, and there is no Replace All operation. The panel displays
the reason. No write fallback is attempted.

TOOLS4's preview remains available through Tools and `Ctrl+Shift+H`. Its
complete-plan and fingerprint checks are useful preview safeguards, but they
do not provide a safe commit primitive.

## Storage contract audit

| Requirement | Hosted workspace | Native ELF workspace |
|---|---|---|
| Read and stat | `file_stat` returns kind and size; `file_read_workspace` reads the whole bounded file. | `bare_metal_file_stat` returns kind and size; `bare_metal_file_read_workspace` reads through VFS. |
| Existing write | `file_write_all` opens the destination using binary truncate mode, writes, calls C++ stream `flush`, then reports the requested byte count when the stream is good. | `bare_metal_file_write_all` calls `vfs::write_file`. For an existing FAT-backed file this writes sectors into the current cluster chain and updates directory size after writing. |
| Temp-file creation | The app can ask `file_write_all` to create a path, but cannot create it exclusively or reserve a unique name atomically. The same call can truncate an existing path. | `write_file` can create a missing file, but there is no app-visible exclusive-create operation or owned-temp handle. |
| Rename / replace | The app ABI exposes no rename or replacement call. The host's general filesystem facilities are not exposed to the app. | `vfs::rename` is not exposed to the app ABI. The kernel VFS implementation dispatches rename only for its FAT32 filesystem type; its helper does not implement an atomic replace-existing operation. |
| Metadata | App-visible stat has no timestamps, permissions, or metadata-copy operation. | Although lower filesystem structures contain some metadata, the app-visible stat result contains only type and size, and no metadata-copy operation is exposed. |
| Flush / durability | C++ stream `flush` reports stream errors; no file-handle synchronization or directory synchronization is exposed. It does not establish crash durability. | The app-visible write returns a byte count. VFS file `flush` is a no-op because it has no buffering; the workspace ABI exposes no file or directory synchronization contract. |
| Lock / conditional commit | None. | None. |
| Failure recovery | Direct truncate/write may leave the existing file truncated or partially written. No before-image or journal is maintained by the workspace API. | Sector writes can fail after earlier sectors changed. There is no rollback or recovery record for workspace writes. |

The implementation evidence was inspected read-only in the Server integration
checkout at `a5613cdcdb9e4b954bef53264201beaaa21de1cc`. The relevant contracts
are `sdk/include/guidexos/abi.h`, `native_app_runtime.cpp`'s
`hostFileWriteAll`, `kernel/core/native_elf/native_elf_loader.cpp`'s
`host_bare_file_write_all`, `kernel/core/vfs.cpp`'s `write_file`, `rename`,
and `flush`, and `kernel/core/fs_fat.cpp`'s `overwrite_path` and
`rename_path`. No Server integration files were modified.

### Editor and plan state

The editor tracks `Document.buffer.dirty` and a buffer generation. The existing
save callback writes the complete buffer through `WorkspaceFileSystem.write`
and preserves the dirty flag if the callback fails. That callback inherits the
workspace's direct-write behavior; it is not a safe replacement API.

Replace preview can read a dirty open document from its captured buffer and
revalidate its document ID, generation, path, and content fingerprint. Disk
files are re-statted and reread, and an open dirty document blocks validation
of a disk-backed plan. Project identity and generation, scan completeness, and
the stored replacement text are also retained in the preview. These checks
detect many stale plans, but a 64-bit FNV-1a fingerprint is not a cryptographic
integrity proof, and there is no lock or conditional replacement to close the
race after revalidation.

An external commit would also need to invalidate or refresh any clean open
document for that path. There is no established workspace API for safely
reloading such a buffer after an external replacement. Dirty editor documents
must be rejected; silently saving or replacing their buffers is not supported.

The preview scans bytes. The scanner filters NUL-containing binary-looking
files, but it does not validate or transcode encodings. No Apply path exists to
preserve supported encoding markers or reject unsupported transformations.

## Capability decision and UI

The Replace in Files panel displays **Apply disabled** and explains that the
workspace APIs lack atomic replacement and durable synchronization. This
applies to hosted and Native ELF builds. Match rows remain preview-only and
there is no selection/apply action while the necessary commit capability is
absent. `Ctrl+Shift+H` and the Tools menu entry continue to open the preview.

The gate is intentionally closed for both environments. Hosting the app on a
Windows or POSIX machine is not sufficient evidence that the app can use the
host OS's rename semantics: its only workspace access is through the limited
Server ABI. Likewise, the fact that Native VFS has a rename helper does not
make it an atomic replace primitive available to Native ELF applications.

### Preconditions a future Apply capability must prove

An Apply implementation needs an app-visible capability with documented
semantics for all of the following:

1. Complete, non-truncated plan and one explicitly selected affected file.
2. Active project identity and root containment at commit time.
3. Immediate source-byte and file-identity revalidation, with an atomic
   conditional commit or a lock that closes the TOCTOU window.
4. Rejection of dirty open documents and a defined clean-buffer invalidation
   path after commit.
5. Same-directory, uniquely owned temporary creation; full-write checking;
   required data and directory synchronization; and verified atomic
   replacement of the existing target.
6. A defined metadata policy and byte-preserving transformation that retains
   all source bytes outside replacement ranges, including line endings and
   supported encoding markers.
7. Post-commit content/metadata verification and bounded evidence for recovery
   if verification fails.

Atomic rename alone must not be described as full crash durability. A
single-file commit must not be described as a multi-file transaction. The
operation must never touch a second file after the selected file fails.

## Failure handling and recovery limits

No guarded write was attempted in TOOLS5 because the current contracts cannot
protect the original. Consequently, partial-write, storage-exhaustion, flush,
commit, post-commit-verification, and temp-cleanup fault injection cannot
qualify a real Apply path yet. The existing editor save reports direct-write
failure and keeps its buffer dirty; that does not prove disk preservation.
Existing F2 rename undo is operation-specific and is not a durable write-ahead
record. Project-generator cleanup is likewise not a replacement recovery
mechanism.

The required future recovery policy is per-file and bounded: retain enough
before-image or journal evidence to identify the original and proposed bytes,
report whether commit occurred, preserve the original on every pre-commit
failure, and report an uncertain state when post-commit verification fails.
It must not modify another file or claim multi-file all-or-nothing behavior.

## Tests and package evidence

The current `replace_preview_test` uses disposable fixtures and proves that
preview leaves LF and CRLF source files unchanged, retains the expected
matches, supports empty replacement previews, and never invokes its write
callback. Existing TOOLS4 test evidence is documented in
`TOOLS4_REPLACE_PREVIEW.md`. TOOLS5 has no successful-Apply or write-fault
injection result because no safe storage primitive is available.

The clean Debug CMake build completed 108/108 steps. CTest passed 37/37
suites, including Replace Preview, Find in Files, navigation, Build/Run,
editor, and debugger coverage. The package build scripts also completed their
hosted model checks for both targets. These qualify the existing behavior and
package artifacts; they do not qualify file replacement.

Both target packages were staged under `out/tools5-package` without replacing
production Server ELFs. The combined package content audit passed and found
exactly `app.json` plus the AMD64 and ARM64 ELF files. `readelf -h` identified
both as little-endian ELF64 ET_EXEC files with the expected machine and no
section metadata.

| Architecture | Staged ELF | Size | SHA-256 |
|---|---|---:|---|
| AMD64 | `out/tools5-package/amd64/Apps/DeveloperStudio/bin/amd64/developerstudio.elf` | 1,201,604 bytes | `A93A91E795824DB6BDB97D05D629BD8217BF7233A6B2DFAC1F60D77C671463A9` |
| ARM64 | `out/tools5-package/arm64/Apps/DeveloperStudio/bin/arm64/developerstudio.elf` | 1,376,764 bytes | `AAD60FA1C0A2EB604E16BC2A58BC78ED15231BC20A48CCA159392671B069FD10` |

The AMD64 entry point is `0x500cbb60`; the ARM64 entry point is `0x500ec92c`.
The staged package audit reports `package_content_audit=PASS`.

Native Apply availability is **disabled** in the packaged application for the
same missing safe-replacement contract. The package was cross-built and
audited; no Native ELF guest runtime acceptance was performed. Guest
acceptance remains pending, no QEMU session was launched, and no bare-metal
write was performed.

## Future multi-file Apply requirements

TOOLS5 does not implement multi-file Apply. Before considering that scope,
provide a durable per-file recovery journal or an actual filesystem
transaction, exact reporting for partial completion, recovery/undo semantics,
project inventory handling for added and removed files, and failure injection
across every commit stage. Do not broaden the feature to Replace All until the
single-file storage contract is implemented and qualified on each target.

## Qualification record

Qualification complete as a storage blocker. The source change is documentation
and a visible disabled-state explanation only; no file-application path was
implemented.

| Evidence | Result |
|---|---|
| Starting source HEAD | `a33918c11a79895f074c47b36f142fcdfaa8a6c3` |
| Starting Server integration HEAD | `a5613cdcdb9e4b954bef53264201beaaa21de1cc` |
| Clean Debug CMake build | PASS, 108/108 steps |
| Hosted CTest | PASS, 37/37 suites |
| AMD64 staged package / audit | PASS, ELF64 ET_EXEC, 1,201,604 bytes, SHA-256 `A93A91E795824DB6BDB97D05D629BD8217BF7233A6B2DFAC1F60D77C671463A9` |
| ARM64 staged package / audit | PASS, ELF64 ET_EXEC, 1,376,764 bytes, SHA-256 `AAD60FA1C0A2EB604E16BC2A58BC78ED15231BC20A48CCA159392671B069FD10` |
| Apply fault injection | Not run; no Apply/storage transaction path exists to inject failures into |
| Hosted Apply acceptance | Unavailable; disabled by current workspace API contract |
| Native Apply acceptance | Unavailable; disabled by current Native VFS/ABI contract |
| Guest acceptance | Not run; no QEMU session launched and no bare-metal write performed |
