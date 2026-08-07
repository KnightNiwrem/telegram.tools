# Telegram Desktop Rich-Message Renderer for Web and VS Code

## Phase-by-phase implementation and verification plan

Status: active plan. Implementation status is tracked in
`renderer/BUILD-STATUS.md`; decisions that changed after this plan was written
are recorded in the decision log (§1.1) rather than by rewriting phase text.

Initial renderer baseline: Telegram Desktop `v7.0.9`

Initial product target: the standalone `KnightNiwrem/telegram.tools` app
(Deno Fresh/Preact, deployed on Deno Deploy). `grammyjs/telegram.tools` and
`grammyjs/vscode` are the intended eventual integrations, not current
targets — see §1.1.

## 1. North star

Build a browser-embeddable WebAssembly renderer that reuses Telegram Desktop's
actual rich-message preparation, layout, styling, and painting code. A user
should be able to supply a grammY `InputRichMessage` or write supported rich
message HTML and see a faithful Telegram Desktop preview without sending a
message.

The same renderer package must run in:

1. a live editor in `telegram.tools`; and
2. a grammY VS Code webview that updates while the source file is edited.

This plan deliberately does **not** use a separately implemented DOM/CSS
renderer as the final preview. HTML, Preact, or Svelte may provide the editor
and controls, but Telegram's visual output must be painted by code compiled
from Telegram Desktop sources.

### 1.1 Decision log — deviations from the plan as written

Recorded changes of direction; the phase text below is left as originally
planned unless it was factually wrong.

- **Standalone product first.** The work lives in `KnightNiwrem/telegram.tools`,
  trimmed to a single-purpose rich-message renderer app rather than the grammY
  multi-tool site. Integration into `grammyjs/telegram.tools` and the grammY
  VS Code extension (Phase 12) is deferred until the renderer itself is
  further along. The Phase 0 licensing decision (renderer as a separately
  distributed GPL artifact) stands and continues to gate only Phase 12.
- **Renderer location.** The renderer lives in `renderer/` inside the app repo
  instead of the dedicated repository §6 prefers. Splitting it out (with
  history) is deliberately deferred.
- **Artifact distribution (refines Phase 10).** CI publishes each verified
  build as a GitHub release (`renderer-wasm-<shortsha>`) and pins it — tag plus
  per-file sha256 and byte size — in `renderer/artifact.lock.json`; the deploy
  build fetches and verifies the pinned assets. This already provides the
  content-hashing and matched-JS/WASM guarantees Phase 10 asks for, without an
  npm package yet.
- **Per-request scale shipped ahead of sequence.** The Phase 4 "scale"
  configuration item is done end-to-end: `RenderRequest.scale` (1..4) via
  `QImage::setDevicePixelRatio`, layout kept in logical pixels, physical
  allocation capped at the scale-1 worst case (4096×65536), applied scale
  echoed in the result. The editor requests `min(devicePixelRatio, 3)`. The
  canonical golden profile (§4) intentionally stays at device pixel ratio 1.
- **Preview presentation (refines Phase 11).** The preview renders at true
  1:1 pixel size inside a keyboard-accessible horizontal-scroll region, in its
  own full-width row above the editor — scroll, never scale, so the chosen
  viewport width shows real wrapping and density.
- **HTML media via placeholder bindings + user-consented asset loading
  (refines Phases 7/8).** Sessionless HTML import no longer drops recognized
  media blocks: each one gets an `ImportedMediaPlaceholderId` bound to its
  `src` string, which becomes the renderer media-store ref. The render result
  lists every media ref with its resolution state (`mediaRefs`). The editor
  turns that list into an assets panel where the user explicitly loads each
  URL (browser-side `fetch` with a byte cap, upload fallback for
  CORS-restricted hosts) and registers the bytes through the existing
  `registerMedia` ABI before re-rendering. The renderer core itself never
  fetches — `src` stays an opaque dictionary key, preserving determinism and
  the no-network guarantee. The parser gate is intentionally untouched: plain
  `<img>` without TDesktop's own-media markup keeps authentic local-import
  semantics (ignored).
- **Sessionless hosted media blocks (Phase 8, corrects an assumption).**
  TDesktop's only `HostedMediaBlockFactory` implementation is session-bound
  (`IvHistoryViewMediaBlockFactory` over `HistoryView::Media`), so photo and
  video blocks had *never* occupied space in this harness — there is no
  upstream "unloaded placeholder" visual to inherit sessionless. The harness
  now provides its own factory (`cpp/core/sessionless_media_blocks.cpp`):
  layout is pure arithmetic from the declared dimensions (ratio clamped to
  4:1 against hostile attributes), painting draws the registered static
  asset aspect-fit over `imageBg`, and a flat `windowBgOver` fill stands in
  when no bytes are registered — geometry is identical before and after
  load, per the Phase 8 verification requirement. Blocks whose source
  declares no dimensions get them backfilled from the registered asset's
  decoded size (without bytes *and* without dimensions the prepare stage
  still drops the block, upstream behavior). Audio, map, channel, and
  grouped-media blocks remain unimplemented sessionless (zero height).

## 2. Initial scope and eventual scope

### Initial target

Faithfully render the rich-message **content area** represented by Telegram
Desktop's `Iv::RichPage`, using the same preparation and `MarkdownArticle`
painting path used by `RichDraftPreview`.

### Eventual target

