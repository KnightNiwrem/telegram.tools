/**
 * grammY `InputRichMessage` → canonical rich-message adapter (plan Phase 6).
 *
 * Translates the published grammY/Bot API rich-message types (pinned to
 * grammy_types@v4.0.0, see renderer/UPSTREAM.md) into the renderer's
 * canonical schema without losing meaning. Every union member is handled by
 * an exhaustive switch; a grammy_types upgrade that adds a member fails
 * `deno check` here until it is mapped.
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

import type {
  InputRichBlock,
  InputRichBlockListItem,
  InputRichMessage,
  InputRichMessageMedia,
  RichBlockCaption,
  RichBlockTableCell,
  RichText,
} from "https://deno.land/x/grammy_types@v4.0.0/rich.ts";
import type {
  InputMediaAnimation,
  InputMediaAudio,
  InputMediaPhoto,
  InputMediaVideo,
  InputMediaVoiceNote,
} from "https://deno.land/x/grammy_types@v4.0.0/methods.ts";
import {
  type CanonicalBlock,
  type CanonicalCaption,
  type CanonicalListItem,
  type CanonicalMediaKind,
  type CanonicalMediaRef,
  type CanonicalRichMessage,
  type CanonicalRichText,
  type CanonicalTableCell,
  type CanonicalTableRow,
  LIMITS_V1,
  type RenderDiagnostic,
  SCHEMA_VERSION,
  validateCanonicalMessage,
} from "../canonical/mod.ts";
import { flattenRichText } from "./richtext.ts";

/** Where the bytes for a media handle come from. Sessionless by design:
 * `file_id` origins cannot be fetched without a Telegram session and render
 * as placeholders unless the application registers bytes for the handle. */
export type MediaOrigin =
  | { kind: "url"; url: string }
  | { kind: "file_id"; fileId: string }
  | { kind: "attach"; attachName: string }
  | { kind: "input_file"; file: unknown };

export interface MediaSource {
  /** Canonical media handle referenced from the message blocks. */
  ref: string;
  origin: MediaOrigin;
}

export type GrammyAdapterResult =
  | {
    mode: "blocks";
    canonical: CanonicalRichMessage;
    media: MediaSource[];
    diagnostics: RenderDiagnostic[];
  }
  | {
    mode: "html" | "markdown";
    source: string;
    /** The id-referenced media list from InputRichMessage.media. */
    media: InputRichMessageMedia<unknown>[];
    rtl: boolean;
    skipEntityDetection: boolean;
    diagnostics: RenderDiagnostic[];
  }
  | {
    mode: "invalid";
    diagnostics: RenderDiagnostic[];
  };

type AnyInputMedia =
  | InputMediaAnimation<unknown>
  | InputMediaAudio<unknown>
  | InputMediaPhoto<unknown>
  | InputMediaVideo<unknown>
  | InputMediaVoiceNote<unknown>;

interface AdapterContext {
  diagnostics: RenderDiagnostic[];
  media: MediaSource[];
  nextMediaIndex: number;
}

function warn(
  ctx: AdapterContext,
  code: string,
  message: string,
  path: string,
) {
  ctx.diagnostics.push({ severity: "warning", code, message, path });
}

function info(
  ctx: AdapterContext,
  code: string,
  message: string,
  path: string,
) {
  ctx.diagnostics.push({ severity: "info", code, message, path });
}

function adaptText(
  text: RichText,
  path: string,
  ctx: AdapterContext,
): CanonicalRichText {
  const { richText, diagnostics } = flattenRichText(text, path);
  ctx.diagnostics.push(...diagnostics);
  return richText;
}

function adaptCaption(
  caption: RichBlockCaption | undefined,
  path: string,
  ctx: AdapterContext,
): CanonicalCaption | undefined {
  if (caption === undefined) return undefined;
  const result: CanonicalCaption = {};
  if (caption.text !== undefined) {
    result.text = adaptText(caption.text, `${path}.text`, ctx);
  }
  if (caption.credit !== undefined) {
    result.credit = adaptText(caption.credit, `${path}.credit`, ctx);
  }
  return result;
}

