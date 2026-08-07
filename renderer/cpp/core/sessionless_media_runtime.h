/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).

Sessionless Iv::Markdown::MediaRuntime (plan Phases 2 and 8): resolves the
sequential photo/document ids assigned by the canonical adapter to media
bytes registered by the host application. No Telegram session, no network.
*/
#pragma once

#include "adapters/canonical_page.h"
#include "iv/markdown/iv_markdown_common.h"

#include <QtGui/QImage>

#include <map>
#include <memory>

namespace Ttr {

struct MediaAsset {
	QImage decoded;
	QString mimeType;
};

class MediaStore final {
public:
	// Decodes and stores bytes under a canonical ref, replacing any
	// previous asset with the same ref (freeing its buffers).
	bool registerAsset(
		const QString &ref,
		const QByteArray &bytes,
		const QString &mimeType);
	void clear();

	[[nodiscard]] std::shared_ptr<const MediaAsset> lookup(
		const QString &ref) const;

private:
	std::map<QString, std::shared_ptr<const MediaAsset>> _assets;

};

class SessionlessMediaRuntime final : public Iv::Markdown::MediaRuntime {
public:
	SessionlessMediaRuntime(
		std::shared_ptr<const MediaStore> store,
		std::vector<MediaIdBinding> bindings);

	std::shared_ptr<Ui::DynamicImage> resolveInlineImage(
		uint64 documentId,
		QSize size) const override;
	std::shared_ptr<Iv::Markdown::PhotoRuntime> resolvePhoto(
		uint64 photoId) const override;
	std::shared_ptr<Iv::Markdown::DocumentRuntime> resolveDocument(
		uint64 documentId) const override;
	std::shared_ptr<Iv::Markdown::HostedMediaBlockFactory>
	hostedMediaBlockFactory() const override;

	// Decoded pixel size of the asset bound to this id, or an invalid size:
	// used to backfill photo/video blocks whose source declared no
	// dimensions (the prepare stage drops dimensionless media).
	[[nodiscard]] QSize assetDimensions(uint64 id) const;

private:
	[[nodiscard]] std::shared_ptr<const MediaAsset> assetById(
		uint64 id) const;

	std::shared_ptr<const MediaStore> _store;
	std::map<uint64, QString> _refsById;
	std::shared_ptr<Iv::Markdown::HostedMediaBlockFactory> _hostedFactory;

};

} // namespace Ttr