- Support every `InputRichMessage` input form: `blocks`, `html`, and `markdown`.
- Support every grammY `InputRichBlock` variant and supported inline rich-text
  entity.
- Support media supplied as browser-local data without requiring a Telegram
  session.
- Support light/dark themes, multiple preview widths, scale factors, LTR, and
  RTL.
- Integrate the same packaged renderer into `telegram.tools` and the grammY
  VS Code extension.
- Optionally render complete chat-message chrome: bubble, sender, timestamp,
  reply header, reactions, and delivery state.

### Non-goals for the first complete content renderer

- Connecting to a user's Telegram account.
- Sending messages during ordinary preview operation.
- Claiming one image is pixel-identical to every operating system. Telegram
  Desktop itself can rasterize fonts differently on Windows, macOS, and Linux.
- Replacing the native renderer with a visually similar browser stylesheet if
  extraction becomes difficult.

## 3. What “faithful” means

Fidelity must be verified at four separate levels:

| Level | Requirement | Pass condition |
| --- | --- | --- |
| Semantic | The same rich blocks, text entities, limits, and fallback decisions reach the renderer. | Canonical structure and diagnostics match the reference. |
| Geometric | Blocks, wrapping, line count, spacing, alignment, and final dimensions match. | No unexplained geometry or line-break differences. |
| Visual | Colors, typography, borders, backgrounds, quote bars, tables, formulae, and media presentation match. | Pixel differences are absent or limited to an explicitly measured rasterization tolerance. |
| Behavioral | Details, spoilers, links, selection, resizing, media loading, and theme changes behave the same where supported. | Interaction tests and state snapshots match the reference behavior. |

“The images look similar” is not an acceptance test. Every supported fixture
must have a recorded semantic result, geometry result, native reference image,
WASM image, and diff result.

## 4. Canonical reference profile

Start with one reproducible profile before expanding the matrix:

- Telegram Desktop tag: `v7.0.9`, resolved and recorded as an immutable commit
  SHA.
- Native reference platform: Linux x86-64 in a pinned container or VM image.
- Exact Qt, compiler, and dependency revisions used by that TDesktop tag.
- Device pixel ratio: `1`.
- UI scale: `100%`.
- Themes: default light and default dark.
- Preview widths: `320`, `480`, and `720` logical pixels.
- Locale set: English initially, then one RTL locale.
- Fonts: the exact font set available to the reference build, stored or
  installed reproducibly subject to licensing.
- Animations: frozen at a documented frame or disabled for golden captures.

Windows, macOS, HiDPI, additional themes, and more locales become additional
reference profiles later. They do not silently change the canonical baseline.

## 5. Intended architecture

```text
InputRichMessage / HTML / Markdown
                |
                v
        TypeScript input adapters
                |
                v
       versioned canonical JSON
                |
                v
  C++ JSON -> Iv::RichPage conversion
                |
                v
 PrepareNativeIvBlocks / native preparation
                |
                v
       MarkdownArticle layout + paint
                |
                v
       QImage RGBA output + metadata
                |
                v
     TypeScript SDK -> browser canvas
                |
        +-------+-------+
        |               |
 telegram.tools     VS Code webview
```

Painting to an off-screen `QImage` is the preferred first implementation. It
keeps Telegram's `QPainter` path while avoiding dependence on a browser DOM
layout engine and reducing dependence on a full `QWidget` hierarchy. The SDK
copies the RGBA buffer into a browser canvas.

If off-screen painting cannot preserve a necessary behavior, add the smallest
Qt/WASM widget host needed for that behavior. Do not replace native layout with
DOM layout.

## 6. Suggested repository/package layout

The exact repository can be selected during Phase 0, but preserve these
boundaries:

```text
renderer/
  upstream/tdesktop/          # pinned source or submodule
  patches/                    # small, reviewable extraction patch stack
  cpp/
    core/                     # sessionless renderer facade
    adapters/                 # canonical JSON -> Iv::RichPage
    native-host/              # native reference/test executable
    wasm-host/                # Emscripten/Qt WASM entry point
  schema/                     # versioned canonical schema and fixtures
  js/
    sdk/                      # public TypeScript API and canvas host
    adapter-grammy/           # grammY types -> canonical schema
    adapter-html/             # HTML import interface
  tests/
    fixtures/
    native-goldens/
    wasm-output/
    diffs/
    integration/
  scripts/
    build-native.*
    build-wasm.*
    capture-goldens.*
    compare-renders.*
  THIRD_PARTY_NOTICES.md
  UPSTREAM.md
```

The renderer should be independently buildable and versioned. Product
repositories should consume a released renderer artifact rather than each
maintaining their own TDesktop fork.

## 7. Public renderer contract

Define the public API before broad feature work. A representative request and
result are:

```ts
interface RenderRequest {
  schemaVersion: 1;
  richMessage: CanonicalRichMessage;
  viewportWidth: number;
  theme: "light" | "dark";
  scale: number;
  direction?: "auto" | "ltr" | "rtl";
  interactionState?: Record<string, unknown>;
}

interface RenderResult {
  width: number;
  height: number;
  rgba: Uint8Array;
  diagnostics: RenderDiagnostic[];
  geometry: BlockGeometry[];
  hitTargets: HitTarget[];
  rendererVersion: string;
  tdesktopRevision: string;
}
```

The C++/WASM boundary should use a narrow C ABI or equally stable exported
surface with explicit create/render/free ownership. Keep Emscripten and Qt
objects behind the boundary.

