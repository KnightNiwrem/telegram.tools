# Extraction patch stack

Small, reviewable patches applied to the pinned TDesktop submodule at build
time (`scripts/build-native.sh` / `scripts/build-wasm.sh` run
`git -C upstream/tdesktop apply ../../patches/*.patch` and revert on
completion). The submodule commit itself is never changed; UPSTREAM.md
records the pinned revision.

| Patch | Purpose |
| --- | --- |
| `0001-add-renderer-harness-hook.patch` | Opt-in `add_subdirectory` hook in `Telegram/CMakeLists.txt` so the harness targets configure inside the pinned tree. No upstream behavior change when the option is unset. |

Rules (plan Phases 2 and 14):

- One concern per patch; every patch names the upstream file and why the
  change cannot live in `renderer/cpp` instead.
- Never modify upstream rendering behavior — hooks and build plumbing only.
  A patch that would change painted output requires a fixture-matrix run
  and an explicit entry in UPSTREAM.md before it lands.
- Rebasing to a newer TDesktop tag means re-applying this stack one patch
  at a time and re-running the full fixture matrix (Phase 14).
