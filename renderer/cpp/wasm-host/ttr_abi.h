/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).

C ABI exported from the WASM artifact (plan Phase 4). Must stay in lockstep
with renderer/js/sdk/module.ts (ABI_VERSION there equals kAbiVersion in
core/version.h). All strings are NUL-terminated UTF-8; all JSON shapes are
documented in module.ts.

Ownership: the string returned by ttr_render and the pixel buffer it
addresses (pixelsOffset/pixelsSize into linear memory) belong to the
renderer context and stay valid until the next ttr_render on the same
handle or ttr_destroy. Callers never free them. Media bytes passed to
ttr_register_media are copied; the caller frees its own buffer.
*/
#pragma once

#include <cstdint>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define TTR_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define TTR_EXPORT
#endif

extern "C" {

TTR_EXPORT int ttr_abi_version(void);
TTR_EXPORT int ttr_initialize(void);
TTR_EXPORT const char *ttr_version(void);
TTR_EXPORT int ttr_create(void);
TTR_EXPORT void ttr_destroy(int handle);
TTR_EXPORT int ttr_register_media(
	int handle,
	const char *ref,
	const std::uint8_t *bytes,
	int size,
	const char *mime_type);
TTR_EXPORT void ttr_clear_media(int handle);
TTR_EXPORT const char *ttr_render(int handle, const char *request_json);

} // extern "C"
