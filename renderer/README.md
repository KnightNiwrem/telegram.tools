# telegram.tools rich-message renderer

A browser-embeddable renderer for Telegram rich messages that reuses Telegram
Desktop's actual preparation, layout, and painting code
(`Iv::Markdown::MarkdownArticle` at the pinned `v7.0.9` revision), compiled to
WebAssembly. Implements `telegram-rich-message-wasm-implementation-plan.md` at
the repository root.

**License: GPL-3.0 with the OpenSSL exception** — this directory is a derivative
work of Telegram Desktop and is licensed separately from the rest of the
repository. See `LICENSE`, `LICENSING.md`, `UPSTREAM.md`, and
`THIRD_PARTY_NOTICES.md`.

## Layout

```text
upstream/tdesktop/   pinned TDesktop submodule (v7.0.9, a1e89e1f); never
                     edited in place
patches/             build-time patch stack (one opt-in CMake hook)
cpp/                 sessionless renderer: canonical JSON -> Iv::RichPage,
                     headless harness, native CLI host, WASM C ABI
schema/              canonical rich-message JSON Schema v1 + pinned limits
js/canonical/        TypeScript canonical model + limits validator
js/adapter-grammy/   grammY InputRichMessage -> canonical (exhaustive)
js/adapter-html/     HTML/Markdown input interface + fidelity-mode labels
js/sdk/              public SDK: WASM loader, C-ABI binding, scheduling,
                     canvas host
tests/fixtures/      Phase 1 fixture corpus (76 fixtures)
tests/               deno tests (93 passing): fixtures, flattener, limits
scripts/             build-native / build-wasm / capture-goldens /
                     compare-renders
```

## Status

See `BUILD-STATUS.md` for the precise, honest state of each plan phase,
including what is verified, what compiles, and what remains.

## Quick start (TypeScript layer)

```sh
deno test --allow-read renderer/tests/   # adapter + schema + limits tests
```

## WASM artifact

```sh
# emsdk must be activated at Qt-wasm's version (3.1.70 for Qt 6.9.2);
# a native build must exist first (host code generators).
renderer/scripts/build-wasm.sh
cp renderer/dist/ttr-renderer.{js,wasm} static/rich-message-renderer/
deno task start   # -> the editor at /
```

The artifact (52 MB, 37 MB gzipped — the ~26 MB of emoji sprite PNGs are the
dominant share and barely compress) is not committed; `static/` and
`renderer/dist/` are gitignored. `scripts/verify-wasm.sh` compares the browser
output against the native goldens (`--geometry-only` gates on render failures
and geometry only, with known differences listed in
`tests/wasm-known-differences.txt`).

## Continuous integration

`.github/workflows/renderer-wasm.yml` builds the artifact on a stock
`ubuntu-24.04` runner and uploads it as a build artifact:
`scripts/setup-ci-host.sh` (apt deps, source-built ada/rnnoise, shim CMake
configs) → aqtinstall Qt 6.9.2 (host + wasm) + emsdk 3.1.70 →
`build-native.sh --codegen-only` (host code generators only) → `build-wasm.sh` →
deno tests + `verify-wasm.sh --geometry-only` → publish as a GitHub release
pinned in `renderer/artifact.lock.json`. Deployments fetch the pinned release at
build time (`deno task fetch-renderer`, part of `deno task build`) instead of
committing the artifact.

## Native host

```sh
renderer/scripts/build-native.sh --deps  # install Ubuntu deps, then build
renderer/build/native/ttr_native_host \
    --request request.json --out out.png --meta out.json
```

## Renderer contract

`js/sdk/types.ts` defines the versioned request/result shapes; the C ABI in
`cpp/wasm-host/ttr_abi.h` and `js/sdk/module.ts` carry a lockstep ABI version.
Every render result reports the renderer version and the pinned TDesktop
revision.
