# Licensing decision record (plan Phase 0 — blocking item)

## Facts

- Telegram Desktop is **GPL-3.0-only with the OpenSSL exception**. Anything
  compiled from its sources — including the renderer WASM — is a derivative
  work and must be distributed under GPL-3.0 terms.
- `grammyjs/telegram.tools` (this repository) is **AGPL-3.0**. AGPL-3.0
  section 13 permits combining with GPL-3.0 code; bundling the GPL-derived
  renderer artifact into the website is compatible.
- `grammyjs/vscode` is **MIT**. Statically bundling GPL-derived WASM into the
  extension would force the combined distributed work onto GPL/AGPL terms.

## Decision (recorded per plan Phase 0)

The TDesktop-derived renderer is shipped as a **separate, independently
distributed GPL-3.0-licensed artifact**:

1. The renderer lives under `renderer/` with its own `LICENSE` (GPL-3.0) and
   is versioned and released independently of the products that load it.
   The plan's preferred long-term home is a dedicated repository; for the
   first implementation it is a package directory in this repository (chosen
   during this branch's Phase 0 because both the plan document and the first
   product integration live here). Splitting it out later changes no
   boundary recorded in this file. This variance from the plan's "prefer a
   dedicated renderer repository" is deliberate and recorded here.
2. Every distributed renderer artifact (`.wasm` + JS glue + assets) is
   accompanied by:
   - the GPL-3.0 license text;
   - `UPSTREAM.md` (exact TDesktop revision and file inventory);
   - a written source offer: the corresponding source is this repository at
     the release tag, including the pinned submodule and patch stack, which
     is sufficient to rebuild the artifact with `scripts/build-wasm.sh`.
3. `telegram.tools` (AGPL-3.0) bundles the artifact directly. Compatible; no
   special boundary needed.
4. The MIT VS Code extension (plan Phase 12) must **load** the released
   renderer artifact at runtime as an arm's-length component (download at
   install time or ship side-by-side with the renderer's own LICENSE and
   source offer intact). It must not vendor TDesktop-derived sources, must
   not statically link/bundle the WASM into its own compiled output, and its
   own sources stay MIT.

## Open item (explicitly not signed off here)

The plan requires confirmation **with counsel** that the extension↔renderer
boundary is mere aggregation rather than a derivative combination for the
exact packaging form used. That is an organizational action that cannot be
completed inside this branch. Per the plan's blocking condition, **Phase 12
(VS Code integration) must not ship until that sign-off is recorded here.**
Phase 10 packaging follows the decision above and keeps the artifact
separately licensed so that either packaging outcome remains possible.

## Redistributed assets checklist

| Asset | Origin | License | Handling |
| --- | --- | --- | --- |
| Renderer WASM + native hosts | Compiled from TDesktop `v7.0.9` + patches | GPL-3.0 (OpenSSL exception) | Own LICENSE + source offer per above |
| Renderer JS SDK / adapters (`renderer/js/`) | Written for this project | GPL-3.0 (kept license-uniform with the artifact they drive) | Included in renderer releases |
| Canonical schema + fixtures | Written for this project | GPL-3.0 | Included in renderer releases |
| TDesktop sources | git submodule, unmodified | GPL-3.0 (OpenSSL exception) | Never vendored; patches in `renderer/patches/` carry the same license |
| Fonts | Open Sans / Vazirmatn as bundled by TDesktop (`Telegram/Resources/fonts`) | OFL-1.1 | Redistributable with attribution; enumerated in THIRD_PARTY_NOTICES.md before any release ships them |
| grammY type definitions | `grammy_types@v4.0.0` | MIT | Imported by URL at build time, not vendored |
