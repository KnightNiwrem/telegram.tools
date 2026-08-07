/**
 * HTML / Markdown input interface (plan Phase 7, TypeScript side).
 *
 * HTML and Markdown parsing is performed by TDesktop's own import code
 * *inside* the renderer artifact (`Iv`/`TextUtilities` import path via the
 * sessionless seam described in renderer/UPSTREAM.md), never by a
 * JavaScript re-implementation. This module only prepares the request and
 * labels the fidelity mode accurately so the UI does not overstate parser
 * fidelity where Telegram-server normalization may differ.
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

import type { RenderContent } from "../sdk/types.ts";

/** Fidelity modes surfaced next to the preview (plan Phase 7). */
export type FidelityMode =
  | "blocks" // direct grammY/Bot API structure preview
  | "html-tdesktop-import" // TDesktop local-import semantics
  | "markdown-tdesktop-import" // TDesktop local Markdown semantics
  | "server-verified"; // structure verified by an opt-in test-bot round trip

export const FIDELITY_MODE_LABELS: Record<FidelityMode, string> = {
  "blocks": "Blocks — exact Bot API structure",
  "html-tdesktop-import":
    "HTML — Telegram Desktop local import semantics (server normalization may differ)",
  "markdown-tdesktop-import":
    "Markdown — Telegram Desktop local parsing semantics (server normalization may differ)",
  "server-verified": "Server-verified structure (test-bot round trip)",
};

export interface HtmlInputOptions {
  /** Mirrors grammY's `skip_entity_detection` (a server-side flag). The
   * local TDesktop import applies explicit markup only and performs no
   * automatic entity detection in either state, so this does not change
   * the preview; it is carried for API parity and the opt-in
   * server-verified mode. */
  skipEntityDetection?: boolean;
}

/** Prepares render content for raw rich-message HTML. */
export function htmlContent(
  source: string,
  options: HtmlInputOptions = {},
): { content: RenderContent; fidelityMode: FidelityMode } {
  return {
    content: {
      mode: "html",
      source,
      skipEntityDetection: options.skipEntityDetection,
    },
    fidelityMode: "html-tdesktop-import",
  };
}

/** Prepares render content for rich-message Markdown. */
export function markdownContent(
  source: string,
  options: HtmlInputOptions = {},
): { content: RenderContent; fidelityMode: FidelityMode } {
  return {
    content: {
      mode: "markdown",
      source,
      skipEntityDetection: options.skipEntityDetection,
    },
    fidelityMode: "markdown-tdesktop-import",
  };
}
