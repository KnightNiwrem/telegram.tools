/**
 * Public renderer contract (plan §7), schema version 1.
 *
 * The request/result shapes here cross the TypeScript↔WASM boundary as JSON
 * plus one RGBA pixel buffer. They are versioned together with
 * `renderer/schema/canonical-rich-message.v1.schema.json`.
 *
 * Deviation from the plan's representative sketch, recorded deliberately:
 * `RenderRequest.content` is a tagged union covering the three input modes
 * (canonical blocks, TDesktop-imported HTML, Markdown) instead of a single
 * `richMessage` field, so Phase 7 input modes use the same contract.
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

import type {
  CanonicalRichMessage,
  RenderDiagnostic,
} from "../canonical/mod.ts";

export type { CanonicalRichMessage, RenderDiagnostic };

export type RenderTheme = "light" | "dark";
export type RenderDirection = "auto" | "ltr" | "rtl";

export type RenderContent =
  | { mode: "blocks"; richMessage: CanonicalRichMessage }
  // html is parsed inside the artifact by TDesktop's own import path
  // (sessionless: media is dropped with a renderer.html-media-dropped
  // warning). markdown has no TDesktop parsing path at v7.0.9 and returns
  // renderer.mode-unsupported until a parser choice is recorded.
  | { mode: "html"; source: string; skipEntityDetection?: boolean }
  | { mode: "markdown"; source: string; skipEntityDetection?: boolean };

/** Interaction state replayed into a render (plan Phase 5). */
export interface InteractionState {
  /** Anchor ids of details blocks whose open state is toggled from default. */
  toggledDetails?: string[];
  /** True once spoilers have been revealed by the user. */
  spoilersRevealed?: boolean;
  /** Active item index per grouped-media block index (slideshow state). */
  slideshowIndices?: Record<string, number>;
}

export interface RenderRequest {
  schemaVersion: 1;
  content: RenderContent;
  /** Logical pixels; canonical profile widths are 320, 480, 720. */
  viewportWidth: number;
  theme: RenderTheme;
  /** Device pixel ratio, 1..4; canonical profile uses 1. Layout stays in
   * logical pixels — only the output bitmap is rendered at
   * viewportWidth*scale. */
  scale: number;
  direction?: RenderDirection;
  interactionState?: InteractionState;
}

export interface BlockGeometry {
  /** Index into the prepared segment list (native article order). */
  segmentIndex: number;
  /** Canonical block kind, or "html-import" derived kinds for HTML mode. */
  kind: string;
  x: number;
  y: number;
  width: number;
  height: number;
  lineCount?: number;
}

export interface HitTarget {
  kind: "link" | "spoiler" | "details-toggle" | "task-marker" | "media";
  x: number;
  y: number;
  width: number;
  height: number;
  /** Link URL, anchor id, or media ref, depending on kind. */
  data?: string;
}

export interface RenderResult {
  /** Physical pixels (viewportWidth*scale); logical size is width/scale.
   * geometry and hitTargets stay in logical pixels. */
  width: number;
  height: number;
  /** Device pixel ratio the bitmap was rendered at. Absent from artifacts
   * that predate per-request scale (those always render at 1). */
  scale?: number;
  /** Tightly packed RGBA, width*height*4 bytes (copied out of WASM memory). */
  rgba: Uint8Array;
  diagnostics: RenderDiagnostic[];
  geometry: BlockGeometry[];
  hitTargets: HitTarget[];
  rendererVersion: string;
  tdesktopRevision: string;
}

/** Media bytes registered under a canonical media handle. */
export interface MediaAsset {
  ref: string;
  bytes: Uint8Array;
  mimeType: string;
}

export interface RendererVersionInfo {
  rendererVersion: string;
  tdesktopRevision: string;
}