Required behaviors:

- The same request produces deterministic dimensions and content under a
  pinned reference profile.
- Invalid or unsupported input returns structured diagnostics instead of
  crashing.
- A render can be cancelled or superseded when a user continues typing.
- Repeated renders reuse the initialized WASM module and renderer context.
- Every result identifies its renderer and TDesktop revisions.

---

# Implementation phases

## Phase 0 — Freeze scope, provenance, and licensing

### Objective

Create a reproducible project baseline and resolve distribution obligations
before extracted code is published.

### Implementation

- [ ] Resolve `v7.0.9` to its immutable TDesktop commit SHA.
- [ ] Record TDesktop's Qt revision, submodules, compiler assumptions, generated
      sources, build flags, and relevant patches.
- [ ] Record the current revisions and build systems of:
  - `grammyjs/telegram.tools` — currently a Deno Fresh/Preact application;
  - `grammyjs/vscode` — currently a VS Code extension with webview support.
- [ ] Write `UPSTREAM.md` describing exactly which TDesktop files and revisions
      are used.
- [ ] Decide whether the renderer lives in a dedicated repository, a TDesktop
      fork, or a package directory in another repository. Prefer a dedicated
      renderer repository with a pinned upstream and a small patch stack.
- [ ] Review compatibility among TDesktop's licence, `telegram.tools`' licence,
      the VS Code extension's licence, Qt's applicable licence, and all bundled
      fonts/assets.
- [ ] **License-conflict resolution (blocking).** The renderer WASM is derived
      from TDesktop, which is GPL-3.0 (with the OpenSSL exception). The two
      product targets differ:
  - `telegram.tools` is **AGPL-3.0** — compatible with statically bundling the
    GPL-derived artifact.
  - `grammyjs/vscode` is **MIT** — statically bundling GPL-derived WASM into it
    would force the combined distributed work to GPL/AGPL terms, which conflicts
    with keeping the extension MIT.
- [ ] **Chosen approach:** ship the TDesktop-derived renderer as a **separate,
      independently distributed GPL-licensed artifact** (own package/repo, own
      LICENSE and source-offer), and have the MIT VS Code extension **load** that
      artifact at runtime rather than vendoring or statically bundling its
      sources. The extension stays MIT; the renderer stays GPL; they are
      distributed together but licensed separately (mere aggregation / a loaded
      component, not a single combined work). Record this decision, and confirm
      with counsel that the extension↔renderer boundary is an
      arm's-length load, not a derivative combination, for the packaging form
      used (see Phase 10/12).
- [ ] Document how corresponding source, build instructions, licence text, and
      notices will accompany the distributed WASM — including the separate
      source-offer for the GPL renderer artifact consumed by the MIT extension.
- [ ] Explicitly define the first product as “TDesktop `v7.0.9` rich-message
      content preview,” not complete chat chrome.

### Verification

- [ ] A clean machine can resolve every pinned source revision.
- [ ] The native TDesktop reference build completes from the recorded inputs.
- [ ] A licence checklist names every redistributed source, binary, font, and
      theme asset and its handling.
- [ ] No source is copied into the project without recorded provenance.
- [ ] The GPL-renderer-vs-MIT-extension conflict is resolved on record: the
      renderer is a separately licensed, separately distributed GPL artifact
      that the MIT extension loads at runtime, with its own LICENSE and
      source-offer. This is a **blocking** condition — packaging (Phase 10) and
      VS Code integration (Phase 12) must not proceed until it is signed off.

### Exit gate

The project has an immutable baseline, a buildable reference client, and an
approved distribution/licensing strategy — including a signed-off resolution of
the GPL-renderer / MIT-extension boundary (renderer shipped as a separate GPL
artifact, loaded by the MIT extension, not statically bundled).

## Phase 1 — Build the fidelity fixture corpus

### Objective

Define what must render correctly before changing or extracting native code.

### Implementation

- [ ] Enumerate the **actual** exported `InputRichBlock` union members at the
      pinned grammY SHA before writing fixtures — do not work from an idealized
      block list. Confirmed exported leaf types at the pinned revision include
      `InputRichBlockParagraph`, `InputRichBlockSectionHeading`,
      `InputRichBlockList`, `InputRichBlockListItem`,
      `InputRichBlockBlockQuotation`, `InputRichBlockDetails`,
      `InputRichBlockPhoto`, `InputRichBlockVideo`, `InputRichBlockAudio`,
      `InputRichBlockVoiceNote`, `InputRichBlockAnimation`,
      `InputRichBlockCollage`, and `InputRichBlockSlideshow`. Kinds mentioned
      only in doc-comments (tables, formula/math, map, footer, divider, anchor)
      must be verified as distinct exported types before a fixture targets them;
      if a kind is not a separately exported member, record how it is actually
      represented in the union.
- [ ] Create small, named fixtures for every enumerated `InputRichBlock` kind.
- [ ] Create inline-text fixtures for plain text, bold, italic, underline,
      strikethrough, spoilers, code, links, mentions, custom emoji, nested
      entities, adjacent entities, and overlapping legal combinations.
- [ ] Add layout edge cases:
  - empty and whitespace-only blocks;
  - very long words and URLs;
  - hard line breaks and repeated spaces;
  - emoji, combining marks, and surrogate pairs;
  - nested lists and maximum legal nesting;
  - large and merged table cells;
  - formulae and syntax-highlighted code;
  - LTR text containing RTL runs and the reverse;
  - minimum, typical, and maximum legal content sizes.
