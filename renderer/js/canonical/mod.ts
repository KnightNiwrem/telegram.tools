/**
 * Canonical rich-message model, schema version 1.
 *
 * This is the TypeScript mirror of
 * `renderer/schema/canonical-rich-message.v1.schema.json`, which in turn
 * mirrors `Iv::RichPage` at TDesktop v7.0.9. Text is flat UTF-16 text plus
 * offset/length entities, matching TDesktop's `TextWithEntities`.
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

export const SCHEMA_VERSION = 1 as const;

export interface CanonicalRichMessage {
  schemaVersion: typeof SCHEMA_VERSION;
  rtl?: boolean;
  blocks: CanonicalBlock[];
}

export interface CanonicalRichText {
  text: string;
  entities?: CanonicalEntity[];
}

export type CanonicalEntityType =
  | "bold"
  | "italic"
  | "underline"
  | "strikethrough"
  | "spoiler"
  | "code"
  | "subscript"
  | "superscript"
  | "marked"
  | "date_time"
  | "text_mention"
  | "mention"
  | "hashtag"
  | "cashtag"
  | "bot_command"
  | "email_address"
  | "phone_number"
  | "bank_card_number"
  | "url"
  | "custom_emoji"
  | "math"
  | "anchor"
  | "anchor_link"
  | "reference"
  | "reference_link";

export interface CanonicalEntityBase {
  type: CanonicalEntityType;
  /** UTF-16 code unit offset into the flat text. */
  offset: number;
  /** UTF-16 code unit length. Zero for `anchor`. */
  length: number;
  url?: string;
  unixTime?: number;
  format?: string;
  userId?: number;
  userFirstName?: string;
  username?: string;
  hashtag?: string;
  cashtag?: string;
  botCommand?: string;
  emailAddress?: string;
  phoneNumber?: string;
  bankCardNumber?: string;
  customEmojiId?: string;
  expression?: string;
  name?: string;
  anchorName?: string;
  referenceName?: string;
}

export type CanonicalEntity = CanonicalEntityBase;

export interface CanonicalCaption {
  text?: CanonicalRichText;
  credit?: CanonicalRichText;
}

export type CanonicalMediaKind =
  | "photo"
  | "video"
  | "animation"
  | "audio"
  | "voice_note";

export interface CanonicalMediaRef {
  /** Handle for separately registered media bytes; 1-64 chars [A-Za-z0-9_-]. */
  ref: string;
  kind: CanonicalMediaKind;
  width?: number;
  height?: number;
  duration?: number;
  mimeType?: string;
  fileName?: string;
  title?: string;
  performer?: string;
  autoplay?: boolean;
  loop?: boolean;
  spoiler?: boolean;
}

export type CanonicalTaskState = "none" | "unchecked" | "checked";

export interface CanonicalListItem {
  blocks: CanonicalBlock[];
  taskState?: CanonicalTaskState;
  value?: number;
  numberingType?: "a" | "A" | "i" | "I" | "1";
}

export interface CanonicalTableCell {
  text?: CanonicalRichText;
  header?: boolean;
  colspan?: number;
  rowspan?: number;
  align?: "left" | "center" | "right";
  valign?: "top" | "middle" | "bottom";
}

export interface CanonicalTableRow {
  cells: CanonicalTableCell[];
}