function mediaOrigin(
  media: unknown,
  path: string,
  ctx: AdapterContext,
): MediaOrigin {
  if (typeof media === "string") {
    if (/^https?:\/\//i.test(media)) return { kind: "url", url: media };
    if (media.startsWith("attach://")) {
      return { kind: "attach", attachName: media.slice("attach://".length) };
    }
    return { kind: "file_id", fileId: media };
  }
  info(
    ctx,
    "adapter.input-file",
    "media supplied as an upload; register its bytes with the renderer under this handle",
    path,
  );
  return { kind: "input_file", file: media };
}

function adaptMedia(
  input: AnyInputMedia,
  expected: CanonicalMediaKind,
  spoilerOverride: boolean | undefined,
  path: string,
  ctx: AdapterContext,
): CanonicalMediaRef {
  const ref = `m${ctx.nextMediaIndex++}`;
  ctx.media.push({
    ref,
    origin: mediaOrigin(input.media, `${path}.media`, ctx),
  });
  if (input.caption !== undefined) {
    warn(
      ctx,
      "adapter.media-caption-ignored",
      "InputMedia captions are ignored in rich message blocks; use the block-level caption instead",
      `${path}.caption`,
    );
  }
  const result: CanonicalMediaRef = { ref, kind: expected };
  switch (input.type) {
    case "photo":
      if (input.has_spoiler) result.spoiler = true;
      break;
    case "video":
      if (input.width !== undefined) result.width = input.width;
      if (input.height !== undefined) result.height = input.height;
      if (input.duration !== undefined) result.duration = input.duration;
      if (input.has_spoiler) result.spoiler = true;
      break;
    case "animation":
      if (input.width !== undefined) result.width = input.width;
      if (input.height !== undefined) result.height = input.height;
      if (input.duration !== undefined) result.duration = input.duration;
      if (input.has_spoiler) result.spoiler = true;
      result.autoplay = true;
      result.loop = true;
      break;
    case "audio":
      if (input.duration !== undefined) result.duration = input.duration;
      if (input.title !== undefined) result.title = input.title;
      if (input.performer !== undefined) result.performer = input.performer;
      break;
    case "voice_note":
      if (input.duration !== undefined) result.duration = input.duration;
      break;
    default: {
      const exhaustive: never = input;
      warn(
        ctx,
        "adapter.unknown-media",
        `unknown input media type ${
          JSON.stringify((exhaustive as { type?: unknown }).type)
        }`,
        path,
      );
    }
  }
  if (spoilerOverride) result.spoiler = true;
  return result;
}

/** Collage/slideshow children must themselves be media blocks. */
function adaptGroupedItems(
  blocks: InputRichBlock<unknown>[],
  path: string,
  ctx: AdapterContext,
): CanonicalMediaRef[] {
  const items: CanonicalMediaRef[] = [];
  blocks.forEach((block, index) => {
    const itemPath = `${path}[${index}]`;
    switch (block.type) {
      case "photo":
        items.push(
          adaptMedia(
            block.photo as AnyInputMedia,
            "photo",
            undefined,
            itemPath,
            ctx,
          ),
        );
        break;
      case "video":
        items.push(
          adaptMedia(
            block.video as AnyInputMedia,
            "video",
            undefined,
            itemPath,
            ctx,
          ),
        );
        break;
      case "animation":
        items.push(
          adaptMedia(
            block.animation as AnyInputMedia,
            "animation",
            undefined,
            itemPath,
            ctx,
          ),
        );
        break;
      default:
        ctx.diagnostics.push({
          severity: "error",
          code: "adapter.grouped-media-kind",
          message:
            `collage/slideshow items must be photo, video, or animation blocks; got ${
              JSON.stringify(block.type)
            }`,
          path: itemPath,
        });
    }
  });
  return items;
}

function adaptListItem(
  item: InputRichBlockListItem<unknown>,
  path: string,
  ctx: AdapterContext,
): CanonicalListItem {
  const result: CanonicalListItem = {
    blocks: adaptBlocks(item.blocks, `${path}.blocks`, ctx),
  };
  if (item.has_checkbox) {
    result.taskState = item.is_checked ? "checked" : "unchecked";
  }
  if (item.value !== undefined) result.value = item.value;
  if (item.type !== undefined) result.numberingType = item.type;
  return result;
}

function adaptTableCell(
  cell: RichBlockTableCell,
  path: string,
  ctx: AdapterContext,
): CanonicalTableCell {
  const result: CanonicalTableCell = {
    align: cell.align,
    valign: cell.valign,
  };
  if (cell.text !== undefined) {
    result.text = adaptText(cell.text, `${path}.text`, ctx);
  }
  if (cell.is_header) result.header = true;
  if (cell.colspan !== undefined && cell.colspan > 1) {
    result.colspan = cell.colspan;
  }
  if (cell.rowspan !== undefined && cell.rowspan > 1) {
    result.rowspan = cell.rowspan;
  }
  return result;
}

function adaptBlock(
  block: InputRichBlock<unknown>,
  path: string,
  ctx: AdapterContext,
): CanonicalBlock {
  switch (block.type) {
    case "paragraph":
      return {
        kind: "paragraph",
        text: adaptText(block.text, `${path}.text`, ctx),
      };
    case "heading":
      return {
        kind: "heading",
        text: adaptText(block.text, `${path}.text`, ctx),
        level: block.size,
      };
    case "pre": {
      const result: CanonicalBlock = {
        kind: "pre",
        text: adaptText(block.text, `${path}.text`, ctx),
      };
      if (block.language !== undefined) result.language = block.language;
      return result;
    }
    case "footer":
      return {
        kind: "footer",
        text: adaptText(block.text, `${path}.text`, ctx),
      };
    case "divider":
      return { kind: "divider" };
    case "mathematical_expression":
      return { kind: "math", expression: block.expression };
    case "anchor":
      return { kind: "anchor", name: block.name };
    case "list": {
      // The Bot API input type has no explicit <ul>/<ol> discriminant; a
      // list is treated as ordered when any item carries ordered-list
      // numbering fields. Recorded as an info diagnostic because it is an
      // inference, not a stated property of the input.
      const ordered = block.items.some(
        (item) => item.value !== undefined || item.type !== undefined,
      );
      if (ordered) {
        info(
          ctx,
          "adapter.list-order-inferred",
          "list treated as ordered because items carry value/type numbering fields",
          path,
        );
      }
      return {
        kind: "list",
        ordered,
        items: block.items.map((item, index) =>
          adaptListItem(item, `${path}.items[${index}]`, ctx)
        ),
      };
    }
    case "blockquote": {
      const result: CanonicalBlock = {
        kind: "blockquote",
        blocks: adaptBlocks(block.blocks, `${path}.blocks`, ctx),
      };
      if (block.credit !== undefined) {
        result.credit = adaptText(block.credit, `${path}.credit`, ctx);
      }
      return result;
    }
    case "pullquote": {
      const result: CanonicalBlock = {
        kind: "pullquote",
        text: adaptText(block.text, `${path}.text`, ctx),
      };
      if (block.credit !== undefined) {
        result.credit = adaptText(block.credit, `${path}.credit`, ctx);
      }
      return result;
    }
    case "collage": {
      const result: CanonicalBlock = {
        kind: "collage",
        items: adaptGroupedItems(block.blocks, `${path}.blocks`, ctx),
      };
      const caption = adaptCaption(block.caption, `${path}.caption`, ctx);
      if (caption !== undefined) result.caption = caption;
      return result;
    }
    case "slideshow": {
      const result: CanonicalBlock = {
        kind: "slideshow",
        items: adaptGroupedItems(block.blocks, `${path}.blocks`, ctx),
      };
      const caption = adaptCaption(block.caption, `${path}.caption`, ctx);
      if (caption !== undefined) result.caption = caption;
      return result;
    }
    case "table": {
      const rows: CanonicalTableRow[] = block.cells.map((row, rowIndex) => ({
        cells: row.map((cell, cellIndex) =>
          adaptTableCell(cell, `${path}.cells[${rowIndex}][${cellIndex}]`, ctx)
        ),
      }));
      const result: CanonicalBlock = { kind: "table", rows };
      if (block.is_bordered) result.bordered = true;
      if (block.is_striped) result.striped = true;
      if (block.caption !== undefined) {
        result.caption = adaptText(block.caption, `${path}.caption`, ctx);
      }
      return result;
    }
    case "details": {
      const result: CanonicalBlock = {
        kind: "details",
        summary: adaptText(block.summary, `${path}.summary`, ctx),
        blocks: adaptBlocks(block.blocks, `${path}.blocks`, ctx),
      };
      if (block.is_open) result.open = true;
      return result;
    }
    case "map": {
      const result: CanonicalBlock = {
        kind: "map",
        latitude: block.location.latitude,
        longitude: block.location.longitude,
        zoom: block.zoom,
        width: block.width,
        height: block.height,
      };
      const caption = adaptCaption(block.caption, `${path}.caption`, ctx);
      if (caption !== undefined) result.caption = caption;
      return result;
    }
    case "animation": {
      const result: CanonicalBlock = {
        kind: "animation",
        media: adaptMedia(
          block.animation as AnyInputMedia,
          "animation",
          undefined,
          `${path}.animation`,
          ctx,
        ),
      };
      const caption = adaptCaption(block.caption, `${path}.caption`, ctx);
      if (caption !== undefined) result.caption = caption;
      return result;
    }
    case "audio": {
      const result: CanonicalBlock = {
        kind: "audio",
        media: adaptMedia(
          block.audio as AnyInputMedia,
          "audio",
          undefined,
          `${path}.audio`,
          ctx,
        ),
      };
      const caption = adaptCaption(block.caption, `${path}.caption`, ctx);
      if (caption !== undefined) result.caption = caption;
      return result;
    }
    case "photo": {
      const result: CanonicalBlock = {
        kind: "photo",
        media: adaptMedia(
          block.photo as AnyInputMedia,
          "photo",
          undefined,
          `${path}.photo`,
          ctx,
        ),
      };
      const caption = adaptCaption(block.caption, `${path}.caption`, ctx);
      if (caption !== undefined) result.caption = caption;
      return result;
    }
    case "video": {
      const result: CanonicalBlock = {
        kind: "video",
        media: adaptMedia(
          block.video as AnyInputMedia,
          "video",
          undefined,
          `${path}.video`,
          ctx,
        ),
      };
      const caption = adaptCaption(block.caption, `${path}.caption`, ctx);
      if (caption !== undefined) result.caption = caption;
      return result;
    }
    case "voice_note": {
      const result: CanonicalBlock = {
        kind: "voice_note",
        media: adaptMedia(
          block.voice_note as AnyInputMedia,
          "voice_note",
          undefined,
          `${path}.voice_note`,
          ctx,
        ),
      };
      const caption = adaptCaption(block.caption, `${path}.caption`, ctx);
      if (caption !== undefined) result.caption = caption;
      return result;
    }
    case "thinking":
      return {
        kind: "thinking",
        text: adaptText(block.text, `${path}.text`, ctx),
      };
    default: {
      // Exhaustiveness guard: a grammy_types upgrade that adds a block
      // union member fails compilation here until it is mapped.
      const exhaustive: never = block;
      const kind = (exhaustive as { type?: unknown }).type;
      ctx.diagnostics.push({
        severity: "error",
        code: "adapter.unknown-block",
        message: `unknown block type ${JSON.stringify(kind)}`,
        path,
      });
      return {
        kind: "unsupported",
        reason: `unknown block type ${String(kind)}`,
      };
    }
  }
}

function adaptBlocks(
  blocks: InputRichBlock<unknown>[],
  path: string,
  ctx: AdapterContext,
): CanonicalBlock[] {
  return blocks.map((block, index) =>
    adaptBlock(block, `${path}[${index}]`, ctx)
  );
}

/**
 * Adapts a grammY `InputRichMessage` (any of its three content forms) for
 * rendering. `blocks` input is translated to the canonical schema and
 * validated against the pinned limits; `html`/`markdown` input is passed
 * through for TDesktop's own import parsing inside the renderer (plan
 * Phase 7) together with its id-referenced media list.
 */
export function adaptInputRichMessage(
  message: InputRichMessage<unknown>,
  options: { validate?: boolean } = {},
): GrammyAdapterResult {
  const provided = (["blocks", "html", "markdown"] as const).filter(
    (field) => message[field] !== undefined,
  );
  if (provided.length !== 1) {
    return {
      mode: "invalid",
      diagnostics: [{
        severity: "error",
        code: "adapter.content-form",
        message: `exactly one of blocks, html, or markdown must be set; got ${
          provided.length === 0 ? "none" : provided.join(", ")
        }`,
        path: "$",
      }],
    };
  }
  if (message.html !== undefined || message.markdown !== undefined) {
    const mode = message.html !== undefined ? "html" : "markdown";
    return {
      mode,
      source: (message.html ?? message.markdown)!,
      media: message.media ?? [],
      rtl: message.is_rtl ?? false,
      skipEntityDetection: message.skip_entity_detection ?? false,
      diagnostics: [],
    };
  }
  const ctx: AdapterContext = { diagnostics: [], media: [], nextMediaIndex: 0 };
  if (message.media !== undefined && message.media.length > 0) {
    ctx.diagnostics.push({
      severity: "warning",
      code: "adapter.media-list-unused",
      message:
        "the top-level media list is only used by html/markdown content via tg:// links; blocks reference media directly",
      path: "$.media",
    });
  }
  const canonical: CanonicalRichMessage = {
    schemaVersion: SCHEMA_VERSION,
    blocks: adaptBlocks(message.blocks!, "blocks", ctx),
  };
  if (message.is_rtl) canonical.rtl = true;
  if (options.validate !== false) {
    const validation = validateCanonicalMessage(canonical, LIMITS_V1);
    ctx.diagnostics.push(...validation.diagnostics);
  }
  return {
    mode: "blocks",
    canonical,
    media: ctx.media,
    diagnostics: ctx.diagnostics,
  };
}

export { flattenRichText } from "./richtext.ts";