- [ ] Add invalid and over-limit fixtures with expected diagnostics.
- [ ] Give each fixture a stable ID and store its source form plus expected
      canonical form.
- [ ] Build an in-TDesktop fixture loader or test-only command that constructs
      the same `Iv::RichPage` objects and renders them through
      `RichDraftPreview`.
- [ ] Capture native golden images for the canonical widths and themes.
- [ ] Instrument or expose prepared block geometry, line counts, and final
      dimensions where practical.

### Verification

- [ ] Every supported block/type in the pinned grammY type set maps to at least
      one fixture.
- [ ] Every fixture can be reproduced without network access, except fixtures
      explicitly classified as remote-media integration tests.
- [ ] Repeating the native capture in the same environment produces identical
      images and metadata.
- [ ] Fixture names and failure output make the affected feature obvious.

### Exit gate

The repository contains a deterministic native baseline capable of detecting
semantic, layout, and visual regressions.

## Phase 2 — Prove a minimal sessionless native renderer

### Objective

Render one paragraph through Telegram's native article code without starting a
Telegram session or connecting to Telegram.

### Implementation

- [ ] Trace the dependency graph beginning at:
  - `HistoryView::Controls::RichDraftPreview`;
  - `Iv::Markdown::TryPrepareNativeInstantView` (a revalidation note here
    previously claimed this symbol does not exist at `v7.0.9`; it does, takes
    no session, and is the preparation entry the harness uses — see
    "Extraction findings" in `renderer/BUILD-STATUS.md`);
  - `Iv::Markdown::MarkdownArticle::setContent`;
  - `MarkdownArticle::resizeGetHeight` and `MarkdownArticle::paint`.
- [ ] Confirm the extraction seam at the source level (verified against
      `v7.0.9`): `MarkdownArticle(const style::Markdown&,
      std::shared_ptr<MathRenderer>)` and `PrepareNativeIvBlocks` take no
      `Main::Session`, `Window`, or `Delegate`, so the paragraph path is
      session-free by construction. If a later block family reintroduces a
      session dependency, record it as an explicit extraction risk rather than
      assuming the seam stays clean.
- [ ] Treat `RichDraftPreview` as the behavioral reference, but extract below
      its `QWidget` and `Main::Session` integration where possible.
- [ ] Introduce a small `RendererContext` that provides only required style,
      theme, dimensions, fonts, limits, media resolution, and repaint hooks.
- [ ] Stub media resolution for this phase.
- [ ] Construct a minimal `Iv::RichPage` paragraph directly in C++.
- [ ] Run the native preparation code, layout it at a requested width, and paint
      it into a `QImage`.
- [ ] Write the image and geometry metadata from a command-line native host.
- [ ] Keep every TDesktop source modification in an explicit patch.

### Verification

- [ ] The harness performs no Telegram authentication or network request.
- [ ] The harness exits cleanly under AddressSanitizer and UndefinedBehaviorSanitizer.
- [ ] Paragraph geometry matches the in-TDesktop reference.
- [ ] Its native output matches the golden image, allowing only a documented
      difference caused by a known host configuration discrepancy.
- [ ] Removing a required upstream dependency causes an explicit build failure,
      not a silent alternate renderer.

### Exit gate

A sessionless native executable renders one real Telegram rich paragraph using
the native preparation/layout/paint path.

## Phase 3 — Retire the Qt/WebAssembly build risk early

### Objective

Compile the minimal Phase 2 renderer to WebAssembly and display its native
pixels in a browser.

### Implementation

- [ ] Build a minimal Qt-for-WebAssembly toolchain matching the pinned Qt
      revision as closely as supported.
- [ ] Start single-threaded to avoid making cross-origin isolation a prerequisite.
- [ ] Compile the minimal renderer and its required TDesktop sources with
      Emscripten.
- [ ] Initialize the required Qt application/runtime without a Telegram
      session.
- [ ] Paint into `QImage`, expose width/height/stride/RGBA memory, and blit it to
      `<canvas>` with `ImageData`.
- [ ] Export explicit allocation and cleanup functions.
- [ ] Add a plain static smoke-test page with one hard-coded paragraph fixture.
- [ ] Record binary size, compressed transfer size, initialization time, render
      time, and peak WASM memory as measurements, not release blockers yet.

### Verification

- [ ] The page works with the network disabled after all static assets are loaded.
- [ ] No DOM element participates in Telegram text/block layout.
- [ ] Repeated rendering does not grow live allocations indefinitely.
- [ ] The browser output has the same dimensions and line breaks as the native
      harness.
- [ ] Chromium and Firefox complete the smoke test; Safari/WebKit is added as
      soon as the build is viable there.

### Escalation path if blocked

1. Expand the extracted native dependency set while preserving the same
   renderer.
2. Use a broader Qt/WASM application shell if a headless shell is insufficient.
3. Use Qt's software/raster paint path into `QImage` if the browser graphics
   backend changes output.
4. Build a larger TDesktop subsystem rather than substituting DOM/CSS layout.

### Exit gate

The actual Telegram article renderer produces visible browser canvas output.
This is the core feasibility proof.

## Phase 4 — Stabilize the renderer core and ABI

### Objective

Turn the spike into a maintainable native/WASM component with a versioned input
and output contract.

### Implementation

