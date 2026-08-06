/**
 * Focused unit tests for `flattenRichText` (plan Phase 1/6): UTF-16
 * offset/length semantics, entity nesting, custom emoji and inline math
 * coverage, and zero-length anchors.
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

import { assertEquals } from "$std/assert/mod.ts";
import { flattenRichText } from "../js/adapter-grammy/richtext.ts";

Deno.test("offsets and lengths are UTF-16 code units for astral-plane emoji", () => {
  // U+1F600 GRINNING FACE occupies two UTF-16 code units.
  const { richText, diagnostics } = flattenRichText(
    ["\u{1F600}", { type: "bold", text: "\u{1F600}\u{1F600}" }, " tail"],
    "$",
  );
  assertEquals(diagnostics, []);
  assertEquals(richText.text, "\u{1F600}\u{1F600}\u{1F600} tail");
  assertEquals(richText.text.length, 11);
  assertEquals(richText.entities, [
    { type: "bold", offset: 2, length: 4 },
  ]);
});

Deno.test("nested wrappers produce exactly nested ranges, innermost first", () => {
  const { richText, diagnostics } = flattenRichText(
    {
      type: "bold",
      text: {
        type: "italic",
        text: { type: "underline", text: "abc" },
      },
    },
    "$",
  );
  assertEquals(diagnostics, []);
  assertEquals(richText.text, "abc");
  // Children finish flattening before their wrapper records its range, so
  // the innermost entity appears first.
  assertEquals(richText.entities, [
    { type: "underline", offset: 0, length: 3 },
    { type: "italic", offset: 0, length: 3 },
    { type: "bold", offset: 0, length: 3 },
  ]);
});

Deno.test("partially nested wrappers keep correct offsets", () => {
  const { richText } = flattenRichText(
    [
      "a ",
      { type: "bold", text: ["b ", { type: "italic", text: "bi" }] },
      " z",
    ],
    "$",
  );
  assertEquals(richText.text, "a b bi z");
  assertEquals(richText.entities, [
    { type: "italic", offset: 4, length: 2 },
    { type: "bold", offset: 2, length: 4 },
  ]);
});

Deno.test("custom emoji alternative text becomes the covered text", () => {
  const { richText, diagnostics } = flattenRichText(
    [
      "Nice ",
      {
        type: "custom_emoji",
        custom_emoji_id: "5368324170671202286",
        alternative_text: "\u{1F44D}",
      },
      "!",
    ],
    "$",
  );
  assertEquals(diagnostics, []);
  assertEquals(richText.text, "Nice \u{1F44D}!");
  assertEquals(richText.entities, [
    {
      type: "custom_emoji",
      offset: 5,
      length: 2, // the surrogate pair of the alternative emoji
      customEmojiId: "5368324170671202286",
    },
  ]);
});

Deno.test("inline math covers its LaTeX source in the flat text", () => {
  const expression = "x^2 + y^2 = z^2";
  const { richText, diagnostics } = flattenRichText(
    ["Formula: ", { type: "mathematical_expression", expression }, " done"],
    "$",
  );
  assertEquals(diagnostics, []);
  assertEquals(richText.text, `Formula: ${expression} done`);
  assertEquals(richText.entities, [
    {
      type: "math",
      offset: 9,
      length: expression.length,
      expression,
    },
  ]);
});

Deno.test("anchors are zero-length entities at the current position", () => {
  const { richText, diagnostics } = flattenRichText(
    [
      { type: "anchor", name: "top" },
      "before ",
      { type: "anchor", name: "middle" },
      "after",
    ],
    "$",
  );
  assertEquals(diagnostics, []);
  assertEquals(richText.text, "before after");
  assertEquals(richText.entities, [
    { type: "anchor", offset: 0, length: 0, name: "top" },
    { type: "anchor", offset: 7, length: 0, name: "middle" },
  ]);
});

Deno.test("plain string input produces no entities key", () => {
  const { richText, diagnostics } = flattenRichText("plain", "$");
  assertEquals(diagnostics, []);
  assertEquals(richText, { text: "plain" });
});