export type CanonicalBlock =
  | { kind: "paragraph"; text: CanonicalRichText }
  | { kind: "heading"; text: CanonicalRichText; level: 1 | 2 | 3 | 4 | 5 | 6 }
  | { kind: "pre"; text: CanonicalRichText; language?: string }
  | { kind: "footer"; text: CanonicalRichText }
  | { kind: "divider" }
  | { kind: "math"; expression: string }
  | { kind: "anchor"; name: string }
  | { kind: "list"; ordered: boolean; items: CanonicalListItem[] }
  | { kind: "blockquote"; blocks: CanonicalBlock[]; credit?: CanonicalRichText }
  | { kind: "pullquote"; text: CanonicalRichText; credit?: CanonicalRichText }
  | { kind: "collage"; items: CanonicalMediaRef[]; caption?: CanonicalCaption }
  | {
    kind: "slideshow";
    items: CanonicalMediaRef[];
    caption?: CanonicalCaption;
  }
  | {
    kind: "table";
    rows: CanonicalTableRow[];
    bordered?: boolean;
    striped?: boolean;
    caption?: CanonicalRichText;
  }
  | {
    kind: "details";
    summary: CanonicalRichText;
    blocks: CanonicalBlock[];
    open?: boolean;
  }
  | {
    kind: "map";
    latitude: number;
    longitude: number;
    zoom: number;
    width: number;
    height: number;
    caption?: CanonicalCaption;
  }
  | { kind: "photo"; media: CanonicalMediaRef; caption?: CanonicalCaption }
  | { kind: "video"; media: CanonicalMediaRef; caption?: CanonicalCaption }
  | { kind: "animation"; media: CanonicalMediaRef; caption?: CanonicalCaption }
  | { kind: "audio"; media: CanonicalMediaRef; caption?: CanonicalCaption }
  | { kind: "voice_note"; media: CanonicalMediaRef; caption?: CanonicalCaption }
  | { kind: "thinking"; text: CanonicalRichText }
  | { kind: "unsupported"; reason?: string };

export type CanonicalBlockKind = CanonicalBlock["kind"];

/** Diagnostic reported by adapters, validation, or the renderer itself. */
export interface RenderDiagnostic {
  severity: "error" | "warning" | "info";
  /** Stable machine-readable code, e.g. "limit.blocks" or "adapter.unknown-block". */
  code: string;
  message: string;
  /** Source path into the original input, e.g. "blocks[2].items[1].text". */
  path?: string;
}

/** Rich-message limits pinned to the target TDesktop revision (schema/limits.v1.json). */
export interface RichMessageLimits {
  lengthLimit: number;
  maxBlocks: number;
  maxDepth: number;
  maxMedia: number;
  maxTableCols: number;
  collageMaxItems: number;
}

export const LIMITS_V1: RichMessageLimits = {
  lengthLimit: 32768,
  maxBlocks: 500,
  maxDepth: 16,
  maxMedia: 50,
  maxTableCols: 20,
  collageMaxItems: 10,
};

const MEDIA_KINDS: readonly string[] = [
  "photo",
  "video",
  "animation",
  "audio",
  "voice_note",
];

const MEDIA_REF_PATTERN = /^[A-Za-z0-9_-]{1,64}$/;

interface ValidationContext {
  diagnostics: RenderDiagnostic[];
  /** Blocks counted so far, including nested blocks/list items/table rows. */
  blockCount: number;
  mediaCount: number;
  textLength: number;
  limits: RichMessageLimits;
}

function error(
  ctx: ValidationContext,
  code: string,
  message: string,
  path: string,
) {
  ctx.diagnostics.push({ severity: "error", code, message, path });
}