- [ ] Define canonical rich-message JSON schema version 1.
- [ ] Implement canonical JSON -> C++ -> `Iv::RichPage` conversion.
- [ ] Represent media references independently of `PhotoData` and
      `DocumentData` at the public boundary.
- [ ] Add a sessionless limits provider whose values are pinned to the target
      TDesktop/Bot API revision.
- [ ] Define structured errors and warnings for invalid input, unsupported
      blocks, truncation, missing media, and renderer failures.
- [ ] Add a stable C ABI with renderer handles and explicit ownership.
- [ ] Add renderer configuration for width, theme, scale, and direction.
- [ ] Return geometry and hit-target metadata alongside pixels.
- [ ] Make re-rendering incremental at the host level: retain initialized
      runtime, caches, fonts, and theme objects while replacing content.
- [ ] Add cancellation/supersession semantics in the TypeScript wrapper.

### Verification

- [ ] Schema validation fixtures pass in TypeScript and C++.
- [ ] Fuzz malformed JSON and deeply nested structures without crashes or
      unbounded allocations.
- [ ] Native and WASM hosts consume exactly the same canonical fixture files.
- [ ] ABI tests detect leaks, use-after-free, double-free, and stale handles.
- [ ] Every diagnostic includes fixture/input location where possible.

### Exit gate

The renderer is a versioned component rather than an application-specific
prototype.

## Phase 5 — Complete non-media block fidelity

### Objective

Support all text, structural, table, formula, and other non-media rich blocks
through the native renderer.

### Implementation

- [ ] Add blocks one family at a time:
  1. paragraphs, headings, footer, divider, anchor;
  2. quotes and pull quotes;
  3. preformatted/code and syntax highlighting;
  4. ordered, unordered, task, and nested lists;
  5. tables, alignment, spans, borders, and stripes;
  6. details/disclosure blocks;
  7. mathematical expressions;
  8. map and thinking/other non-media blocks supported by the pinned types.
- [ ] Add all inline text entities and entity combinations.
- [ ] Implement deterministic font loading and fallback ordering.
- [ ] Reproduce TDesktop rich-message limits and unsupported-block behavior.
- [ ] Implement RTL and mixed-direction behavior.
- [ ] Add interaction state for spoilers and open/closed details.

### Verification per block family

- [ ] Canonical structure matches expected fixtures.
- [ ] Prepared native structure matches the in-TDesktop reference.
- [ ] Final width, height, block rectangles, and line breaks match.
- [ ] Light and dark render diffs pass at all canonical widths.
- [ ] Narrow, long, nested, malformed, and maximum-limit cases pass.
- [ ] Native and WASM results are compared separately to the in-TDesktop
      baseline; matching each other is not sufficient by itself.

### Exit gate

Every non-media grammY rich block in the pinned specification has passing
semantic, geometric, and visual fixtures.

## Phase 6 — Implement the grammY `InputRichMessage` adapter

### Objective

Translate grammY's published rich-message types into the renderer's canonical
schema without losing meaning.

### Implementation

- [ ] Pin the exact `grammy` and `grammy_types` revisions used to generate the
      adapter compatibility matrix.
- [ ] Accept `InputRichMessageWithoutUpload` for pure JSON use and a browser
      media resolver for upload-bearing forms.
- [ ] Implement direct `blocks` -> canonical structure translation.
- [ ] Preserve discriminants, nested lists, tables, text entities, media IDs,
      captions, dimensions, spoilers, and block-specific flags.
- [ ] Generate exhaustive TypeScript switch statements so newly added union
      members fail compilation until mapped.
- [ ] Return source-path diagnostics such as `blocks[2].items[1].text`.
- [ ] Publish adapters separately from the C++ schema so grammY version updates
      do not require rebuilding WASM when the canonical meaning is unchanged.

### Verification

- [ ] Every `InputRichBlock` union member has an adapter test.
- [ ] Type-level exhaustiveness tests fail when a fixture introduces an unknown
      block discriminant.
- [ ] Round-trip tests preserve all representable fields:
      grammY object -> canonical JSON -> C++ `Iv::RichPage` debug form.
- [ ] Bot API limit errors are surfaced before rendering where possible.
- [ ] A selected set of block objects sent through a secret-gated Telegram test
      bot produces returned rich-message structures consistent with the local
      adapter.

### Exit gate

Users can paste or construct grammY `InputRichMessage` block objects and obtain
a faithful native-rendered preview.

## Phase 7 — Add HTML and Markdown input semantics

### Objective

Let users type HTML or Markdown while clearly distinguishing local parser
behavior from Telegram server normalization.

### Implementation

- [ ] Expose TDesktop's HTML parsing/import conversion used by
      `BlocksFromHtmlSource` through an adapter. Note (verified against
      `v7.0.9`): this function is **not** session-free — its signature is
      `BlocksFromHtmlSource(not_null<Main::Session*> session, const QString&
      html, const QString& basePath, const RichMessageLimits&, int usedBlocks)`,
      so the session is taken at the top-level entry, not only for media.
- [ ] Trace what the `Main::Session*` is actually used for inside
      `BlocksFromHtmlSource` before assuming any seam. It delegates to
      `TextUtilities::BlocksFromHtml()` and an `ImportContext`; determine
      whether the session-free seam is `TextUtilities::BlocksFromHtml` (or a
      lower layer) and build the sessionless adapter there, stubbing/faking the
      session-bound remainder (e.g. media/base-path resolution) with a
      deterministic fixture context.
