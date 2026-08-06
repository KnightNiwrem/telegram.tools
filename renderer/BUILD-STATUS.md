# Build and phase status

Honest, current state of the implementation against the plan
(`telegram-rich-message-wasm-implementation-plan.md`). Updated 2026-08-06.

## What is done and verified

| Plan phase | State |
| --- | --- |
| Phase 0 — provenance/licensing | **Done.** TDesktop `v7.0.9` pinned as submodule (`a1e89e1f`), UPSTREAM.md/LICENSING.md/THIRD_PARTY_NOTICES.md recorded. Counsel sign-off on the MIT-extension boundary is an organizational action and remains open (blocking Phase 12 only). |
| Phase 1 — fixture corpus | **Done for the input side.** 76 fixtures covering all 21 `InputRichBlock` kinds, all inline entity forms, layout edge cases, and over-limit inputs; 93 deno tests pass. Native golden captures require the native host (below); `scripts/capture-goldens.sh` is ready. |
| Phase 4 (TS half) — schema/contract | **Done.** Canonical JSON Schema v1, TS model + limits validator, versioned C ABI defined on both sides (`js/sdk/module.ts` ↔ `cpp/wasm-host/ttr_abi.h`). |
| Phase 6 — grammY adapter | **Done and tested** at `grammy_types@v4.0.0`. Exhaustive union handling with compile-time guards, source-path diagnostics, media handles. |
| Phase 11 — telegram.tools scaffold | **Done as scaffold.** `/rich-message-preview` route + `RichMessageEditor` island: three input modes, width/theme controls, debounced latest-wins loop, diagnostics panel, canonical inspector, honest no-artifact state. |

## Extraction findings that supersede the plan text

- `Iv::Markdown::TryPrepareNativeInstantView` **does exist** at `v7.0.9`
  (the plan's revalidation note said otherwise). It is the preparation
  entry used by `RichDraftPreview`, and it takes **no session**: sessions
  enter only through `MediaRuntime` (an interface with two pure-virtual
  methods) and `ResolveRichMessageLimits`.
- The whole `iv/markdown` subsystem lives in CMake target `td_iv`, which
  does not link the Data/Main layer at all. `chat_style`/`chat_theme` are
  session-free; `ChatStyle` has an isolated-palette constructor.
- `iv_rich_page.cpp` mixes session-bound and session-free functions in one
  translation unit; the harness avoids it entirely by constructing
  `Iv::RichPage` directly from canonical JSON, mirroring `ParseRichPage`
  conventions (recorded in `cpp/adapters/canonical_page.cpp`).
- Exactly one upstream symbol is stubbed: `LocationClickHandler::Url`
  (byte-identical copy) — its real home transitively includes the whole
  application shell.
- `tests/test_main.cpp` (`test_text`) provided the proven headless
  bootstrap (integrations, `style::StartManager`, `Ui::Emoji::Init`).

## Native build (Phase 2)

The pinned tree **configures and generates** on this dev host with the
harness hook. Known dev-host requirements beyond apt packages (all
machine-local, not committed):

- `ada` and `rnnoise` built from source into `/usr/local` (no Ubuntu
  packages).
- Shim CMake package configs in `/usr/local/lib/cmake` for `protobuf`
  (Ubuntu ships no config files), `tg_owt`, and `tde2e` (call libraries
  the harness never builds or links; the Telegram executable cannot be
  built against these shims).
- `-DQSB_EXECUTABLE=/usr/lib/qt6/bin/qsb` (hint list misses the Ubuntu
  location).
- Distro Qt is 6.9.2 while the pinned lib_ui expects ≥ 6.10 in one
  accessibility file — patched under `patches/lib_ui/` (metadata only, no
  rendering change). **The distro-Qt build is a development harness, not
  the canonical reference profile**; goldens that certify fidelity must
  come from a pinned-Qt container build (plan §4), which is not yet set
  up.

Build result: see the bottom of this file for the latest attempt log
summary.

## Not yet implemented (ordered by plan sequence)

- **Phase 2 verification**: in-TDesktop reference comparison, ASan/UBSan
  runs, golden capture (requires the built native host + a reference
  TDesktop build).
- **Phase 3 — Qt/WASM build**: requires emsdk + Qt-for-WebAssembly and a
  cross-compiled subset of the desktop-app externals (notably microtex,
  cmark-gfm, prisma for WASM; OpenSSL/ffmpeg-dependent parts of lib_ui
  need pruning or WASM ports). `scripts/build-wasm.sh` encodes the
  intended invocation; it has not succeeded yet.
- **Phase 5** block-family fidelity matrix (needs goldens).
- **Phase 7** HTML/Markdown import inside the renderer: the sessionless
  seam below `BlocksFromHtmlSource` (`TextUtilities::BlocksFromHtml` +
  `ImportContext`) is traced in the plan but not yet extracted; the
  renderer currently reports html/markdown modes as structured
  `renderer.mode-unsupported` diagnostics.
- **Phase 8** media beyond static images (video posters, audio metadata
  rendering paths are wired but unverified; animated media intentionally
  static).
- **Dark theme**: the render service currently renders the default
  (light) palette and emits `renderer.theme-unavailable` for dark until
  embedded night-palette loading is wired.
- **Phases 9, 10, 12, 13, 14** as described in the plan.

## Latest build attempt

(Appended by hand after each significant attempt.)