function validateRichText(
  text: unknown,
  path: string,
  ctx: ValidationContext,
): void {
  if (typeof text !== "object" || text === null) {
    error(ctx, "schema.type", "expected a rich text object", path);
    return;
  }
  const t = text as CanonicalRichText;
  if (typeof t.text !== "string") {
    error(ctx, "schema.type", "rich text requires a string `text`", path);
    return;
  }
  ctx.textLength += t.text.length;
  if (t.entities === undefined) return;
  if (!Array.isArray(t.entities)) {
    error(ctx, "schema.type", "`entities` must be an array", path);
    return;
  }
  t.entities.forEach((entity, index) => {
    const entityPath = `${path}.entities[${index}]`;
    if (typeof entity !== "object" || entity === null) {
      error(ctx, "schema.type", "expected an entity object", entityPath);
      return;
    }
    if (
      typeof entity.offset !== "number" ||
      typeof entity.length !== "number" ||
      entity.offset < 0 ||
      entity.length < 0 ||
      !Number.isInteger(entity.offset) ||
      !Number.isInteger(entity.length)
    ) {
      error(
        ctx,
        "schema.entity-range",
        "entity offset/length must be non-negative integers",
        entityPath,
      );
      return;
    }
    if (entity.offset + entity.length > t.text.length) {
      error(
        ctx,
        "schema.entity-range",
        `entity range ${entity.offset}+${entity.length} exceeds text length ${t.text.length}`,
        entityPath,
      );
    }
    switch (entity.type) {
      case "custom_emoji":
        // The alternative text IS the covered text in the canonical form,
        // so it is already counted once via the flat text above.
        break;
      case "math":
        ctx.textLength += (entity.expression ?? "").length;
        break;
      case "anchor":
        if (entity.length !== 0) {
          error(
            ctx,
            "schema.entity-range",
            "anchor entities must have zero length",
            entityPath,
          );
        }
        break;
      default:
        break;
    }
  });
}

function validateCaption(
  caption: unknown,
  path: string,
  ctx: ValidationContext,
): void {
  if (caption === undefined) return;
  if (typeof caption !== "object" || caption === null) {
    error(ctx, "schema.type", "expected a caption object", path);
    return;
  }
  const c = caption as CanonicalCaption;
  if (c.text !== undefined) validateRichText(c.text, `${path}.text`, ctx);
  if (c.credit !== undefined) validateRichText(c.credit, `${path}.credit`, ctx);
}

function validateMediaRef(
  media: unknown,
  path: string,
  ctx: ValidationContext,
): void {
  ctx.mediaCount += 1;
  if (typeof media !== "object" || media === null) {
    error(ctx, "schema.type", "expected a media reference object", path);
    return;
  }
  const m = media as CanonicalMediaRef;
  if (typeof m.ref !== "string" || !MEDIA_REF_PATTERN.test(m.ref)) {
    error(
      ctx,
      "schema.media-ref",
      "media `ref` must be 1-64 characters of A-Z, a-z, 0-9, _ or -",
      `${path}.ref`,
    );
  }
  if (!MEDIA_KINDS.includes(m.kind)) {
    error(
      ctx,
      "schema.media-kind",
      `unknown media kind ${JSON.stringify(m.kind)}`,
      `${path}.kind`,
    );
  }
}

