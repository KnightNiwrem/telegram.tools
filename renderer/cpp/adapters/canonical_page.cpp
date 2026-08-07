/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).
*/
#include "adapters/canonical_page.h"

#include "iv/markdown/iv_markdown_prepare.h"
#include "iv/markdown/iv_markdown_prepare_links.h"
#include "ui/text/text_entity.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>

namespace Ttr {
namespace {

using Iv::RichPage;
using BlockKind = RichPage::BlockKind;

struct Context {
	std::vector<MediaIdBinding> *media = nullptr;
	std::vector<Diagnostic> *diagnostics = nullptr;
	uint64 nextMediaId = 1;
};

void Report(
		const Context &context,
		Diagnostic::Severity severity,
		const QString &code,
		const QString &message,
		const QString &path) {
	context.diagnostics->push_back({ severity, code, message, path });
}

void Error(
		const Context &context,
		const QString &code,
		const QString &message,
		const QString &path) {
	Report(context, Diagnostic::Severity::Error, code, message, path);
}

// Mirrors the AdoptAnchor pattern of iv_rich_page.cpp: the first anchor
// found in a block's text becomes the block anchor.
void AdoptAnchor(QString *anchorId, RichPage::RichText *text) {
	if (anchorId->isEmpty() && !text->anchorId.isEmpty()) {
		*anchorId = text->anchorId;
	}
}

void AddRichAnchor(
		QString *primary,
		std::vector<QString> *extra,
		QString anchorId) {
	if (anchorId.isEmpty()) {
		return;
	}
	if (primary && primary->isEmpty()) {
		*primary = std::move(anchorId);
		return;
	}
	if (primary && *primary == anchorId) {
		return;
	}
	if (extra
		&& (std::find(extra->begin(), extra->end(), anchorId)
			== extra->end())) {
		extra->push_back(std::move(anchorId));
	}
}

[[nodiscard]] FormattedDateFlags DateFlagsFromFormat(const QString &format) {
	auto flags = FormattedDateFlags();
	if (format == u"r"_q) {
		return flags | FormattedDateFlag::Relative;
	}
	for (const auto &ch : format) {
		if (ch == u'w') {
			flags |= FormattedDateFlag::DayOfWeek;
		} else if (ch == u'd') {
			flags |= FormattedDateFlag::ShortDate;
		} else if (ch == u'D') {
			flags |= FormattedDateFlag::LongDate;
		} else if (ch == u't') {
			flags |= FormattedDateFlag::ShortTime;
		} else if (ch == u'T') {
			flags |= FormattedDateFlag::LongTime;
		}
	}
	return flags;
}

[[nodiscard]] RichPage::RichText ParseRichText(
		const QJsonValue &value,
		const Context &context,
		const QString &path) {
	auto result = RichPage::RichText();
	if (!value.isObject()) {
		Error(context, u"schema.type"_q, u"expected a rich text object"_q, path);
		return result;
	}
	const auto object = value.toObject();
	result.text.text = object.value(u"text"_q).toString();
	const auto entities = object.value(u"entities"_q).toArray();
	auto index = 0;
	for (const auto &entityValue : entities) {
		const auto entityPath = path + u".entities["_q
			+ QString::number(index++) + u"]"_q;
		const auto entity = entityValue.toObject();
		const auto type = entity.value(u"type"_q).toString();
		const auto offset = entity.value(u"offset"_q).toInt();
		const auto length = entity.value(u"length"_q).toInt();
		if (offset < 0
			|| length < 0
			|| offset + length > int(result.text.text.size())) {
			Error(
				context,
				u"schema.entity-range"_q,
				u"entity range exceeds text length"_q,
				entityPath);
			continue;
		}
		const auto add = [&](EntityType entityType, QString data = QString()) {
			result.text.entities.push_back(
				EntityInText(entityType, offset, length, std::move(data)));
		};
		if (type == u"bold"_q) {
			add(EntityType::Bold);
		} else if (type == u"italic"_q) {
			add(EntityType::Italic);
		} else if (type == u"underline"_q) {
			add(EntityType::Underline);
		} else if (type == u"strikethrough"_q) {
			add(EntityType::StrikeOut);
		} else if (type == u"spoiler"_q) {
			add(EntityType::Spoiler);
		} else if (type == u"code"_q) {
			add(EntityType::Code);
		} else if (type == u"subscript"_q) {
			add(EntityType::Subscript);
		} else if (type == u"superscript"_q) {
			add(EntityType::Superscript);
		} else if (type == u"marked"_q) {
			add(EntityType::Marked);
		} else if (type == u"date_time"_q) {
			add(EntityType::FormattedDate, SerializeFormattedDateData(
				int32(entity.value(u"unixTime"_q).toDouble()),
				DateFlagsFromFormat(entity.value(u"format"_q).toString())));
		} else if (type == u"text_mention"_q) {
			// Sessionless: no access hash is available; the entity keeps the
			// user id so the mention renders as a name link without a
			// resolvable peer (matches MentionNameEntityData with hash 0).
			add(EntityType::MentionName, QString::number(
				qint64(entity.value(u"userId"_q).toDouble())) + u".0"_q);
		} else if (type == u"mention"_q) {
			add(EntityType::Mention);
		} else if (type == u"hashtag"_q) {
			add(EntityType::Hashtag);
		} else if (type == u"cashtag"_q) {
			add(EntityType::Cashtag);
		} else if (type == u"bot_command"_q) {
			add(EntityType::BotCommand);
		} else if (type == u"email_address"_q) {
			add(
				EntityType::CustomUrl,
				u"mailto:"_q + entity.value(u"emailAddress"_q).toString());
		} else if (type == u"phone_number"_q) {
			add(
				EntityType::CustomUrl,
				u"tel:"_q + entity.value(u"phoneNumber"_q).toString());
		} else if (type == u"bank_card_number"_q) {
			add(EntityType::BankCard);
		} else if (type == u"url"_q) {
			add(EntityType::CustomUrl, entity.value(u"url"_q).toString());
		} else if (type == u"custom_emoji"_q) {
			add(
				EntityType::CustomEmoji,
				entity.value(u"customEmojiId"_q).toString());
		} else if (type == u"math"_q) {
			// Inline formulas follow AppendRichText's MTPDtextMath branch:
			// the covered source range is replaced by a single object
			// replacement character carrying the serialized formula entity.
			const auto source = entity.value(u"expression"_q).toString();
			const auto entityData
				= Iv::Markdown::SerializeInlineTextObjectEntity({
					.kind = Iv::Markdown::InlineTextObjectKind::Formula,
					.data = Iv::Markdown::InlineTextObjectFormulaData{
						.copySource
							= Iv::Markdown::InlineFormulaCopySource(source),
						.trimmedTex = source,
					},
				});
			if (entityData.isEmpty()) {
				continue;
			}
			result.text.text.replace(
				offset,
				length,
				QChar(QChar::ObjectReplacementCharacter));
			// Later entity offsets refer to the original text; the
			// TypeScript adapter emits inline math as the trailing entity of
			// its covered range, so a single in-place replacement keeps
			// earlier offsets valid. Overlapping entities after an inline
			// formula are rejected instead of silently corrupted.
			const auto delta = length - 1;
			if (delta != 0) {
				for (auto &existing : result.text.entities) {
					if (existing.offset() > offset) {
						Error(
							context,
							u"adapter.math-overlap"_q,
							u"inline formula overlaps a later entity"_q,
							entityPath);
					}
				}
			}
			result.text.entities.push_back(EntityInText(
				EntityType::CustomEmoji,
				offset,
				1,
				entityData));
		} else if (type == u"anchor"_q) {
			AddRichAnchor(
				&result.anchorId,
				&result.anchorIds,
				Iv::Markdown::NormalizeFragmentId(
					entity.value(u"name"_q).toString()));
		} else if (type == u"anchor_link"_q) {
			add(
				EntityType::CustomUrl,
				u"#"_q + Iv::Markdown::NormalizeFragmentId(
					entity.value(u"anchorName"_q).toString()));
		} else if (type == u"reference"_q) {
			// Best-effort mapping pending Phase 5 verification against the
			// in-TDesktop reference: a reference defines a linkable target.
			AddRichAnchor(
				&result.anchorId,
				&result.anchorIds,
				Iv::Markdown::NormalizeFragmentId(
					entity.value(u"name"_q).toString()));
		} else if (type == u"reference_link"_q) {
			add(
				EntityType::CustomUrl,
				u"#"_q + Iv::Markdown::NormalizeFragmentId(
					entity.value(u"referenceName"_q).toString()));
		} else {
			Error(
				context,
				u"schema.unknown-entity"_q,
				u"unknown entity type "_q + type,
				entityPath);
		}
	}
	return result;
}

[[nodiscard]] uint64 RegisterMedia(
		Context &context,
		const QJsonObject &media,
		bool isPhoto) {
	const auto id = context.nextMediaId++;
	context.media->push_back({
		.id = id,
		.ref = media.value(u"ref"_q).toString(),
		.isPhoto = isPhoto,
	});
	return id;
}

void FillMediaBlock(
		Context &context,
		RichPage::Block *block,
		const QJsonObject &media,
		const QString &kind) {
	block->width = media.value(u"width"_q).toInt();
	block->height = media.value(u"height"_q).toInt();
	block->spoiler = media.value(u"spoiler"_q).toBool();
	if (kind == u"photo"_q) {
		block->kind = BlockKind::Photo;
		block->photoId = RegisterMedia(context, media, true);
	} else if (kind == u"video"_q || kind == u"animation"_q) {
		block->kind = BlockKind::Video;
		block->documentId = RegisterMedia(context, media, false);
		const auto animated = (kind == u"animation"_q);
		block->autoplay = media.value(u"autoplay"_q).toBool(animated);
		block->loop = media.value(u"loop"_q).toBool(animated);
	} else if (kind == u"audio"_q || kind == u"voice_note"_q) {
		block->kind = BlockKind::Audio;
		block->documentId = RegisterMedia(context, media, false);
		block->audioTitle = media.value(u"title"_q).toString();
		block->audioPerformer = media.value(u"performer"_q).toString();
		block->audioFileName = media.value(u"fileName"_q).toString();
		block->audioDuration = int(media.value(u"duration"_q).toDouble());
	} else {
		block->kind = BlockKind::Unsupported;
	}
}

[[nodiscard]] RichPage::GroupedMediaItem ParseGroupedItem(
		Context &context,
		const QJsonObject &media) {
	auto item = RichPage::GroupedMediaItem();
	const auto kind = media.value(u"kind"_q).toString();
	item.width = media.value(u"width"_q).toInt();
	item.height = media.value(u"height"_q).toInt();
	item.spoiler = media.value(u"spoiler"_q).toBool();
	if (kind == u"photo"_q) {
		item.kind = BlockKind::Photo;
		item.photoId = RegisterMedia(context, media, true);
	} else {
		item.kind = BlockKind::Video;
		item.documentId = RegisterMedia(context, media, false);
		const auto animated = (kind == u"animation"_q);
		item.autoplay = media.value(u"autoplay"_q).toBool(animated);
		item.loop = media.value(u"loop"_q).toBool(animated);
	}
	return item;
}

void ParseCaptionInto(
		Context &context,
		RichPage::Block *block,
		const QJsonObject &object,
		const QString &path) {
	// RichPage keeps the caption text and its credit merged in
	// Block.caption; ParseCaption in iv_rich_page.cpp appends the credit
	// after the caption text separated by a newline.
	const auto caption = object.value(u"caption"_q);
	if (!caption.isObject()) {
		return;
	}
	const auto captionObject = caption.toObject();
	auto text = captionObject.contains(u"text"_q)
		? ParseRichText(
			captionObject.value(u"text"_q),
			context,
			path + u".caption.text"_q)
		: RichPage::RichText();
	if (captionObject.contains(u"credit"_q)) {
		auto credit = ParseRichText(
			captionObject.value(u"credit"_q),
			context,
			path + u".caption.credit"_q);
		if (!credit.text.text.isEmpty()) {
			if (!text.text.text.isEmpty()) {
				text.text.append(u"\n"_q);
			}
			const auto shift = int(text.text.text.size());
			for (auto &entity : credit.text.entities) {
				text.text.entities.push_back(EntityInText(
					entity.type(),
					entity.offset() + shift,
					entity.length(),
					entity.data()));
			}
			text.text.text.append(credit.text.text);
			AddRichAnchor(
				&text.anchorId,
				&text.anchorIds,
				std::move(credit.anchorId));
		}
	}
	block->caption = std::move(text);
	AdoptAnchor(&block->anchorId, &block->caption);
}

void ParseBlocks(
	Context &context,
	std::vector<RichPage::Block> *result,
	const QJsonArray &blocks,
	const QString &path);

[[nodiscard]] RichPage::Block ParseBlock(
		Context &context,
		const QJsonObject &object,
		const QString &path) {
	auto block = RichPage::Block();
	const auto kind = object.value(u"kind"_q).toString();
	const auto text = [&](const char *field) {
		return ParseRichText(
			object.value(QString::fromLatin1(field)),
			context,
			path + u"."_q + QString::fromLatin1(field));
	};
	if (kind == u"paragraph"_q) {
		block.kind = BlockKind::Paragraph;
		block.text = text("text");
		AdoptAnchor(&block.anchorId, &block.text);
	} else if (kind == u"heading"_q) {
		block.kind = BlockKind::Heading;
		block.headingLevel = object.value(u"level"_q).toInt(1);
		block.text = text("text");
		AdoptAnchor(&block.anchorId, &block.text);
	} else if (kind == u"pre"_q) {
		block.kind = BlockKind::Code;
		block.language = object.value(u"language"_q).toString();
		block.text = text("text");
		AdoptAnchor(&block.anchorId, &block.text);
	} else if (kind == u"footer"_q) {
		block.kind = BlockKind::Footer;
		block.text = text("text");
		AdoptAnchor(&block.anchorId, &block.text);
	} else if (kind == u"divider"_q) {
		block.kind = BlockKind::Divider;
	} else if (kind == u"math"_q) {
		block.kind = BlockKind::Math;
		block.formula = object.value(u"expression"_q).toString();
	} else if (kind == u"anchor"_q) {
		block.kind = BlockKind::Anchor;
		block.anchorId = Iv::Markdown::NormalizeFragmentId(
			object.value(u"name"_q).toString());
	} else if (kind == u"list"_q) {
		block.kind = BlockKind::List;
		block.listKind = object.value(u"ordered"_q).toBool()
			? RichPage::ListKind::Ordered
			: RichPage::ListKind::Bullet;
		const auto items = object.value(u"items"_q).toArray();
		auto index = 0;
		for (const auto &itemValue : items) {
			const auto itemPath = path + u".items["_q
				+ QString::number(index++) + u"]"_q;
			const auto itemObject = itemValue.toObject();
			auto item = RichPage::ListItem();
			const auto taskState = itemObject.value(u"taskState"_q).toString();
			item.taskState = (taskState == u"checked"_q)
				? RichPage::TaskState::Checked
				: (taskState == u"unchecked"_q)
				? RichPage::TaskState::Unchecked
				: RichPage::TaskState::None;
			if (itemObject.contains(u"value"_q)) {
				item.number.value = itemObject.value(u"value"_q).toInt();
			}
			if (itemObject.contains(u"numberingType"_q)) {
				item.number.type
					= itemObject.value(u"numberingType"_q).toString();
			}
			ParseBlocks(
				context,
				&item.blocks,
				itemObject.value(u"blocks"_q).toArray(),
				itemPath + u".blocks"_q);
			// Mirrors AdoptLeadingParagraphListItemText: a single leading
			// paragraph becomes the item text so simple items lay out on
			// the marker line.
			if (!item.blocks.empty()
				&& item.blocks.front().kind == BlockKind::Paragraph) {
				item.text = std::move(item.blocks.front().text);
				item.blocks.erase(item.blocks.begin());
				AdoptAnchor(&item.anchorId, &item.text);
			}
			block.listItems.push_back(std::move(item));
		}
	} else if (kind == u"blockquote"_q) {
		block.kind = BlockKind::Quote;
		ParseBlocks(
			context,
			&block.blocks,
			object.value(u"blocks"_q).toArray(),
			path + u".blocks"_q);
		if (object.contains(u"credit"_q)) {
			block.caption = text("credit");
			AdoptAnchor(&block.anchorId, &block.caption);
		}
	} else if (kind == u"pullquote"_q) {
		block.kind = BlockKind::Quote;
		block.pullquote = true;
		block.text = text("text");
		AdoptAnchor(&block.anchorId, &block.text);
		if (object.contains(u"credit"_q)) {
			block.caption = text("credit");
			AdoptAnchor(&block.anchorId, &block.caption);
		}
	} else if (kind == u"collage"_q || kind == u"slideshow"_q) {
		block.kind = BlockKind::GroupedMedia;
		block.mediaIntent = (kind == u"collage"_q)
			? RichPage::GroupedMediaIntent::Collage
			: RichPage::GroupedMediaIntent::Slideshow;
		const auto items = object.value(u"items"_q).toArray();
		for (const auto &itemValue : items) {
			block.mediaItems.push_back(
				ParseGroupedItem(context, itemValue.toObject()));
		}
		ParseCaptionInto(context, &block, object, path);
	} else if (kind == u"table"_q) {
		block.kind = BlockKind::Table;
		block.bordered = object.value(u"bordered"_q).toBool();
		block.striped = object.value(u"striped"_q).toBool();
		if (object.contains(u"caption"_q)) {
			// The table title/caption lands in Block.text, matching
			// MTPDpageBlockTable parsing.
			block.text = text("caption");
			AdoptAnchor(&block.anchorId, &block.text);
		}
		const auto rows = object.value(u"rows"_q).toArray();
		auto rowIndex = 0;
		for (const auto &rowValue : rows) {
			const auto rowPath = path + u".rows["_q
				+ QString::number(rowIndex++) + u"]"_q;
			auto row = RichPage::TableRow();
			const auto cells = rowValue.toObject().value(u"cells"_q).toArray();
			auto cellIndex = 0;
			for (const auto &cellValue : cells) {
				const auto cellPath = rowPath + u".cells["_q
					+ QString::number(cellIndex++) + u"]"_q;
				const auto cellObject = cellValue.toObject();
				auto cell = RichPage::TableCell();
				cell.colspan = std::max(
					1,
					cellObject.value(u"colspan"_q).toInt(1));
				cell.rowspan = std::max(
					1,
					cellObject.value(u"rowspan"_q).toInt(1));
				cell.header = cellObject.value(u"header"_q).toBool();
				const auto align = cellObject.value(u"align"_q).toString();
				cell.alignment = (align == u"center"_q)
					? RichPage::TableAlignment::Center
					: (align == u"right"_q)
					? RichPage::TableAlignment::Right
					: RichPage::TableAlignment::Left;
				const auto valign = cellObject.value(u"valign"_q).toString();
				cell.verticalAlignment = (valign == u"middle"_q)
					? RichPage::TableVerticalAlignment::Middle
					: (valign == u"bottom"_q)
					? RichPage::TableVerticalAlignment::Bottom
					: RichPage::TableVerticalAlignment::Top;
				if (cellObject.contains(u"text"_q)) {
					cell.text = ParseRichText(
						cellObject.value(u"text"_q),
						context,
						cellPath + u".text"_q);
					// Table cells cannot carry anchors (matches upstream).
					cell.text.anchorId.clear();
					cell.text.anchorIds.clear();
				}
				row.cells.push_back(std::move(cell));
			}
			block.tableRows.push_back(std::move(row));
		}
	} else if (kind == u"details"_q) {
		block.kind = BlockKind::Details;
		block.open = object.value(u"open"_q).toBool();
		block.text = text("summary");
		AdoptAnchor(&block.anchorId, &block.text);
		ParseBlocks(
			context,
			&block.blocks,
			object.value(u"blocks"_q).toArray(),
			path + u".blocks"_q);
	} else if (kind == u"map"_q) {
		block.kind = BlockKind::Map;
		block.latitude = object.value(u"latitude"_q).toDouble();
		block.longitude = object.value(u"longitude"_q).toDouble();
		block.zoom = object.value(u"zoom"_q).toInt();
		block.width = object.value(u"width"_q).toInt();
		block.height = object.value(u"height"_q).toInt();
		ParseCaptionInto(context, &block, object, path);
	} else if (kind == u"photo"_q
		|| kind == u"video"_q
		|| kind == u"animation"_q
		|| kind == u"audio"_q
		|| kind == u"voice_note"_q) {
		FillMediaBlock(
			context,
			&block,
			object.value(u"media"_q).toObject(),
			kind);
		ParseCaptionInto(context, &block, object, path);
	} else if (kind == u"thinking"_q) {
		block.kind = BlockKind::Thinking;
		block.text = text("text");
	} else if (kind == u"unsupported"_q) {
		block.kind = BlockKind::Unsupported;
	} else {
		block.kind = BlockKind::Unsupported;
		Error(
			context,
			u"schema.unknown-kind"_q,
			u"unknown block kind "_q + kind,
			path);
	}
	return block;
}

void ParseBlocks(
		Context &context,
		std::vector<RichPage::Block> *result,
		const QJsonArray &blocks,
		const QString &path) {
	result->reserve(result->size() + blocks.size());
	auto index = 0;
	for (const auto &value : blocks) {
		const auto blockPath = path + u"["_q
			+ QString::number(index++) + u"]"_q;
		if (!value.isObject()) {
			Error(
				context,
				u"schema.type"_q,
				u"expected a block object"_q,
				blockPath);
			continue;
		}
		result->push_back(ParseBlock(context, value.toObject(), blockPath));
	}
}

} // namespace

CanonicalPageResult ParseCanonicalRichMessage(const QJsonObject &root) {
	auto result = CanonicalPageResult();
	auto context = Context{
		.media = &result.media,
		.diagnostics = &result.diagnostics,
	};
	const auto version = root.value(u"schemaVersion"_q).toInt();
	if (version != 1) {
		Error(
			context,
			u"schema.version"_q,
			u"unsupported schemaVersion "_q + QString::number(version),
			u"$.schemaVersion"_q);
		return result;
	}
	auto page = std::make_shared<Iv::RichPage>();
	page->rtl = root.value(u"rtl"_q).toBool();
	ParseBlocks(
		context,
		&page->blocks,
		root.value(u"blocks"_q).toArray(),
		u"blocks"_q);
	for (const auto &diagnostic : result.diagnostics) {
		if (diagnostic.severity == Diagnostic::Severity::Error) {
			return result;
		}
	}
	result.page = std::move(page);
	return result;
}

QString DiagnosticSeverityName(Diagnostic::Severity severity) {
	switch (severity) {
	case Diagnostic::Severity::Error: return u"error"_q;
	case Diagnostic::Severity::Warning: return u"warning"_q;
	case Diagnostic::Severity::Info: return u"info"_q;
	}
	return u"error"_q;
}

} // namespace Ttr