- [ ] Do not treat "pure HTML parsing vs session-bound media resolution" as a
      given separation; prove where it lies during extraction and record it,
      since the public entry point does not expose it.
- [ ] Feed imported blocks into the same canonical/native rendering path.
- [ ] Add Markdown using TDesktop's applicable rich-message parsing path where
      one exists; otherwise document and isolate the chosen parser.
- [ ] Return parser diagnostics, truncation, unsupported constructs, and source
      ranges to the editor.
- [ ] Label preview modes accurately:
  - **blocks**: direct grammY/Bot API structure preview;
  - **HTML (TDesktop import)**: TDesktop local-import semantics;
  - **server-verified HTML**: structure verified by an opt-in test-bot round trip.
- [ ] Build a secret-gated integration harness that sends fixtures only to a
      configured private test chat, retrieves the resulting structure, and
      compares it with local parsing. Keep this out of ordinary/offline use.

### Verification

- [ ] Import -> render fixtures cover every supported HTML tag and attribute.
- [ ] HTML exported by TDesktop and imported back produces equivalent native
      rich content for supported features.
- [ ] Invalid HTML cannot hang or exhaust WASM memory.
- [ ] Local-vs-server parser differences are recorded as explicit fixtures,
      not hidden behind visual similarity.
- [ ] Ordinary browser preview performs no Telegram network request.

### Exit gate

HTML and Markdown editors have deterministic preview behavior, and the UI does
not overstate parser fidelity where server normalization differs.

## Phase 8 — Add media without a Telegram session

### Objective

Render photos, video thumbnails, audio/voice blocks, collage, and slideshow
content using browser-provided assets.

### Implementation

- [ ] Define media handles in canonical JSON and a separate binary asset
      registration API.
- [ ] Implement an in-memory `MediaRuntime`/resolver compatible with the native
      article renderer.
- [ ] Decode supported images in a deterministic path available to native and
      WASM builds.
- [ ] Generate or supply deterministic thumbnails/posters for video and audio
      fixtures.
- [ ] Support captions, aspect ratios, spoilers, collage ordering, and
      slideshow state.
- [ ] Trigger repaint/relayout when an asynchronously decoded asset becomes
      available.
- [ ] Enforce byte, pixel-dimension, item-count, and decode-time limits.
- [ ] Revoke browser object URLs and release C++/WASM media buffers when replaced.

### Verification

- [ ] Media block geometry matches native references before and after load.
- [ ] Missing, corrupt, huge, and unsupported media fail safely with diagnostics.
- [ ] Replacing media repeatedly returns memory to the steady-state envelope.
- [ ] Collage/slideshow order and crop behavior match reference captures.
- [ ] Spoiler and reveal states match the recorded behavior.

### Exit gate

All grammY media-bearing rich blocks render without a Telegram session.

## Phase 9 — Harden fidelity, performance, and browser support

### Objective

Move from feature completeness to a reliable shared renderer artifact.

### Implementation

- [ ] Run the full native and WASM fixture matrix in CI.
- [ ] Classify visual differences into:
  - semantic/layout failure;
  - color/style failure;
  - glyph/font-selection failure;
  - antialiasing-only difference;
  - approved intentional difference.
- [ ] Establish pixel thresholds only after measuring unavoidable raster-only
      differences. Never let a broad threshold hide wrapping or geometry changes.
- [ ] Test supported Chromium, Firefox, and WebKit/Safari versions.
- [ ] Measure cold initialization, warm render latency, rapid edit bursts,
      memory high-water mark, and compressed asset size.
- [ ] Add debounce and latest-request-wins scheduling in the SDK.
- [ ] Split or lazy-load optional resources where it does not alter rendering.
- [ ] Test keyboard navigation and expose a textual accessibility representation
      beside the canvas without using it for visual layout.
- [ ] Add CSP-compatible loading and avoid `eval`-dependent glue.

### Verification

- [ ] No semantic or geometric mismatch is waived as “pixel noise.”
- [ ] A long rapid-typing test does not leak WASM, JS, canvas, or media memory.
- [ ] Rendering failure leaves the editor responsive and displays diagnostics.
- [ ] Browser output remains functional when offline after assets are cached.
- [ ] Every approved golden update identifies the upstream or deliberate change
      that caused it.

### Exit gate

The renderer has a documented support matrix, stable performance envelope, and
automated fidelity gate.

## Phase 10 — Package the shared browser SDK

### Objective

Produce one consumable artifact for both product integrations.

### Implementation

- [ ] Package ESM glue, TypeScript declarations, `.wasm`, fonts/assets, schema,
      and revision metadata together.
- [ ] Provide APIs for initialization, render, resize, theme switch, media
      registration, interaction, cancellation, and disposal.
- [ ] Provide a small canvas host component with no framework dependency.
- [ ] Document bundling for Deno/Fresh, ordinary browser builds, and VS Code
      webviews.
- [ ] Use content-hashed assets and expose a renderer version string.
- [ ] Publish corresponding source/build instructions as required by the
      licensing decision from Phase 0.
- [ ] Package the TDesktop-derived renderer as its **own GPL-licensed artifact**
      with its own LICENSE and written source-offer, per the Phase 0 decision.
      Consumers (including the MIT VS Code extension) load it as an
      arm's-length component; the package must not require vendoring TDesktop
      sources into an MIT consumer.
