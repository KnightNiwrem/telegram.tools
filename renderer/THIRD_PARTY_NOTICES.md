# Third-party notices

This renderer is a derivative work of Telegram Desktop and redistributes or
links the following third-party components. Provenance for source revisions is
in `UPSTREAM.md`; the distribution strategy is in `LICENSING.md`.

## Telegram Desktop

- Copyright the Telegram Desktop contributors.
- License: GPL-3.0-only with the OpenSSL exception (`upstream/tdesktop/LEGAL`,
  `upstream/tdesktop/LICENSE`).
- Used as: pinned submodule + patch stack; compiled into the native harness and
  the WASM artifact.

## desktop-app libraries (lib_ui, lib_base, lib_rpl, lib_crl, lib_style, codegen)

- Copyright the desktop-app contributors.
- License: GPL-3.0-only with the OpenSSL exception.
- Used as: submodules of the pinned TDesktop checkout, linked into the renderer.

## Qt

- License: LGPL-3.0 (as used by TDesktop Linux builds) / GPL-compatible.
- Native harness: distro Qt 6 packages (Ubuntu). WASM artifact: Qt for
  WebAssembly built per `scripts/build-wasm.sh`; the exact Qt revision used for
  any released artifact must be recorded in the release manifest.
- LGPL obligations are satisfied by the renderer's full corresponding-source
  offer (the entire build is open source).

## Fonts

- Open Sans (Apache-2.0/OFL depending on version bundled by TDesktop) and
  Vazirmatn (OFL-1.1) from `upstream/tdesktop/Telegram/Resources/fonts`.
- Any release that embeds font files must list the exact files and their license
  texts here before shipping. No release in this branch ships fonts yet.

## grammY / grammy_types

- Copyright the grammY contributors. License: MIT.
- Type definitions consumed at build/check time from
  `https://deno.land/x/grammy_types@v4.0.0/`; not redistributed.

## Emscripten runtime (WASM builds)

- License: MIT/University of Illinois. JS glue emitted by Emscripten is
  distributed with the artifact under its permissive license.
