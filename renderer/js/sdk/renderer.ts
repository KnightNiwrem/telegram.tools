/**
 * High-level renderer host: owns one WASM renderer context, serializes
 * requests across the C ABI, copies pixel results out of WASM memory, and
 * schedules renders latest-request-wins so rapid typing cannot queue stale
 * frames (plan Phases 4 and 9).
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

import type { RenderDiagnostic } from "../canonical/mod.ts";
import {
  ABI_VERSION,
  bindAbi,
  type EmscriptenModule,
  loadModule,
  type LoadModuleOptions,
  type RendererAbi,
  RendererUnavailableError,
} from "./module.ts";
import type {
  MediaAsset,
  RendererVersionInfo,
  RenderRequest,
  RenderResult,
} from "./types.ts";

interface RawRenderResult {
  status: "ok" | "error";
  width?: number;
  height?: number;
  pixelsOffset?: number;
  pixelsSize?: number;
  diagnostics?: RenderDiagnostic[];
  geometry?: RenderResult["geometry"];
  hitTargets?: RenderResult["hitTargets"];
  rendererVersion?: string;
  tdesktopRevision?: string;
}

export class RenderCancelledError extends Error {
  constructor() {
    super("render superseded by a newer request");
    this.name = "RenderCancelledError";
  }
}

export interface RichMessageRendererOptions extends LoadModuleOptions {}

export class RichMessageRenderer {
  #module: EmscriptenModule;
  #abi: RendererAbi;
  #handle: number;
  #disposed = false;
  #versionInfo: RendererVersionInfo;
  /** Chain that serializes renders; each entry checks for supersession. */
  #pending: Promise<unknown> = Promise.resolve();
  #latestRequestId = 0;

  private constructor(
    module: EmscriptenModule,
    abi: RendererAbi,
    handle: number,
    versionInfo: RendererVersionInfo,
  ) {
    this.#module = module;
    this.#abi = abi;
    this.#handle = handle;
    this.#versionInfo = versionInfo;
  }

  /** Loads the WASM artifact and initializes one renderer context. The
   * module and context are retained across renders (plan §7: repeated
   * renders reuse the initialized module and renderer context). */
  static async load(
    options: RichMessageRendererOptions,
  ): Promise<RichMessageRenderer> {
    const module = await loadModule(options);
    const abi = bindAbi(module);
    const abiVersion = abi.abiVersion();
    if (abiVersion !== ABI_VERSION) {
      throw new RendererUnavailableError(
        `renderer ABI version mismatch: artifact has ${abiVersion}, SDK expects ${ABI_VERSION}`,
      );
    }
    const initResult = abi.initialize();
    if (initResult !== 0) {
      throw new RendererUnavailableError(
        `renderer runtime initialization failed with code ${initResult}`,
      );
    }
    const versionInfo = JSON.parse(abi.version()) as RendererVersionInfo;
    const handle = abi.create();
    if (handle === 0) {
      throw new RendererUnavailableError("renderer context creation failed");
    }
    return new RichMessageRenderer(module, abi, handle, versionInfo);
  }

  get versionInfo(): RendererVersionInfo {
    return this.#versionInfo;
  }

  /** Registers media bytes under a canonical handle. Replacing a handle
   * frees the previous buffer inside the renderer. */
  registerMedia(asset: MediaAsset): void {
    this.#assertUsable();
    const pointer = this.#module._malloc(asset.bytes.length);
    try {
      this.#module.HEAPU8.set(asset.bytes, pointer);
      const status = this.#abi.registerMedia(
        this.#handle,
        asset.ref,
        pointer,
        asset.bytes.length,
        asset.mimeType,
      );
      if (status !== 0) {
        throw new Error(
          `media registration failed for ${asset.ref} with code ${status}`,
        );
      }
    } finally {
      this.#module._free(pointer);
    }
  }

  clearMedia(): void {
    this.#assertUsable();
    this.#abi.clearMedia(this.#handle);
  }

  /**
   * Renders a request. Latest-request-wins: if another render() is issued
   * before this one starts, this one rejects with RenderCancelledError
   * without ever crossing the WASM boundary.
   */
  render(request: RenderRequest): Promise<RenderResult> {
    this.#assertUsable();
    const requestId = ++this.#latestRequestId;
    const run = this.#pending.then(() => {
      if (this.#disposed) {
        throw new RendererUnavailableError("renderer disposed");
      }
      if (requestId !== this.#latestRequestId) throw new RenderCancelledError();
      return this.#renderNow(request);
    });
    // Keep the chain alive regardless of individual outcomes.
    this.#pending = run.catch(() => undefined);
    return run;
  }

  #renderNow(request: RenderRequest): RenderResult {
    const resultPointer = this.#abi.render(
      this.#handle,
      JSON.stringify(request),
    );
    if (resultPointer === 0) {
      throw new Error("renderer returned no result (allocation failure)");
    }
    const raw = JSON.parse(
      this.#module.UTF8ToString(resultPointer),
    ) as RawRenderResult;
    const diagnostics = raw.diagnostics ?? [];
    if (raw.status !== "ok") {
      // Structured failure: dimensions are absent, diagnostics explain why.
      return {
        width: 0,
        height: 0,
        rgba: new Uint8Array(0),
        diagnostics: diagnostics.length > 0 ? diagnostics : [{
          severity: "error",
          code: "renderer.failed",
          message: "renderer reported failure without diagnostics",
        }],
        geometry: [],
        hitTargets: [],
        rendererVersion: raw.rendererVersion ??
          this.#versionInfo.rendererVersion,
        tdesktopRevision: raw.tdesktopRevision ??
          this.#versionInfo.tdesktopRevision,
      };
    }
    const pixelsOffset = raw.pixelsOffset ?? 0;
    const pixelsSize = raw.pixelsSize ?? 0;
    // Copy out: the WASM-side buffer is only valid until the next render.
    const rgba = this.#module.HEAPU8.slice(
      pixelsOffset,
      pixelsOffset + pixelsSize,
    );
    return {
      width: raw.width ?? 0,
      height: raw.height ?? 0,
      rgba,
      diagnostics,
      geometry: raw.geometry ?? [],
      hitTargets: raw.hitTargets ?? [],
      rendererVersion: raw.rendererVersion ?? this.#versionInfo.rendererVersion,
      tdesktopRevision: raw.tdesktopRevision ??
        this.#versionInfo.tdesktopRevision,
    };
  }

  dispose(): void {
    if (this.#disposed) return;
    this.#disposed = true;
    this.#abi.destroy(this.#handle);
  }

  #assertUsable(): void {
    if (this.#disposed) {
      throw new RendererUnavailableError("renderer disposed");
    }
  }
}
