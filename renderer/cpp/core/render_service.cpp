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

#include "iv/editor/iv_editor_clipboard_import.h"
#include "iv/editor/iv_editor_text_entities.h"
#include "iv/iv_rich_page.h"
#include "iv/markdown/iv_markdown_article.h"
#include "iv/markdown/iv_markdown_media_block.h"
#include "iv/markdown/iv_markdown_prepare.h"
#include "ui/chat/chat_style.h"
#include "ui/chat/chat_theme.h"
#include "ui/painter.h"
#include "ui/style/style_core_palette.h"
#include "ui/text/text_html_tags.h"

#include "styles/style_chat.h"
#include "styles/style_iv.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSet>

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

// The prepare stage drops photo/video blocks without positive dimensions;
// a registered asset can supply them when the source declared none.
void BackfillMediaDimensions(
		std::vector<Iv::RichPage::Block> &blocks,
		const SessionlessMediaRuntime &runtime) {
	using Kind = Iv::RichPage::BlockKind;
	for (auto &block : blocks) {
		if ((block.kind == Kind::Photo || block.kind == Kind::Video)
			&& (block.width <= 0 || block.height <= 0)) {
			const auto size = runtime.assetDimensions(
				(block.kind == Kind::Photo)
					? block.photoId
					: block.documentId);
			if (!size.isEmpty()) {
				block.width = size.width();
				block.height = size.height();
			}
		}
		BackfillMediaDimensions(block.blocks, runtime);
		for (auto &item : block.listItems) {
			BackfillMediaDimensions(item.blocks, runtime);
		}
	}
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
		if (mode != u"blocks"_q && mode != u"html"_q) {
			// TDesktop v7.0.9 has no markdown parsing path (Iv::Markdown
			// is the renderer namespace, not a parser); a documented
			// parser choice is tracked in renderer/BUILD-STATUS.md.
			return fail(
				u"renderer.mode-unsupported"_q,
				u"blocks and html input are implemented; markdown import "
				"is tracked in renderer/BUILD-STATUS.md"_q);
		}
		const auto viewportWidth = request.value(u"viewportWidth"_q).toInt();
		if (viewportWidth <= 0 || viewportWidth > 4096) {
			return fail(
				u"renderer.viewport"_q,
				u"viewportWidth must be within 1..4096"_q);
		}
		// Device pixel ratio: layout stays in logical pixels (identical
		// line wrapping at any scale); only the output bitmap is rendered
		// at viewportWidth*scale, via QImage::setDevicePixelRatio.
		const auto rawScale = request.value(u"scale"_q);
		const auto scale = rawScale.isUndefined() ? 1. : rawScale.toDouble();
		if ((!rawScale.isUndefined() && !rawScale.isDouble())
			|| scale < 1.
			|| scale > 4.) {
			return fail(
				u"renderer.scale"_q,
				u"scale must be a number within 1..4"_q);
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

		// For blocks mode, limits are enforced by the TypeScript validator
		// before the request crosses the boundary and re-checked
		// structurally by the canonical parser below; for html mode they
		// bound the import itself. Iv::ValidateRichMessage lives in the
		// app-bound iv_rich_page.cpp translation unit and is deliberately
		// not linked.
		const auto limits = SessionlessLimits();

		auto page = std::shared_ptr<Iv::RichPage>();
		auto media = std::vector<MediaIdBinding>();
		if (mode == u"blocks"_q) {
			auto parsed = ParseCanonicalRichMessage(
				content.value(u"richMessage"_q).toObject());
			for (const auto &diagnostic : parsed.diagnostics) {
				diagnostics.append(DiagnosticToJson(diagnostic));
			}
			if (!parsed.ok()) {
				output.metadata = failureMetadata(diagnostics);
				return output;
			}
			page = std::move(parsed.page);
			media = std::move(parsed.media);
		} else {
			// TDesktop's own import path (patch 0003 exposes it with a
			// null session): remote media identities resolve to nothing
			// sessionless, so each recognized media block keeps a
			// placeholder id bound to its src string. Bytes registered
			// under that ref (registerMedia) render for real; anything
			// else paints the authentic unloaded-media placeholder. The
			// renderer never fetches a source itself.
			const auto source = content.value(u"source"_q).toString();
			auto imported = Iv::Editor::BlocksFromHtmlSource(
				nullptr,
				source,
				QString(),
				limits,
				0);
			if (!imported) {
				// BlocksFromHtml deliberately rejects a single plain
				// paragraph so TDesktop's clipboard callers insert it as
				// inline rich text instead of a block import; mirror that
				// fallback so one-paragraph sources still render.
				// TextWithTagsFromHtml returns nullopt when no formatting
				// survives — the paste path then uses the plain text,
				// which for an HTML source is the tag-stripped fragment.
				auto inlineText = TextUtilities::TextWithTagsFromHtml(
					source,
					true);
				if (!inlineText) {
					inlineText = TextUtilities::TextWithTagsFromHtmlFragment(
						source);
				}
				if (inlineText->text.trimmed().isEmpty()) {
					return fail(
						u"renderer.html-import"_q,
						u"no supported rich content found in the HTML "
						"source"_q);
				}
				auto paragraph = Iv::RichPage::Block();
				paragraph.kind = Iv::RichPage::BlockKind::Paragraph;
				paragraph.text.text = Iv::Editor::ConvertEditorTagsToRichText(
					std::move(*inlineText));
				imported = Iv::Editor::BlocksImportResult();
				imported->blocks.push_back(std::move(paragraph));
			}
			if (imported->truncated) {
				diagnostics.append(DiagnosticToJson({
					Diagnostic::Severity::Warning,
					u"renderer.html-truncated"_q,
					u"HTML import hit the rich-message limits; "
					"content was truncated"_q,
					QString(),
				}));
			}
			auto unresolved = 0;
			for (auto &binding : imported->mediaSources) {
				if (!_mediaStore->lookup(binding.source)) {
					++unresolved;
				}
				media.push_back({
					.id = binding.id,
					.ref = std::move(binding.source),
				});
			}
			if (unresolved > 0) {
				diagnostics.append(DiagnosticToJson({
					Diagnostic::Severity::Info,
					u"renderer.html-media-placeholder"_q,
					QString::number(unresolved)
						+ u" media item(s) rendered as placeholders; "
						"register bytes for their src to show content"_q,
					QString(),
				}));
			}
			const auto dropped = imported->droppedMedia
				+ int(imported->localMediaPaths.size());
			if (dropped > 0) {
				diagnostics.append(DiagnosticToJson({
					Diagnostic::Severity::Warning,
					u"renderer.html-media-dropped"_q,
					QString::number(dropped)
						+ u" media item(s) dropped; their sources could "
						"not be bound sessionless"_q,
					QString(),
				}));
			}
			page = std::make_shared<Iv::RichPage>();
			page->blocks = std::move(imported->blocks);
		}

		// Every media ref this render depends on, with its resolution
		// state, so the host can offer explicit per-source asset loading
		// (deduplicated: one entry per distinct ref).
		auto mediaRefs = QJsonArray();
		{
			auto seen = QSet<QString>();
			for (const auto &binding : media) {
				if (seen.contains(binding.ref)) {
					continue;
				}
				seen.insert(binding.ref);
				auto entry = QJsonObject();
				entry.insert(u"ref"_q, binding.ref);
				entry.insert(
					u"resolved"_q,
					_mediaStore->lookup(binding.ref) != nullptr);
				mediaRefs.append(entry);
			}
		}

		auto mediaRuntime = std::make_shared<SessionlessMediaRuntime>(
			_mediaStore,
			std::move(media));
		BackfillMediaDimensions(page->blocks, *mediaRuntime);
		auto prepared = Iv::Markdown::TryPrepareNativeInstantView({
			.richPage = page,
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
		if (height <= 0 || height * scale > 65536.) {
			return fail(
				u"renderer.layout"_q,
				u"layout produced an invalid height"_q);
		}
		_article.setVisibleTopBottom(0, height);

		// Bound the physical allocation to the worst case the viewport
		// and height limits above already allow at scale 1 (4096 x
		// 65536); high scales must not multiply it. A null image from a
		// failed allocation must fail the render rather than report ok
		// with an empty bitmap.
		const auto physical = QSize(viewportWidth, height) * scale;
		if (double(physical.width()) * physical.height() > 4096. * 65536.) {
			return fail(
				u"renderer.output-size"_q,
				u"scaled output exceeds the maximum image area"_q);
		}
		auto image = QImage(physical, QImage::Format_ARGB32_Premultiplied);
		if (image.isNull()) {
			return fail(
				u"renderer.allocation"_q,
				u"pixel buffer allocation failed"_q);
		}
		image.setDevicePixelRatio(scale);
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

		// Same-depth conversion on an rvalue converts in place, so the
		// peak stays one image; a null result (fallback-path allocation
		// failure) must fail the render, not report ok.
		output.image = std::move(image).convertToFormat(
			QImage::Format_RGBA8888);
		if (output.image.isNull()) {
			return fail(
				u"renderer.allocation"_q,
				u"pixel buffer allocation failed"_q);
		}
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
		// width/height above are physical pixels (viewportWidth*scale);
		// geometry and hit targets below stay in logical pixels.
		metadata.insert(u"scale"_q, scale);
		metadata.insert(u"diagnostics"_q, diagnostics);
		metadata.insert(u"geometry"_q, geometry);
		metadata.insert(u"mediaRefs"_q, mediaRefs);
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