- [ ] Add a minimal example app that imports only the released package.

### Verification

- [ ] The example app builds in a clean checkout without TDesktop source present.
- [ ] The distributed WASM checksum matches the release manifest.
- [ ] Package consumers cannot accidentally load mismatched JS and WASM versions.
- [ ] The GPL renderer artifact ships with its own LICENSE and source-offer, and
      an MIT consumer can depend on it without inheriting GPL obligations into
      its own sources (loaded component, not statically combined).
- [ ] Init/render/dispose integration tests pass under all supported browsers.

### Exit gate

A versioned renderer SDK can be consumed without copying implementation code
into either product.

## Phase 11 — Integrate into `telegram.tools`

### Objective

Ship an in-browser rich-message editor with live native preview.

### Implementation

- [ ] Add a dedicated Fresh route and Preact island for the editor.
- [ ] Load the shared SDK and WASM from versioned static assets.
- [ ] Provide three input modes: grammY blocks/JSON, HTML, and Markdown.
- [ ] Use an editor with syntax highlighting, diagnostics, and source-location
      navigation.
- [ ] Render on a debounced latest-request-wins loop.
- [ ] Add width, theme, scale, direction, and interaction-state controls.
- [ ] Display renderer/TDesktop revision and fidelity mode near the preview.
- [ ] Display parser/limit/unsupported diagnostics separately from visual output.
- [ ] Add shareable examples without uploading private editor content by default.
- [ ] Keep the first production build single-threaded unless measured performance
      justifies COOP/COEP and shared-memory complexity.

### Verification

- [ ] Route-level tests load JS, WASM, fonts, and workers from production paths.
- [ ] Editing each fixture updates the preview and diagnostics correctly.
- [ ] Browser refresh, back/forward navigation, and theme changes do not leak
      renderer instances.
- [ ] Production CSP permits required assets without unsafe script execution.
- [ ] A deployment smoke test runs the golden subset against the deployed build.
- [ ] The tool works locally and offline after its assets have been loaded.

### Exit gate

Users can write rich-message content in `telegram.tools` and see the native
TDesktop content rendering live.

## Phase 12 — Integrate into the grammY VS Code extension

### Objective

Show the same preview while a developer edits grammY code.

### Implementation

- [ ] Add a Rich Message Preview webview using the extension's existing webview
      infrastructure.
- [ ] Consume the exact same SDK/WASM release used by `telegram.tools`, as the
      **separately licensed GPL renderer artifact** (Phase 0 decision). The MIT
      extension loads it at runtime and ships it alongside its own code with the
      renderer's LICENSE and source-offer intact — it does not vendor TDesktop
      sources or relicense the artifact. Keep the extension's own sources MIT.
- [ ] Load assets using VS Code webview URIs and a strict nonce-based CSP.
- [ ] Add commands such as:
  - `grammY: Open Rich Message Preview`;
  - `grammY: Preview Selected Rich Message`;
  - `grammY: Pin Rich Message Preview`.
- [ ] Use the TypeScript compiler/language-service API to recognize supported
      grammY rich-message object literals and relevant API calls.
- [ ] Statically evaluate only safe constructs: literals, object/array literals,
      template literals with resolvable constants, and imported constants whose
      values can be proven. Never execute workspace code to obtain a preview.
- [ ] For dynamic expressions, render the known portion and report precise
      “cannot statically evaluate” diagnostics.
- [ ] Send canonical JSON from the extension host to the webview; keep parsing
      and workspace access out of the webview.
- [ ] Re-run extraction incrementally on document changes and render only the
      newest successful request.
- [ ] Support JavaScript and TypeScript first, then add Deno/project-layout
      variations explicitly.

### Verification

- [ ] Extension-host tests cover supported AST forms and rejected dynamic forms.
- [ ] No workspace source is evaluated or imported at runtime for previewing.
- [ ] Webview integration tests verify WASM loading, CSP, resize, theme, and
      disposal.
- [ ] Typing rapidly does not queue stale renders or block the extension host.
- [ ] The same canonical fixture produces the same pixels in the website and
      VS Code webview for the same render profile.
- [ ] Multi-root workspaces and multiple preview panels keep state isolated.

### Exit gate

Supported grammY code updates a faithful native preview while the developer
types, with safe diagnostics for code that cannot be statically resolved.

## Phase 13 — Optional complete chat-message rendering

### Objective

Extend content fidelity to full Telegram Desktop message presentation.

### Implementation

- [ ] Define a separate fake-message schema containing sender, direction,
      timestamp, reply, forward information, reactions, views, edit status, and
      delivery status.
- [ ] Construct a synthetic `HistoryItem` or the smallest equivalent model
      accepted by `HistoryView::Message`.
- [ ] Replace remaining session/network dependencies with deterministic context
      interfaces and fixture data.
- [ ] Paint the complete message element into the off-screen surface.
- [ ] Add controls for incoming/outgoing, group/private/channel context, bubble
      width, sender color, and status.
- [ ] Keep this as a distinct render mode so content-only consumers do not pay
      its dependency or binary-size cost unless needed.

### Verification

- [ ] Capture independent goldens from unmodified TDesktop chat history using
      matching synthetic fixture content.
- [ ] Verify bubble geometry, tails, timestamp placement, sender/reply headers,
      reactions, and status icons independently from rich-content geometry.
- [ ] Confirm the content renderer remains unchanged when chat chrome is added.
- [ ] Measure and document the extra WASM size and initialization cost.

