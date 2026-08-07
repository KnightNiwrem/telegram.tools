/// <reference lib="dom" />
/**
 * Framework-free canvas host: blits renderer RGBA output into a
 * HTMLCanvasElement (plan Phase 10: "a small canvas host component with no
 * framework dependency"). No DOM element participates in Telegram layout;
 * the canvas only displays pixels painted by the native code.
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

import type { RenderResult } from "./types.ts";

export interface BlitOptions {
  /** Device pixel ratio the result was rendered at (RenderRequest.scale). */
  scale?: number;
}

/** Copies a render result into the canvas, resizing it to fit. */
export function blitToCanvas(
  result: RenderResult,
  canvas: HTMLCanvasElement,
  options: BlitOptions = {},
): void {
  const scale = options.scale ?? 1;
  canvas.width = result.width;
  canvas.height = result.height;
  canvas.style.width = `${result.width / scale}px`;
  canvas.style.height = `${result.height / scale}px`;
  if (result.width === 0 || result.height === 0) return;
  const context = canvas.getContext("2d");
  if (context === null) {
    throw new Error("2d canvas context unavailable");
  }
  // Copy into a fresh non-shared buffer; ImageData rejects views over
  // SharedArrayBuffer-backed (threaded WASM) memory.
  const pixels = new Uint8ClampedArray(result.rgba.byteLength);
  pixels.set(result.rgba);
  const image = new ImageData(pixels, result.width, result.height);
  context.putImageData(image, 0, 0);
}
