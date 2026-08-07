/**
 * Unit tests for `validateCanonicalMessage` limits (plan Phase 1): every
 * `limit.*` diagnostic code is exercised, with at-limit controls that must
 * pass.
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

import { assert, assertEquals } from "$std/assert/mod.ts";
import {
  type CanonicalBlock,
  type CanonicalRichMessage,
  LIMITS_V1,
  SCHEMA_VERSION,
  validateCanonicalMessage,
} from "../js/canonical/mod.ts";

function message(blocks: CanonicalBlock[]): CanonicalRichMessage {
  return { schemaVersion: SCHEMA_VERSION, blocks };
}

function paragraph(text: string): CanonicalBlock {
  return { kind: "paragraph", text: { text } };
}

function photo(index: number): CanonicalBlock {
  return { kind: "photo", media: { ref: `m${index}`, kind: "photo" } };
}

function nestedQuotes(levels: number): CanonicalBlock {
  let block: CanonicalBlock = paragraph("innermost");
  for (let i = 1; i < levels; i++) {
    block = { kind: "blockquote", blocks: [block] };
  }
  return block;
}

function codes(result: { diagnostics: { code: string }[] }): string[] {
  return result.diagnostics.map((d) => d.code);
}

Deno.test("limit.blocks fires above 500 blocks and not at 500", () => {
  const over = validateCanonicalMessage(
    message(Array.from({ length: 501 }, (_, i) => paragraph(`b${i}`))),
  );
  assertEquals(over.valid, false);
  assert(codes(over).includes("limit.blocks"));
  assertEquals(over.blockCount, 501);

  const atLimit = validateCanonicalMessage(
    message(Array.from({ length: 500 }, (_, i) => paragraph(`b${i}`))),
  );
  assertEquals(atLimit.valid, true);
  assertEquals(atLimit.blockCount, 500);
});

Deno.test("limit.blocks counts nested list items and table rows", () => {
  // 1 list block + 499 items + 499 nested paragraphs = 999 blocks.
  const list: CanonicalBlock = {
    kind: "list",
    ordered: false,
    items: Array.from({ length: 499 }, (_, i) => ({
      blocks: [paragraph(`item ${i}`)],
    })),
  };
  const result = validateCanonicalMessage(message([list]));
  assertEquals(result.blockCount, 999);
  assertEquals(result.valid, false);
  assert(codes(result).includes("limit.blocks"));
});

Deno.test("limit.depth fires at depth 17 and not at depth 16", () => {
  const over = validateCanonicalMessage(message([nestedQuotes(17)]));
  assertEquals(over.valid, false);
  assert(codes(over).includes("limit.depth"));

  const atLimit = validateCanonicalMessage(message([nestedQuotes(16)]));
  assertEquals(atLimit.valid, true);
});

Deno.test("limit.media fires above 50 media refs and not at 50", () => {
  const over = validateCanonicalMessage(
    message(Array.from({ length: 51 }, (_, i) => photo(i))),
  );
  assertEquals(over.valid, false);
  assert(codes(over).includes("limit.media"));
  assertEquals(over.mediaCount, 51);

  const atLimit = validateCanonicalMessage(
    message(Array.from({ length: 50 }, (_, i) => photo(i))),
  );
  assertEquals(atLimit.valid, true);
});

Deno.test("limit.table-columns fires above 20 columns, counting colspan", () => {
  const over = validateCanonicalMessage(message([{
    kind: "table",
    rows: [{
      cells: [
        { text: { text: "wide" }, colspan: 19 },
        { text: { text: "a" } },
        { text: { text: "b" } },
      ],
    }],
  }]));
  assertEquals(over.valid, false);
  assert(codes(over).includes("limit.table-columns"));

  const atLimit = validateCanonicalMessage(message([{
    kind: "table",
    rows: [{
      cells: Array.from(
        { length: 20 },
        (_, i) => ({ text: { text: `c${i}` } }),
      ),
    }],
  }]));
  assertEquals(atLimit.valid, true);
});

Deno.test("limit.collage-items fires above 10 items for collage and slideshow", () => {
  const items = Array.from({ length: 11 }, (_, i) => ({
    ref: `m${i}`,
    kind: "photo" as const,
  }));
  const collage = validateCanonicalMessage(
    message([{ kind: "collage", items }]),
  );
  assertEquals(collage.valid, false);
  assert(codes(collage).includes("limit.collage-items"));

  const slideshow = validateCanonicalMessage(
    message([{ kind: "slideshow", items }]),
  );
  assertEquals(slideshow.valid, false);
  assert(codes(slideshow).includes("limit.collage-items"));

  const atLimit = validateCanonicalMessage(
    message([{ kind: "collage", items: items.slice(0, 10) }]),
  );
  assertEquals(atLimit.valid, true);
});

Deno.test("limit.map-size fires when width + height exceed 10000", () => {
  const over = validateCanonicalMessage(message([{
    kind: "map",
    latitude: 41.9,
    longitude: 12.5,
    zoom: 14,
    width: 6000,
    height: 5000,
  }]));
  assertEquals(over.valid, false);
  assert(codes(over).includes("limit.map-size"));

  const atLimit = validateCanonicalMessage(message([{
    kind: "map",
    latitude: 41.9,
    longitude: 12.5,
    zoom: 14,
    width: 5000,
    height: 5000,
  }]));
  assertEquals(atLimit.valid, true);
});

Deno.test("limit.map-ratio fires when the side ratio exceeds 20", () => {
  const over = validateCanonicalMessage(message([{
    kind: "map",
    latitude: 41.9,
    longitude: 12.5,
    zoom: 14,
    width: 4000,
    height: 100,
  }]));
  assertEquals(over.valid, false);
  assert(codes(over).includes("limit.map-ratio"));

  const atLimit = validateCanonicalMessage(message([{
    kind: "map",
    latitude: 41.9,
    longitude: 12.5,
    zoom: 14,
    width: 2000,
    height: 100,
  }]));
  assertEquals(atLimit.valid, true);
});

Deno.test("limit.length fires above 32768 UTF-16 units and not at 32768", () => {
  const over = validateCanonicalMessage(
    message([paragraph("x".repeat(LIMITS_V1.lengthLimit + 1))]),
  );
  assertEquals(over.valid, false);
  assert(codes(over).includes("limit.length"));
  assertEquals(over.textLength, LIMITS_V1.lengthLimit + 1);

  const atLimit = validateCanonicalMessage(
    message([paragraph("x".repeat(LIMITS_V1.lengthLimit))]),
  );
  assertEquals(atLimit.valid, true);
});

Deno.test("limit.length counts block and inline math expression sources", () => {
  const blockMath = validateCanonicalMessage(message([
    paragraph("x".repeat(LIMITS_V1.lengthLimit - 10)),
    { kind: "math", expression: "y".repeat(20) },
  ]));
  assertEquals(blockMath.valid, false);
  assert(codes(blockMath).includes("limit.length"));

  const inlineMath = validateCanonicalMessage(message([{
    kind: "paragraph",
    text: {
      text: "x".repeat(LIMITS_V1.lengthLimit),
      entities: [
        { type: "math", offset: 0, length: 5, expression: "z".repeat(5) },
      ],
    },
  }]));
  assertEquals(inlineMath.valid, false);
  assert(codes(inlineMath).includes("limit.length"));
});