### Exit gate

The browser can reproduce a complete pinned-version TDesktop message view, not
only its rich-message article content.

## Phase 14 — Upstream tracking and long-term maintenance

### Objective

Keep fidelity as Telegram Desktop and grammY evolve.

### Implementation

- [ ] Record upstream revision in every renderer release.
- [ ] Automate detection of changes to imported TDesktop source files, style
      definitions, block enums, and grammY rich-message union types.
- [ ] Rebase the patch stack one upstream release at a time.
- [ ] Require the full fixture matrix before changing the declared TDesktop
      compatibility version.
- [ ] Generate a compatibility report listing supported TDesktop, Bot API,
      grammY types, browser, and VS Code versions.
- [ ] Preserve prior renderer releases so users can intentionally preview an
      older Telegram Desktop version.
- [ ] Add a process for reviewing and approving golden changes.

### Verification

- [ ] A deliberate upstream enum/style change makes CI fail until reviewed.
- [ ] Old fixture sets remain reproducible with their matching renderer release.
- [ ] Release notes distinguish source-semantic, layout, and raster-only changes.
- [ ] Corresponding source and notices remain accessible for every distributed
      WASM version.

### Exit gate

Updating the renderer is a controlled, reviewable compatibility exercise rather
than an ad hoc re-extraction.

---

## 8. Continuous verification pipeline

Run these layers in increasing cost order:

1. **Type and schema tests** — grammY unions, canonical JSON, diagnostics.
2. **C++ unit tests** — conversion, limits, preparation, ownership.
3. **Native render tests** — geometry and deterministic PNG output.
4. **WASM render tests** — headless browser dimensions, pixels, memory.
5. **Independent TDesktop comparisons** — prove extraction still matches the
   unmodified application.
6. **Product integration tests** — `telegram.tools` and VS Code webview.
7. **Secret-gated server oracle tests** — detect HTML/Markdown normalization
   differences; never required for ordinary offline preview.

Every visual failure should retain these CI artifacts:

- input fixture;
- canonical JSON;
- prepared/debug structure;
- reference PNG;
- candidate PNG;
- amplified diff PNG;
- geometry diff;
- renderer and upstream revision metadata.

## 9. Definition of done for the main goal

The main goal is complete when all of the following are true:

- [ ] The visual preview is produced by TDesktop-derived native rendering code
      compiled to WASM, not a DOM recreation.
- [ ] Every pinned grammY `InputRichBlock` type is either supported with passing
      fixtures or clearly reported as unsupported.
- [ ] `blocks`, HTML, and Markdown input modes state and meet their fidelity
      guarantees.
- [ ] Native TDesktop, native extracted, and WASM comparison gates pass for the
      declared reference profiles.
- [ ] Media, themes, widths, LTR/RTL, and interaction states have automated tests.
- [ ] `telegram.tools` and the VS Code extension consume the same renderer SDK
      and produce the same preview for the same request.
- [ ] Preview operation is local/offline and requires no Telegram credentials.
- [ ] Distributed artifacts satisfy the recorded source and licensing obligations.
- [ ] Upstream revisions and known fidelity limitations are visible to users.

## 10. Recommended first working sequence

Do not begin with the website or VS Code UI. The shortest sequence that proves
the central idea is:

1. pin and build TDesktop `v7.0.9`;
2. capture one paragraph through unmodified `RichDraftPreview`;
3. render that paragraph through a sessionless native `MarkdownArticle` harness;
4. compile the harness to WASM;
5. blit its `QImage` pixels to a browser canvas;
6. compare all three outputs;
7. only then expand the block matrix and input adapters.

That sequence leaves a working artifact at every step and tests the hardest
assumption—native Telegram rendering in a browser—before product integration
work begins.

## 11. Source anchors

- [Telegram Desktop `v7.0.9` RichDraftPreview](https://github.com/telegramdesktop/tdesktop/blob/v7.0.9/Telegram/SourceFiles/history/view/controls/history_view_rich_draft_preview.cpp)
- [Telegram Desktop `Iv::RichPage`](https://github.com/telegramdesktop/tdesktop/blob/v7.0.9/Telegram/SourceFiles/iv/iv_rich_page.h)
- [Native rich-page preparation](https://github.com/telegramdesktop/tdesktop/blob/v7.0.9/Telegram/SourceFiles/iv/markdown/iv_markdown_prepare_native_blocks.cpp)
- [MarkdownArticle interface](https://github.com/telegramdesktop/tdesktop/blob/v7.0.9/Telegram/SourceFiles/iv/markdown/iv_markdown_article.h)
- [MarkdownArticle painting](https://github.com/telegramdesktop/tdesktop/blob/v7.0.9/Telegram/SourceFiles/iv/markdown/iv_markdown_article_paint.cpp)
- [TDesktop HTML import](https://github.com/telegramdesktop/tdesktop/blob/v7.0.9/Telegram/SourceFiles/iv/editor/iv_editor_clipboard_import.cpp)
- [grammY rich-message exports](https://github.com/grammyjs/grammY/blob/9f5474fc3a8aa6ee96fd2e178fd1f7e0b6617e38/src/types.web.ts)
- [telegram.tools](https://github.com/grammyjs/telegram.tools)
- [grammY VS Code extension](https://github.com/grammyjs/vscode)
- [Qt for WebAssembly documentation](https://doc.qt.io/qt-6/wasm.html)
