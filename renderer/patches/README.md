# Extraction patch stack

Small, reviewable patches applied to the pinned TDesktop submodule at build
time (`scripts/build-native.sh` / `scripts/build-wasm.sh` run
`git -C upstream/tdesktop apply ../../patches/*.patch` and revert on
completion). The submodule commit itself is never changed; UPSTREAM.md
records the pinned revision.

Patches are grouped by the repository they apply to (`tdesktop/` for the
main tree, `lib_ui/`, `lib_base/`, `lib_crl/` for nested submodules,
`cmake_helpers/` for the desktop-app cmake submodule), because `git apply`
cannot cross submodule boundaries.

| Patch | Purpose |
| --- | --- |
| `tdesktop/0001-add-renderer-harness-hook.patch` | Opt-in `add_subdirectory` hook in `Telegram/CMakeLists.txt` so the harness targets configure inside the pinned tree. No upstream behavior change when the option is unset. |
| `tdesktop/0002-harness-and-wasm-exclusions.patch` | Harness builds drop the tgcalls/webrtc-dependent td_ui sources (round video recorder, desktop-capture picker); Emscripten additionally drops the ffmpeg clip player and stripe card editor, guards a QSemaphore include, and makes one file-scope `QLocale()` lazy (it ran before embind was ready — QTBUG-131279). |
| `tdesktop/0003-sessionless-html-import-seam.patch` | Exposes `Iv::Editor::BlocksFromHtmlSource` (was in an anonymous namespace, so the harness cannot link it and duplicating the `HtmlBlock` → `RichPage::Block` conversion in `renderer/cpp` would risk diverging from TDesktop import semantics) and lets its session be null: remote photo/document identities then resolve to nothing and media falls back to the existing local/placeholder path or is dropped, counted in the new `BlocksImportResult::droppedMedia`. No behavior change with a non-null session; nothing painted changes. |
| `lib_ui/0001-wasm-build-fixes.patch` | Qt<6.10 accessibility-attribute guard (dev host); sprite-cache SHA-256 via QCryptographicHash instead of OpenSSL; XCB focus handling excluded; spoiler masks generated inline under Emscripten (waiting on same-thread work would deadlock). |
| `lib_base/0001-randomfill-getentropy-emscripten.patch` | `RandomFill`/`RandomAddSeed` use `getentropy` (browser CSPRNG) instead of OpenSSL under Emscripten. |
| `lib_crl/0001-emscripten-main-loop-async.patch` | crl's Qt backend dispatches async work through the main event loop; single-threaded Qt-wasm has no QThreadPool. |
| `cmake_helpers/0001-emscripten-branch-nice-target-sources.patch` | Adds the missing Emscripten branch to `nice_target_sources` — without it every desktop platform's sources compile under Emscripten. |

Rules (plan Phases 2 and 14):

- One concern per patch; every patch names the upstream file and why the
  change cannot live in `renderer/cpp` instead.
- Never modify upstream rendering behavior — hooks and build plumbing only.
  A patch that would change painted output requires a fixture-matrix run
  and an explicit entry in UPSTREAM.md before it lands.
- Rebasing to a newer TDesktop tag means re-applying this stack one patch
  at a time and re-running the full fixture matrix (Phase 14).
