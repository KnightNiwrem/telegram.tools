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
| Phase 3 — Qt/WASM build | **Done: the core feasibility proof holds.** The artifact builds through a dedicated Emscripten superbuild (`cpp/wasm-superbuild`, since desktop-app's cmake has no Emscripten platform branch) and renders in headless Chrome. `/rich-message-preview` shows real TDesktop output: headings, bold, spoiler particles, bullet lists, task checkboxes. |
| Phase 11 — telegram.tools scaffold | **Done as scaffold, now with live pixels.** `/rich-message-preview` route + `RichMessageEditor` island: three input modes, width/theme controls, debounced latest-wins loop, diagnostics panel, canonical inspector, honest no-artifact state. |

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
harness hook. The dev-host requirements beyond apt packages are now
captured as a script — `scripts/setup-ci-host.sh` (apt set, ada v2.9.2
and rnnoise v0.2 from source, shim CMake configs) — used by CI and
usable on any fresh Ubuntu 24.04+ machine. Originally machine-local:

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

**Build result: the native host builds and renders.**
`build/native/ttr_native_host` (from `scripts/build-native.sh`) renders the
entire blocks-mode fixture corpus — **71 fixtures × 3 canonical widths =
213 goldens, zero failures** — through the unmodified
`TryPrepareNativeInstantView` → `MarkdownArticle` layout/paint path.
Captured goldens live in `tests/native-goldens/` (dev-profile: distro Qt
6.9.2, light theme; canonical pinned-Qt goldens remain an open item).
Spot-checked output includes correct Telegram task-list checkboxes,
spoiler mess particles, BiDi shaping, table borders/spans/stripes/caption,
quote bars, and syntax-highlighted code blocks.

Link strategy note: `td_iv`/`td_ui` are OBJECT libraries whose editor/box
objects reference application symbols that only exist in the Telegram
executable. Symbols the render path needs are linked for real or provided
byte-identically in `cpp/core/link_stubs.cpp` (`LocationClickHandler::Url`,
three `Data::LocationPoint` members, `Lang::details::Current/Value` over
the generated base-English values, `Iv::Encode/DecodeRichPageLinkUrl`);
remaining app-only references resolve to null via
`--unresolved-symbols=ignore-all` and would crash loudly if reached — the
full-corpus golden capture is the tripwire, and it passes clean.

## Not yet implemented (ordered by plan sequence)

- **Phase 2 verification remainder**: comparison against an unmodified
  in-TDesktop `RichDraftPreview` capture (requires a full reference client
  build), ASan/UBSan runs, and pinned-Qt container goldens. The harness
  renders and its goldens are recorded, but they are not yet certified
  against the reference client.
- **Phase 3 remainder**: pixel-level fidelity (below), the one pullquote
  spacing difference, binary-size work (52 MB raw / 37 MB gzipped; the
  ~26 MB of embedded emoji-sprite PNGs are the dominant share and are
  already-compressed data, so the practical fix is loading them lazily
  outside the binary rather than better transport compression),
  Firefox and WebKit runs, and the
  performance measurements the plan asks for (cold init, warm render,
  memory high-water).
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

## Native vs WASM comparison (plan Phase 3 verification)

`scripts/verify-wasm.sh` renders every blocks-mode fixture in headless
Chrome and compares against the committed native goldens:

- **Geometry: 212 of 213 identical.** Same widths, heights, and therefore
  the same line breaking and block layout as the native harness. The one
  exception is `block-pullquote-credit` at 320px (native 320×70, WASM
  320×55). Narrowed down: both render the same content with the same line
  wrapping (quote on two lines, credit on one); the 15px is vertical
  spacing around the credit. Still open.
- **Pixels: 176 of 213 differ.** These are font-rasterization differences
  between Qt-wasm's bundled FreeType and the host's, which plan §3 and
  Phase 9 classify as a raster-only difference class. They are *not*
  waived: no pixel threshold is configured, the comparison reports every
  difference, and establishing the tolerance requires the pinned-Qt
  reference profile that is still outstanding.

The CI gate is `scripts/verify-wasm.sh --geometry-only`: render failures
and geometry mismatches fail the build; pixel-checksum differences are
counted and reported without gating (no threshold is configured — see
above); the one known geometry difference is listed with its tracking
note in `tests/wasm-known-differences.txt`, and an entry that stops
diverging fails the run until removed.

## Continuous integration (GitHub Actions)

`.github/workflows/renderer-wasm.yml` (push to `renderer/**` on
`main`/`wasm-rich-message-renderer`, or manual dispatch) reproduces the
build on a stock `ubuntu-24.04` runner:

1. shallow submodule checkout of the pinned tree;
2. `scripts/setup-ci-host.sh` — the scripted form of the former
   machine-local setup;
3. Qt 6.9.2 via aqtinstall: `linux_gcc_64` (+`qtshadertools` for qsb)
   as host Qt and `wasm_singlethread`, so the host Qt version matches
   the pinned Qt-wasm exactly (runner distro Qt is too old for the
   pinned tree); emsdk 3.1.70; both cached, compiles through ccache;
4. `build-native.sh --codegen-only` — only the host code generators the
   superbuild imports (`codegen_style`/`lang`/`emoji`), not the full
   native host;
5. `build-wasm.sh`, deno tests, `verify-wasm.sh --geometry-only`
   against the committed goldens;
6. uploads `ttr-renderer.{js,wasm}` as a build artifact.

The full native host + golden recapture stays a manual/dev-host flow
(goldens are committed).

After the verify gate, the workflow publishes the artifact as a GitHub
release (`renderer-wasm-<shortsha>`) and pins it — release tag plus
per-file sha256 — in `renderer/artifact.lock.json` via a bot commit
(excluded from the workflow's own trigger paths). Deployment consumes
the pin: `deno task build` first runs
`scripts/fetch-renderer-artifact.ts`, which downloads the pinned
release assets into `static/rich-message-renderer/` and verifies the
hashes. A deploy platform that rebuilds on every push therefore always
serves the exact bytes of the last *verified* renderer build — a deploy
racing a 20-minute renderer CI run just keeps the previous pin until
the lock-bump commit lands (and itself triggers the redeploy).

### WebP emoji sprites

Qt for WebAssembly ships no WebP image plugin, so TDesktop's
`Resources/emoji/emoji_N.webp` sprites decoded to null images and
`Ui::Emoji::Init()` aborted on an empty sprite list (visible only as a
console assertion — renders still returned). `cpp/wasm-superbuild/emoji_png.cmake`
re-encodes the sprites to PNG at build time and maps them onto the same
`:/gui/emoji/emoji_N.webp` resource paths, so no upstream code changes and
the raster is identical. Costly: the PNG re-encode adds ~26 MB to the
artifact (25 → 52 MB raw; earlier notes understated this as ~9 MB), and
being already-compressed it also dominates the gzipped size (10 → 37 MB).
Getting the sprites out of the binary is the top item of the Phase 3
binary-size work.

## Determinism check

`scripts/verify-goldens.sh` re-renders every golden and compares
byte-for-byte (`scripts/compare-renders.py`): **213/213 identical** on
recapture in the same environment (plan Phase 1 verification: "repeating
the native capture in the same environment produces identical images").
