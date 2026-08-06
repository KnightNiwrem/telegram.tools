/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).

The prepare/layout/paint sequence below mirrors RichDraftPreview
(history_view_rich_draft_preview.cpp) with the QWidget and Main::Session
integration replaced by the sessionless context (plan Phase 2).
*/
#include "core/render_service.h"

#include "core/harness_app.h"
#include "core/version.h"

#include "iv/iv_rich_page.h"
#include "iv/markdown/iv_markdown_article.h"
#include "iv/markdown/iv_markdown_media_block.h"
#include "iv/markdown/iv_markdown_prepare.h"
#include "ui/chat/chat_style.h"
#include "ui/chat/chat_theme.h"
#include "ui/painter.h"
#include "ui/style/style_core_palette.h"

#include "styles/style_chat.h"
#include "styles/style_iv.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

namespace Ttr {
namespace {

// Preview-only host: static assets are fully loaded before layout, so
// repaint/relayout requests cannot occur inside a single synchronous render.
class NullMediaBlockHost final : public Iv::Markdown::MediaBlockHost {
public:
	void requestRepaint(QRect articleRect) override {
	}
	void requestRelayout(QRect articleRect) override {
	}
};

[[nodiscard]] QJsonObject DiagnosticToJson(const Diagnostic &diagnostic) {
	auto result = QJsonObject();
	result.insert(u"severity"_q, DiagnosticSeverityName(diagnostic.severity));
	result.insert(u"code"_q, diagnostic.code);
	result.insert(u"message"_q, diagnostic.message);
	if (!diagnostic.path.isEmpty()) {
		result.insert(u"path"_q, diagnostic.path);
	}
	return result;
}

[[nodiscard]] Iv::RichMessageLimits SessionlessLimits() {
	// Defaults in iv_rich_page.h are pinned to the target Bot API revision;
	// ResolveRichMessageLimits(session) only applies server-side overrides.
	return Iv::RichMessageLimits();
}

} // namespace

class RenderService::Impl final {
public:
	Impl()
	: _mediaStore(std::make_shared<MediaStore>())
	, _chatStyle(std::make_unique<Ui::ChatStyle>(style::main_palette::get()))
	, _chatTheme(std::make_unique<Ui::ChatTheme>())
	, _article(st::messageMarkdown) {
		_article.setMediaBlockHost(&_mediaBlockHost);
		// Single-frame renders repaint nothing, but spoiler/text animations
		// assert on a missing repaint callback (mirrors RichDraftPreview).
		_article.setTextRepaintCallbacks([] {}, [](QRect) {});
	}

	~Impl() {
		_article.setMediaBlockHost(nullptr);
		_article.clearBeforeDestroy();
	}

	bool registerMedia(
			const QString &ref,
			const QByteArray &bytes,
			const QString &mimeType) {
		return _mediaStore->registerAsset(ref, bytes, mimeType);
	}

	void clearMedia() {
		_mediaStore->clear();
	}