function validateBlock(
  block: unknown,
  path: string,
  depth: number,
  ctx: ValidationContext,
): void {
  ctx.blockCount += 1;
  if (depth > ctx.limits.maxDepth) {
    error(
      ctx,
      "limit.depth",
      `nesting depth ${depth} exceeds the limit of ${ctx.limits.maxDepth}`,
      path,
    );
    return;
  }
  if (typeof block !== "object" || block === null) {
    error(ctx, "schema.type", "expected a block object", path);
    return;
  }
  const b = block as CanonicalBlock;
  switch (b.kind) {
    case "paragraph":
    case "footer":
    case "thinking":
      validateRichText(b.text, `${path}.text`, ctx);
      break;
    case "heading":
      validateRichText(b.text, `${path}.text`, ctx);
      if (!Number.isInteger(b.level) || b.level < 1 || b.level > 6) {
        error(
          ctx,
          "schema.range",
          "heading level must be an integer from 1 to 6",
          `${path}.level`,
        );
      }
      break;
    case "pre":
      validateRichText(b.text, `${path}.text`, ctx);
      break;
    case "divider":
      break;
    case "math":
      if (typeof b.expression !== "string") {
        error(
          ctx,
          "schema.type",
          "math blocks require a string `expression`",
          `${path}.expression`,
        );
      } else {
        ctx.textLength += b.expression.length;
      }
      break;
    case "anchor":
      if (typeof b.name !== "string") {
        error(
          ctx,
          "schema.type",
          "anchor blocks require a string `name`",
          `${path}.name`,
        );
      }
      break;
    case "list":
      if (!Array.isArray(b.items)) {
        error(ctx, "schema.type", "`items` must be an array", `${path}.items`);
        break;
      }
      b.items.forEach((item, index) => {
        ctx.blockCount += 1; // list items count as blocks toward the limit
        const itemPath = `${path}.items[${index}]`;
        if (!Array.isArray(item?.blocks)) {
          error(
            ctx,
            "schema.type",
            "list items require a `blocks` array",
            itemPath,
          );
          return;
        }
        item.blocks.forEach((child, childIndex) =>
          validateBlock(
            child,
            `${itemPath}.blocks[${childIndex}]`,
            depth + 1,
            ctx,
          )
        );
      });
      break;
    case "blockquote":
      if (!Array.isArray(b.blocks)) {
        error(
          ctx,
          "schema.type",
          "`blocks` must be an array",
          `${path}.blocks`,
        );
        break;
      }
      b.blocks.forEach((child, index) =>
        validateBlock(child, `${path}.blocks[${index}]`, depth + 1, ctx)
      );
      if (b.credit !== undefined) {
        validateRichText(b.credit, `${path}.credit`, ctx);
      }
      break;
    case "pullquote":
      validateRichText(b.text, `${path}.text`, ctx);
      if (b.credit !== undefined) {
        validateRichText(b.credit, `${path}.credit`, ctx);
      }
      break;
    case "collage":
    case "slideshow":
      if (!Array.isArray(b.items)) {
        error(ctx, "schema.type", "`items` must be an array", `${path}.items`);
        break;
      }
      if (b.items.length > ctx.limits.collageMaxItems) {
        error(
          ctx,
          "limit.collage-items",
          `${b.kind} has ${b.items.length} items; the limit is ${ctx.limits.collageMaxItems}`,
          `${path}.items`,
        );
      }
      b.items.forEach((item, index) =>
        validateMediaRef(item, `${path}.items[${index}]`, ctx)
      );
      validateCaption(b.caption, `${path}.caption`, ctx);
      break;
    case "table": {
      if (!Array.isArray(b.rows)) {
        error(ctx, "schema.type", "`rows` must be an array", `${path}.rows`);
        break;
      }
      b.rows.forEach((row, rowIndex) => {
        ctx.blockCount += 1; // table rows count as blocks toward the limit
        const rowPath = `${path}.rows[${rowIndex}]`;
        if (!Array.isArray(row?.cells)) {
          error(
            ctx,
            "schema.type",
            "table rows require a `cells` array",
            rowPath,
          );
          return;
        }
        const columns = row.cells.reduce(
          (total, cell) => total + Math.max(1, cell?.colspan ?? 1),
          0,
        );
        if (columns > ctx.limits.maxTableCols) {
          error(
            ctx,
            "limit.table-columns",
            `row spans ${columns} columns; the limit is ${ctx.limits.maxTableCols}`,
            rowPath,
          );
        }
        row.cells.forEach((cell, cellIndex) => {
          if (cell?.text !== undefined) {
            validateRichText(
              cell.text,
              `${rowPath}.cells[${cellIndex}].text`,
              ctx,
            );
          }
        });
      });
      if (b.caption !== undefined) {
        validateRichText(b.caption, `${path}.caption`, ctx);
      }
      break;
    }
    case "details":
      validateRichText(b.summary, `${path}.summary`, ctx);
      if (!Array.isArray(b.blocks)) {
        error(
          ctx,
          "schema.type",
          "`blocks` must be an array",
          `${path}.blocks`,
        );
        break;
      }
      b.blocks.forEach((child, index) =>
        validateBlock(child, `${path}.blocks[${index}]`, depth + 1, ctx)
      );
      break;
    case "map":
      if (
        typeof b.latitude !== "number" || typeof b.longitude !== "number" ||
        b.latitude < -90 || b.latitude > 90 ||
        b.longitude < -180 || b.longitude > 180
      ) {
        error(ctx, "schema.range", "invalid map coordinates", path);
      }
      if (typeof b.width !== "number" || typeof b.height !== "number") {
        error(ctx, "schema.type", "map requires numeric width/height", path);
      } else {
        if (b.width < 0 || b.height < 0 || b.width + b.height > 10000) {
          error(
            ctx,
            "limit.map-size",
            "map width and height must not exceed 10000 in total",
            path,
          );
        }
        const larger = Math.max(b.width, b.height);
        const smaller = Math.max(1, Math.min(b.width, b.height));
        if (larger / smaller > 20) {
          error(
            ctx,
            "limit.map-ratio",
            "map width/height ratio must be at most 20",
            path,
          );
        }
      }
      validateCaption(b.caption, `${path}.caption`, ctx);
      break;
    case "photo":
    case "video":
    case "animation":
    case "audio":
    case "voice_note":
      validateMediaRef(b.media, `${path}.media`, ctx);
      validateCaption(b.caption, `${path}.caption`, ctx);
      break;
    case "unsupported":
      break;
    default: {
      const exhaustive: never = b;
      error(
        ctx,
        "schema.unknown-kind",
        `unknown block kind ${
          JSON.stringify((exhaustive as { kind?: unknown }).kind)
        }`,
        path,
      );
    }
  }
}

