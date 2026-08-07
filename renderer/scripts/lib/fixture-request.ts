/**
 * Shared fixture → RenderRequest content mapping for the golden scripts
 * (capture-goldens.sh, verify-goldens.sh, verify-wasm.sh). One definition
 * so the three scripts cannot silently diverge on which fixtures render
 * and what request they produce.
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

export interface RenderableFixture {
  expected?: { mode?: string; canonical?: unknown };
  input?: { html?: unknown };
}

/** Content for a fixture's render request, or null if it is not renderable
 * (invalid-mode and markdown fixtures have no renderer support yet). */
export function requestContentFor(
  fixture: RenderableFixture,
): Record<string, unknown> | null {
  const mode = fixture.expected?.mode;
  if (mode === "blocks") {
    return { mode, richMessage: fixture.expected?.canonical };
  }
  if (mode === "html" && typeof fixture.input?.html === "string") {
    return { mode, source: fixture.input.html };
  }
  return null;
}

/** Full render request at the canonical golden profile. */
export function goldenRequestFor(
  fixture: RenderableFixture,
  width: number,
): Record<string, unknown> | null {
  const content = requestContentFor(fixture);
  return content === null ? null : {
    schemaVersion: 1,
    content,
    viewportWidth: width,
    theme: "light",
    scale: 1,
  };
}
