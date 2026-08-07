/**
 * Flattens grammY's recursive `RichText` tree into canonical flat text plus
 * offset/length entities (UTF-16 code units), matching TDesktop's
 * `TextWithEntities` representation.
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

import type { RichText } from "https://deno.land/x/grammy_types@v4.0.0/rich.ts";
import type {
  CanonicalEntity,
  CanonicalEntityType,
  CanonicalRichText,
  RenderDiagnostic,
} from "../canonical/mod.ts";

interface FlattenState {
  text: string;
  entities: CanonicalEntity[];
  diagnostics: RenderDiagnostic[];
}

/** Entity forms that simply wrap child text with no extra attributes. */
const PLAIN_WRAPPERS: Partial<Record<string, CanonicalEntityType>> = {
  bold: "bold",
  italic: "italic",
  underline: "underline",
  strikethrough: "strikethrough",
  spoiler: "spoiler",
  code: "code",
  subscript: "subscript",
  superscript: "superscript",
  marked: "marked",
};

function pushWrapped(
  state: FlattenState,
  path: string,
  child: RichText,
  entity: Omit<CanonicalEntity, "offset" | "length">,
): void {
  const start = state.text.length;
  flattenInto(child, path, state);
  const length = state.text.length - start;
  state.entities.push({ ...entity, offset: start, length });
}

function flattenInto(node: RichText, path: string, state: FlattenState): void {
  if (typeof node === "string") {
    state.text += node;
    return;
  }
  if (Array.isArray(node)) {
    node.forEach((child, index) =>
      flattenInto(child, `${path}[${index}]`, state)
    );
    return;
  }
  switch (node.type) {
    case "bold":
    case "italic":
    case "underline":
    case "strikethrough":
    case "spoiler":
    case "subscript":
    case "superscript":
    case "marked":
    case "code":
      pushWrapped(state, `${path}.text`, node.text, {
        type: PLAIN_WRAPPERS[node.type]!,
      });
      return;
    case "date_time":
      pushWrapped(state, `${path}.text`, node.text, {
        type: "date_time",
        unixTime: node.unix_time,
        format: node.date_time_format,
      });
      return;
    case "text_mention":
      pushWrapped(state, `${path}.text`, node.text, {
        type: "text_mention",
        userId: node.user.id,
        userFirstName: node.user.first_name,
      });
      return;
    case "custom_emoji": {
      // The alternative text becomes the covered text; TDesktop substitutes
      // the animated emoji at paint time via the custom emoji id.
      const start = state.text.length;
      state.text += node.alternative_text;
      state.entities.push({
        type: "custom_emoji",
        offset: start,
        length: node.alternative_text.length,
        customEmojiId: node.custom_emoji_id,
      });
      return;
    }
    case "mathematical_expression": {
      // Inline math covers its LaTeX source in the flat text; the renderer
      // replaces the covered range with the typeset expression.
      const start = state.text.length;
      state.text += node.expression;
      state.entities.push({
        type: "math",
        offset: start,
        length: node.expression.length,
        expression: node.expression,
      });
      return;
    }
    case "url":
      pushWrapped(state, `${path}.text`, node.text, {
        type: "url",
        url: node.url,
      });
      return;
    case "email_address":
      pushWrapped(state, `${path}.text`, node.text, {
        type: "email_address",
        emailAddress: node.email_address,
      });
      return;
    case "phone_number":
      pushWrapped(state, `${path}.text`, node.text, {
        type: "phone_number",
        phoneNumber: node.phone_number,
      });
      return;
    case "bank_card_number":
      pushWrapped(state, `${path}.text`, node.text, {
        type: "bank_card_number",
        bankCardNumber: node.bank_card_number,
      });
      return;
    case "mention":
      pushWrapped(state, `${path}.text`, node.text, {
        type: "mention",
        username: node.username,
      });
      return;
    case "hashtag":
      pushWrapped(state, `${path}.text`, node.text, {
        type: "hashtag",
        hashtag: node.hashtag,
      });
      return;
    case "cashtag":
      pushWrapped(state, `${path}.text`, node.text, {
        type: "cashtag",
        cashtag: node.cashtag,
      });
      return;
    case "bot_command":
      pushWrapped(state, `${path}.text`, node.text, {
        type: "bot_command",
        botCommand: node.bot_command,
      });
      return;
    case "anchor":
      state.entities.push({
        type: "anchor",
        offset: state.text.length,
        length: 0,
        name: node.name,
      });
      return;
    case "anchor_link":
      pushWrapped(state, `${path}.text`, node.text, {
        type: "anchor_link",
        anchorName: node.anchor_name,
      });
      return;
    case "reference":
      pushWrapped(state, `${path}.text`, node.text, {
        type: "reference",
        name: node.name,
      });
      return;
    case "reference_link":
      pushWrapped(state, `${path}.text`, node.text, {
        type: "reference_link",
        referenceName: node.reference_name,
      });
      return;
    default: {
      // Exhaustiveness guard: a grammy_types upgrade that adds a RichText
      // form fails compilation here until it is mapped (plan Phase 6).
      const exhaustive: never = node;
      state.diagnostics.push({
        severity: "error",
        code: "adapter.unknown-entity",
        message: `unknown rich text form ${
          JSON.stringify((exhaustive as { type?: unknown }).type)
        }`,
        path,
      });
    }
  }
}

export interface FlattenResult {
  richText: CanonicalRichText;
  diagnostics: RenderDiagnostic[];
}

/** Flattens a grammY `RichText` tree rooted at `path` (for diagnostics). */
export function flattenRichText(node: RichText, path: string): FlattenResult {
  const state: FlattenState = { text: "", entities: [], diagnostics: [] };
  flattenInto(node, path, state);
  const richText: CanonicalRichText = { text: state.text };
  if (state.entities.length > 0) richText.entities = state.entities;
  return { richText, diagnostics: state.diagnostics };
}