export interface ValidationResult {
  valid: boolean;
  diagnostics: RenderDiagnostic[];
  /** Total blocks counted (including nested blocks, list items, table rows). */
  blockCount: number;
  mediaCount: number;
  textLength: number;
}

/**
 * Structural + limits validation of a canonical message. This is the
 * TypeScript twin of the C++ validation in `renderer/cpp/adapters`; the two
 * consume identical fixture files (plan Phase 4 verification).
 */
export function validateCanonicalMessage(
  message: unknown,
  limits: RichMessageLimits = LIMITS_V1,
): ValidationResult {
  const ctx: ValidationContext = {
    diagnostics: [],
    blockCount: 0,
    mediaCount: 0,
    textLength: 0,
    limits,
  };
  if (typeof message !== "object" || message === null) {
    error(ctx, "schema.type", "expected a canonical message object", "$");
  } else {
    const m = message as CanonicalRichMessage;
    if (m.schemaVersion !== SCHEMA_VERSION) {
      error(
        ctx,
        "schema.version",
        `unsupported schemaVersion ${
          JSON.stringify(m.schemaVersion)
        }; expected ${SCHEMA_VERSION}`,
        "$.schemaVersion",
      );
    }
    if (!Array.isArray(m.blocks)) {
      error(ctx, "schema.type", "`blocks` must be an array", "$.blocks");
    } else {
      m.blocks.forEach((block, index) =>
        validateBlock(block, `$.blocks[${index}]`, 1, ctx)
      );
    }
  }
  if (ctx.blockCount > ctx.limits.maxBlocks) {
    error(
      ctx,
      "limit.blocks",
      `message contains ${ctx.blockCount} blocks (including nested blocks, list items, and table rows); the limit is ${ctx.limits.maxBlocks}`,
      "$",
    );
  }
  if (ctx.mediaCount > ctx.limits.maxMedia) {
    error(
      ctx,
      "limit.media",
      `message contains ${ctx.mediaCount} media attachments; the limit is ${ctx.limits.maxMedia}`,
      "$",
    );
  }
  if (ctx.textLength > ctx.limits.lengthLimit) {
    error(
      ctx,
      "limit.length",
      `message text totals ${ctx.textLength} characters (including formula sources); the limit is ${ctx.limits.lengthLimit}`,
      "$",
    );
  }
  return {
    valid: !ctx.diagnostics.some((d) => d.severity === "error"),
    diagnostics: ctx.diagnostics,
    blockCount: ctx.blockCount,
    mediaCount: ctx.mediaCount,
    textLength: ctx.textLength,
  };
}
