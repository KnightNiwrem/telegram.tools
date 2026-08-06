/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE). Derived-work boundary: this file consumes
Telegram Desktop headers from the pinned submodule (renderer/UPSTREAM.md).

Canonical JSON (schema v1) -> Iv::RichPage conversion (plan Phase 4).
The conversion mirrors the field conventions of Iv::ParseRichPage at
TDesktop v7.0.9 (Telegram/SourceFiles/iv/iv_rich_page.cpp):
- table caption/title and details summary land in Block.text;
- blockquote/pullquote credit lands in Block.caption;
- block anchors are lifted from their text via the AdoptAnchor pattern;
- inline entity encodings follow AppendRichText (CustomUrl data for links,
  "mailto:"/"tel:" prefixes, FormattedDate serialized data, CustomEmoji
  entity data carrying the emoji document id, inline formulas as
  ObjectReplacementCharacter + serialized inline text object entities).
*/
#pragma once

#include "iv/iv_rich_page.h"

#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include <memory>
#include <vector>

namespace Ttr {

struct Diagnostic {
	enum class Severity : unsigned char {
		Error,
		Warning,
		Info,
	};

	Severity severity = Severity::Error;
	QString code;
	QString message;
	QString path;
};

struct MediaIdBinding {
	// Sequentially assigned ids stand in for Telegram photo/document ids;
	// the sessionless media runtime resolves them back to canonical refs.
	uint64 id = 0;
	QString ref;
	bool isPhoto = false;
};

struct CanonicalPageResult {
	std::shared_ptr<Iv::RichPage> page;
	std::vector<MediaIdBinding> media;
	std::vector<Diagnostic> diagnostics;

	[[nodiscard]] bool ok() const {
		return page != nullptr;
	}
};

[[nodiscard]] CanonicalPageResult ParseCanonicalRichMessage(
	const QJsonObject &root);

[[nodiscard]] QString DiagnosticSeverityName(Diagnostic::Severity severity);

} // namespace Ttr
