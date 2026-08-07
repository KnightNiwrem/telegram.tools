/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).

Sessionless render facade (plan Phases 2 and 4): canonical JSON request in,
RGBA pixels + geometry + diagnostics out, through the unmodified native
preparation (TryPrepareNativeInstantView) and MarkdownArticle layout/paint
path. Behavioral reference: HistoryView::Controls::RichDraftPreview.
*/
#pragma once

#include "adapters/canonical_page.h"
#include "core/sessionless_media_runtime.h"

#include <QtCore/QJsonObject>
#include <QtGui/QImage>

#include <memory>
#include <vector>

namespace Ttr {

struct RenderOutput {
	QImage image;
	QJsonObject metadata; // width/height/diagnostics/geometry/... (no pixels)
	bool ok = false;
};

class RenderService final {
public:
	RenderService();
	~RenderService();

	RenderService(const RenderService &) = delete;
	RenderService &operator=(const RenderService &) = delete;

	bool registerMedia(
		const QString &ref,
		const QByteArray &bytes,
		const QString &mimeType);
	void clearMedia();

	// `request` is the SDK RenderRequest JSON (renderer/js/sdk/types.ts).
	[[nodiscard]] RenderOutput render(const QJsonObject &request);

	[[nodiscard]] static QJsonObject VersionInfo();

private:
	class Impl;

	std::unique_ptr<Impl> _impl;

};

} // namespace Ttr