	RenderOutput render(const QJsonObject &request) {
		auto output = RenderOutput();
		auto diagnostics = QJsonArray();

		const auto fail = [&](const QString &code, const QString &message) {
			diagnostics.append(DiagnosticToJson({
				Diagnostic::Severity::Error,
				code,
				message,
				QString(),
			}));
			output.metadata = failureMetadata(diagnostics);
			return output;
		};

		const auto content = request.value(u"content"_q).toObject();
		const auto mode = content.value(u"mode"_q).toString();
		if (mode != u"blocks"_q) {
			// HTML/Markdown import (plan Phase 7) enters below
			// BlocksFromHtmlSource once the sessionless seam inside
			// TextUtilities::BlocksFromHtml is established; until then the
			// renderer reports the mode as unsupported instead of guessing.
			return fail(
				u"renderer.mode-unsupported"_q,
				u"only canonical blocks input is implemented; HTML/Markdown "
				"import is tracked in renderer/BUILD-STATUS.md"_q);
		}
		const auto viewportWidth = request.value(u"viewportWidth"_q).toInt();
		if (viewportWidth <= 0 || viewportWidth > 4096) {
			return fail(
				u"renderer.viewport"_q,
				u"viewportWidth must be within 1..4096"_q);
		}
		const auto theme = request.value(u"theme"_q).toString();
		if (theme == u"dark"_q) {
			// Dark palette loading (embedded night theme) is a recorded
			// follow-up; rendering proceeds with the light palette so the
			// caller still gets deterministic output plus this warning.
			diagnostics.append(DiagnosticToJson({
				Diagnostic::Severity::Warning,
				u"renderer.theme-unavailable"_q,
				u"dark theme palette is not wired up yet; rendered light"_q,
				QString(),
			}));
		}

		auto parsed = ParseCanonicalRichMessage(
			content.value(u"richMessage"_q).toObject());
		for (const auto &diagnostic : parsed.diagnostics) {
			diagnostics.append(DiagnosticToJson(diagnostic));
		}
		if (!parsed.ok()) {
			output.metadata = failureMetadata(diagnostics);
			return output;
		}

		// Limits are enforced by the TypeScript validator before the request
		// crosses the boundary and re-checked structurally by the canonical
		// parser above; Iv::ValidateRichMessage lives in the app-bound
		// iv_rich_page.cpp translation unit and is deliberately not linked.
		const auto limits = SessionlessLimits();

		auto mediaRuntime = std::make_shared<SessionlessMediaRuntime>(
			_mediaStore,
			std::move(parsed.media));
		auto prepared = Iv::Markdown::TryPrepareNativeInstantView({
			.richPage = parsed.page,
			.mediaRuntime = mediaRuntime,
			.dimensionsOverride
				= Iv::Markdown::CaptureMarkdownPrepareDimensions(
					st::messageMarkdown),
			.tableRenderLimits
				= Iv::Markdown::PrepareTableRenderLimitsForRichMessage(
					limits),
		});
		if (!prepared.supported()) {
			return fail(
				u"renderer.unsupported-content"_q,
				u"native preparation rejected the page: "_q
					+ prepared.debugReason);
		}
		_article.setContent(std::move(prepared.content));

		// Interaction state: replay details toggles before layout.
		const auto interaction
			= request.value(u"interactionState"_q).toObject();
		for (const auto &anchor
				: interaction.value(u"toggledDetails"_q).toArray()) {
			(void)_article.toggleDetails(anchor.toString());
		}

		const auto height = _article.resizeGetHeight(viewportWidth);
		if (height <= 0 || height > 65536) {
			return fail(
				u"renderer.layout"_q,
				u"layout produced an invalid height"_q);
		}
		_article.setVisibleTopBottom(0, height);

		auto image = QImage(
			QSize(viewportWidth, height),
			QImage::Format_ARGB32_Premultiplied);
		image.fill(st::historyComposeAreaBg->c);
		{
			auto p = Painter(&image);
			const auto area = QRect(0, 0, viewportWidth, height);
			auto context = Iv::Markdown::MarkdownArticlePaintContext(
				_chatTheme->preparePaintContext(
					_chatStyle.get(),
					area,
					area,
					area,
					false));
			const auto messageStyle = context.messageStyle();
			context.caches = {
				.pre = messageStyle->preCache.get(),
				.blockquote = context.quoteCache({}, 0),
				.colors = _chatStyle->highlightColors(),
				.st = &messageStyle->richPageStyle,
				.repaint = [] {},
				.repaintRect = [](QRect) {},
			};
			_article.paint(p, context);
		}

		output.image = image.convertToFormat(QImage::Format_RGBA8888);
		output.ok = true;

		auto geometry = QJsonArray();
		for (const auto &media : _article.mediaBlockGeometries()) {
			auto entry = QJsonObject();
			entry.insert(u"kind"_q, u"media"_q);
			entry.insert(u"x"_q, media.mediaRect.x());
			entry.insert(u"y"_q, media.mediaRect.y());
			entry.insert(u"width"_q, media.mediaRect.width());
			entry.insert(u"height"_q, media.mediaRect.height());
			geometry.append(entry);
		}

		auto metadata = QJsonObject();
		metadata.insert(u"status"_q, u"ok"_q);
		metadata.insert(u"width"_q, output.image.width());
		metadata.insert(u"height"_q, output.image.height());
		metadata.insert(u"diagnostics"_q, diagnostics);
		metadata.insert(u"geometry"_q, geometry);
		metadata.insert(u"hitTargets"_q, QJsonArray());
		metadata.insert(u"rendererVersion"_q, QString::fromLatin1(
			kRendererVersion));
		metadata.insert(u"tdesktopRevision"_q, QString::fromLatin1(
			kTdesktopRevision));
		output.metadata = metadata;
		return output;
	}

private:
	[[nodiscard]] static QJsonObject failureMetadata(
			const QJsonArray &diagnostics) {
		auto metadata = QJsonObject();
		metadata.insert(u"status"_q, u"error"_q);
		metadata.insert(u"diagnostics"_q, diagnostics);
		metadata.insert(u"rendererVersion"_q, QString::fromLatin1(
			kRendererVersion));
		metadata.insert(u"tdesktopRevision"_q, QString::fromLatin1(
			kTdesktopRevision));
		return metadata;
	}

	std::shared_ptr<MediaStore> _mediaStore;
	NullMediaBlockHost _mediaBlockHost;
	std::unique_ptr<Ui::ChatStyle> _chatStyle;
	std::unique_ptr<Ui::ChatTheme> _chatTheme;
	Iv::Markdown::MarkdownArticle _article;

};

RenderService::RenderService() : _impl(std::make_unique<Impl>()) {
}

RenderService::~RenderService() = default;

bool RenderService::registerMedia(
		const QString &ref,
		const QByteArray &bytes,
		const QString &mimeType) {
	return _impl->registerMedia(ref, bytes, mimeType);
}

void RenderService::clearMedia() {
	_impl->clearMedia();
}

RenderOutput RenderService::render(const QJsonObject &request) {
	return _impl->render(request);
}

QJsonObject RenderService::VersionInfo() {
	auto result = QJsonObject();
	result.insert(u"rendererVersion"_q, QString::fromLatin1(
		kRendererVersion));
	result.insert(u"tdesktopRevision"_q, QString::fromLatin1(
		kTdesktopRevision));
	return result;
}

} // namespace Ttr
