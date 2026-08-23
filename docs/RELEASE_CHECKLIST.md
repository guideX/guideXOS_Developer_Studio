# Developer Studio release-candidate checklist

This checklist is the repeatable manual distribution rehearsal for the
experimental, hosted-development Developer Studio package. It does not make
a 1.0 stability claim and it does not authorize a repository push.

## Source and build

- [ ] Record the Developer Studio and paired Server branch, commit, upstream,
      and divergence before testing.
- [ ] Confirm both worktrees are clean and freeze feature scope.
- [ ] Run the fast validation tier and retain its external trace directory.
- [ ] Run the required debugger tier and retain its external trace directory.
- [ ] Build the paired experimental Server and record warnings separately from
      failures.
- [ ] Run the explicit `developer_studio_debugger_watches_test` CTest case to
      cover the 64 KiB native-thread stack regression.

## Package and distribution

- [ ] Build the production package from the frozen source with
      `build.ps1 -Configuration Debug`.
- [ ] Audit the exact runtime allowlist: `app.json` and
      `bin/amd64/developerstudio.elf`.
- [ ] Record manifest identity, package size, ELF shape, entry point, symbol
      table, debug-section policy, and SHA-256.
- [ ] Stage the package outside both repositories and audit the staged copy
      for checkout paths, test paths, log paths, and forbidden files.
- [ ] Copy the staged package into `Apps/DeveloperStudio` and verify hashes.
- [ ] Remove the installed package, recreate the staging copy, reinstall it,
      and verify the second copy before launch.

## Real hosted rehearsal

- [ ] Start the real experimental Server and launch by canonical App Model ID
      `com.guidexos.developerstudio`.
- [ ] Confirm package-source resolution, initial render, window ownership,
      project open, edit/save, build output, and clean close.
- [ ] Exercise document A/B switching, dirty-state preservation, exact
      `Point::origin` completion acceptance, and completion dismissal.
- [ ] Exercise an intentional build failure and a subsequent successful
      recovery.
- [ ] Exercise F9, Ctrl+F5, a real source breakpoint, Call Stack, Locals,
      Watches, Continue/step lifecycle, and targeted shutdown.
- [ ] Relaunch after the second install and confirm clean Server shutdown.

## Negative and reproducibility checks

- [ ] Substitute a temporary package with a missing executable and confirm the
      package audit and App Model launch reject it without a stale process.
- [ ] Mutate a temporary ELF and confirm its SHA-256 differs from the release
      artifact; never mutate the authoritative package.
- [ ] Build the package twice without source changes and compare size and
      SHA-256 byte-for-byte.
- [ ] Recreate staging from scratch and compare both runtime hashes.

## Handoff and cleanup

- [ ] Update the release-readiness record, candidate status, and this checklist
      with the evidence location and exact result classification.
- [ ] Restore generated desktop state and any temporarily hidden runtime logs.
- [ ] Remove or move generated build directories out of the repositories.
- [ ] Run `git diff --check` and verify both worktrees are clean except for the
      intended release-document commit.
- [ ] Do not push. Record the final local divergence and the next manual
      synchronization step.
