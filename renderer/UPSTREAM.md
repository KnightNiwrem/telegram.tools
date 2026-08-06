# Upstream provenance

This directory contains a browser-embeddable WebAssembly renderer derived from
Telegram Desktop's rich-message preparation, layout, and painting code, per
`telegram-rich-message-wasm-implementation-plan.md` at the repository root.

Every upstream revision consumed by the renderer is pinned here. No source is
copied into this tree without an entry in this file.

## Telegram Desktop (primary upstream)

- Repository: <https://github.com/telegramdesktop/tdesktop>
- Tag: `v7.0.9`
- Immutable commit SHA: `a1e89e1f64f08cb058caf1c61ff43f319f98a6ec`
- Consumed as: git submodule at `renderer/upstream/tdesktop`, checked out at
  the SHA above. The submodule is the single source of truth; extraction
  changes live exclusively in `renderer/patches/` and are applied at build
  time. TDesktop sources are never edited in place and never vendored
  elsewhere in this repository.
- License: GPL-3.0-only with the OpenSSL exception (see
  `upstream/tdesktop/LEGAL`).

### Pinned desktop-app submodule revisions at `v7.0.9`

Recorded from `telegramdesktop/tdesktop@a1e89e1f` (`Telegram/` subdirectory):

| Submodule | SHA |
| --- | --- |
| `Telegram/codegen` | `51cc8c564555914f0cf2f9eba9e9ea9df339192a` |
| `Telegram/lib_base` | `82d182a275e197fd717fecc86193d9d91f4fc5b5` |
| `Telegram/lib_crl` | `7a165302fed408c84b2d1c2513e35a21a141da44` |
| `Telegram/lib_lottie` | `49d67cf66d3573cd71a3228af47a4ac59b64fc90` |
| `Telegram/lib_qr` | `6fdf60461444ba150e13ac36009c0ffce72c4c83` |
| `Telegram/lib_rpl` | `c57cccffb01d85570decd7fccb88419c9a682e63` |
| `Telegram/lib_spellcheck` | `1f18c1e35b99697fe58d48d7f5a88e96c928128e` |
| `Telegram/lib_storage` | `ccdc72548a5065b5991b4e06e610d76bc4f6023e` |
| `Telegram/lib_tl` | `aa7791326d9b8516fa0f8086599414c7bd645382` |
| `Telegram/lib_translate` | `09c10726220e6862ca91d39b5dec5119aa0177bc` |
| `Telegram/lib_ui` | `9e9e5e1fc5ba7e3f4d5407b484210db3b46aa53d` |
| `Telegram/lib_webrtc` | `52636e86eaa493de670daf71959d000b281bd153` |
| `Telegram/lib_webview` | `71c948902bfac5b25e90e1c9d1c8a34d1ee275c0` |

### TDesktop source files at the extraction seam

The renderer reuses (via the submodule + patch stack, not by copying):

- `Telegram/SourceFiles/iv/iv_rich_page.{h,cpp}` — `Iv::RichPage` model,
  `RichMessageLimits`, validation.
- `Telegram/SourceFiles/iv/markdown/iv_markdown_prepare_native_blocks.{h,cpp}`
  — `Iv::Markdown::PrepareNativeIvBlocks` / `UpdatePreparedNativeIvLeaf`
  (native preparation entry points; there is no `TryPrepareNativeInstantView`
  symbol at this revision).
- `Telegram/SourceFiles/iv/markdown/iv_markdown_article*.{h,cpp}` —
  `MarkdownArticle` layout and painting.
- `Telegram/SourceFiles/iv/markdown/iv_markdown_common.h`,
  `iv_markdown_prepare.h`, `iv_markdown_media_block.h` — prepared structures
  and the `MediaBlockHost` interface used by the sessionless media runtime.
- `Telegram/SourceFiles/history/view/controls/history_view_rich_draft_preview.*`
  — behavioral reference only (its `QWidget`/`Main::Session` integration is
  not extracted).
- `Telegram/lib_ui` (`Ui::Text`, styles, painting) and its transitive
  dependencies `lib_base`, `lib_rpl`, `lib_crl`, `lib_style` codegen — linked
  as libraries, unmodified except for patches recorded in `renderer/patches/`.

Extraction seam facts verified against `v7.0.9` sources:

- `MarkdownArticle(const style::Markdown&, std::shared_ptr<MathRenderer>)` and
  `PrepareNativeIvBlocks` take no `Main::Session`, `Window`, or `Delegate`;
  the paragraph path is session-free by construction.
- `Iv::BlocksFromHtmlSource(not_null<Main::Session*>, const QString&, const
  QString&, const RichMessageLimits&, int)` is **not** session-free at the
  top-level entry; the sessionless HTML-import seam must be established below
  it (see plan Phase 7) and is recorded as an open extraction risk.

## grammY (input type contract)

- Repository: <https://github.com/grammyjs/grammY>
- Pinned commit: `9f5474fc3a8aa6ee96fd2e178fd1f7e0b6617e38`
  (`src/types.web.ts` re-exports the rich-message types).
- Types package: `grammy_types` `v4.0.0`
  (<https://deno.land/x/grammy_types@v4.0.0/rich.ts>), which defines
  `InputRichMessage<F>`, `InputRichMessageMedia<F>`, `InputRichBlock<F>` (21
  union members), `RichText` (26 forms including plain strings and arrays),
  and the documented rich-message limits (32768 chars, 500 blocks, 16 nesting
  levels, 50 media, 20 table columns).
- License: MIT.

Exported `InputRichBlock` union members at the pinned revision (fixture
enumeration for plan Phase 1): `paragraph`, `heading`, `pre`, `footer`,
`divider`, `mathematical_expression`, `anchor`, `list`, `blockquote`,
`pullquote`, `collage`, `slideshow`, `table`, `details`, `map`, `animation`,
`audio`, `photo`, `video`, `voice_note`, `thinking`. Note: the plan's Phase 1
"confirmed exported leaf types" list was written against an older revision;
`grammy_types@v4.0.0` additionally exports distinct block types for `pre`,
`footer`, `divider`, `mathematical_expression`, `anchor`, `pullquote`,
`table`, `map`, and `thinking`, and does **not** export a separate
`InputRichBlockListItem`-as-block or `InputRichBlockSectionHeading` name for
`heading` (it is `InputRichBlockSectionHeading` with `type: "heading"`).
The fixture corpus follows the actual `v4.0.0` union.

## Product targets

- `grammyjs/telegram.tools` — this repository. Deno Fresh `1.6.8` / Preact
  application, license AGPL-3.0 (root `LICENSE`).
- `grammyjs/vscode` — VS Code extension with webview support, license MIT.
  Not modified in this branch; integration is plan Phase 12 and consumes the
  released renderer artifact only.

## Reference profile (canonical, per plan §4)

- TDesktop `v7.0.9` (`a1e89e1f64f08cb058caf1c61ff43f319f98a6ec`).
- Native reference platform: Linux x86-64.
- Qt: TDesktop's pinned Qt for release builds; the development/native harness
  in this environment currently uses distro Qt 6.9.2 (Ubuntu 25.10), which is
  recorded as a host configuration discrepancy for golden comparison until a
  pinned-Qt container build is available (see `BUILD-STATUS.md`).
- Device pixel ratio 1, UI scale 100%, themes: default light/dark, widths
  320/480/720, locale: English first.
