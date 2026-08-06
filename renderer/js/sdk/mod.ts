/**
 * Public TypeScript SDK for the telegram.tools rich-message renderer
 * (plan Phase 10). Consumers: the telegram.tools editor island and the
 * grammY VS Code extension webview.
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

export * from "./types.ts";
export {
  ABI_VERSION,
  type LoadModuleOptions,
  RendererUnavailableError,
} from "./module.ts";
export {
  RenderCancelledError,
  RichMessageRenderer,
  type RichMessageRendererOptions,
} from "./renderer.ts";
export { blitToCanvas } from "./canvas.ts";
export {
  LIMITS_V1,
  SCHEMA_VERSION,
  validateCanonicalMessage,
} from "../canonical/mod.ts";
