/**
 * Typed surface of the Emscripten-generated WASM module and its C ABI.
 *
 * The exported C symbols are defined in `renderer/cpp/wasm-host/ttr_abi.h`
 * and must stay in lockstep with this file; both sides carry the ABI
 * version below and `ttr_abi_version()` is checked at load time.
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

export const ABI_VERSION = 1;

/** Subset of the Emscripten Module object the SDK relies on. */
export interface EmscriptenModule {
  HEAPU8: Uint8Array;
  cwrap(
    name: string,
    returnType: string | null,
    argTypes: string[],
  ): (...args: (number | string)[]) => number | string;
  _malloc(size: number): number;
  _free(pointer: number): void;
  UTF8ToString(pointer: number): string;
}

/** cwrap'ped C ABI. All strings crossing the boundary are UTF-8 JSON. */
export interface RendererAbi {
  /** () → ABI version; must equal {@link ABI_VERSION}. */
  abiVersion(): number;
  /** () → 0 on success; initializes the Qt runtime once per module. */
  initialize(): number;
  /** () → static JSON: {"rendererVersion", "tdesktopRevision"}. */
  version(): string;
  /** () → renderer context handle (> 0) or 0 on failure. */
  create(): number;
  /** (handle) → void; frees the context and all registered media. */
  destroy(handle: number): void;
  /**
   * (handle, ref, bytesPtr, bytesSize, mimeType) → 0 on success.
   * The renderer copies the bytes; the caller frees bytesPtr.
   */
  registerMedia(
    handle: number,
    ref: string,
    bytesPointer: number,
    bytesSize: number,
    mimeType: string,
  ): number;
  /** (handle) → void; drops all registered media buffers. */
  clearMedia(handle: number): void;
  /**
   * (handle, requestJson) → pointer to a NUL-terminated UTF-8 JSON result:
   * {status, width, height, pixelsOffset, pixelsSize, diagnostics, geometry,
   *  hitTargets, rendererVersion, tdesktopRevision}.
   * pixelsOffset/pixelsSize address the RGBA buffer inside WASM memory; the
   * buffer and the returned string stay valid until the next render on the
   * same handle or destroy(). Returns 0 only on allocation failure.
   */
  render(handle: number, requestJson: string): number;
}

export function bindAbi(module: EmscriptenModule): RendererAbi {
  const abiVersion = module.cwrap("ttr_abi_version", "number", []);
  const initialize = module.cwrap("ttr_initialize", "number", []);
  const version = module.cwrap("ttr_version", "string", []);
  const create = module.cwrap("ttr_create", "number", []);
  const destroy = module.cwrap("ttr_destroy", null, ["number"]);
  const registerMedia = module.cwrap("ttr_register_media", "number", [
    "number",
    "string",
    "number",
    "number",
    "string",
  ]);
  const clearMedia = module.cwrap("ttr_clear_media", null, ["number"]);
  const render = module.cwrap("ttr_render", "number", ["number", "string"]);
  return {
    abiVersion: () => abiVersion() as number,
    initialize: () => initialize() as number,
    version: () => version() as string,
    create: () => create() as number,
    destroy: (handle) => void destroy(handle),
    registerMedia: (handle, ref, pointer, size, mime) =>
      registerMedia(handle, ref, pointer, size, mime) as number,
    clearMedia: (handle) => void clearMedia(handle),
    render: (handle, requestJson) => render(handle, requestJson) as number,
  };
}

export type ModuleFactory = (options: {
  locateFile?: (path: string) => string;
}) => Promise<EmscriptenModule>;

export interface LoadModuleOptions {
  /** URL of the Emscripten ESM glue (default export: module factory). */
  glueUrl: string;
  /** Maps emitted asset names (the .wasm file) to their served URLs. */
  locateFile?: (path: string) => string;
}

export class RendererUnavailableError extends Error {
  constructor(message: string, options?: { cause?: unknown }) {
    super(message, options);
    this.name = "RendererUnavailableError";
  }
}

/** Loads the Emscripten glue + WASM. Throws RendererUnavailableError when the
 * artifact is missing or fails to initialize (the artifact ships separately
 * from application code; see renderer/LICENSING.md). */
export async function loadModule(
  options: LoadModuleOptions,
): Promise<EmscriptenModule> {
  let factory: ModuleFactory;
  try {
    const glue = await import(options.glueUrl);
    factory = (glue.default ?? glue.createModule) as ModuleFactory;
  } catch (cause) {
    throw new RendererUnavailableError(
      `renderer WASM glue not loadable from ${options.glueUrl}`,
      { cause },
    );
  }
  if (typeof factory !== "function") {
    throw new RendererUnavailableError(
      `renderer glue at ${options.glueUrl} does not export a module factory`,
    );
  }
  try {
    return await factory({ locateFile: options.locateFile });
  } catch (cause) {
    throw new RendererUnavailableError(
      "renderer WASM module failed to instantiate",
      { cause },
    );
  }
}
