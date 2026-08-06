/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).
*/
#include "core/sessionless_media_runtime.h"

#include "ui/dynamic_image.h"

#include <QtCore/QBuffer>
#include <QtGui/QImageReader>

namespace Ttr {
namespace {

// Decode-time guards (plan Phase 8): registration fails safely instead of
// letting a hostile asset exhaust WASM memory.
constexpr auto kMaxDecodedDimension = 4096;

class StaticImage final : public Ui::DynamicImage {
public:
	explicit StaticImage(QImage image) : _image(std::move(image)) {
	}

	std::shared_ptr<Ui::DynamicImage> clone() override {
		return std::make_shared<StaticImage>(_image);
	}

	QImage image(int size) override {
		if (_image.isNull() || size <= 0) {
			return _image;
		}
		if (_scaled.width() == size || _scaled.height() == size) {
			return _scaled;
		}
		_scaled = _image.scaled(
			size,
			size,
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation);
		return _scaled;
	}

	void subscribeToUpdates(Fn<void()> callback) override {
		// Static assets never change; loaded state is final at creation.
	}

private:
	QImage _image;
	QImage _scaled;

};

class AssetPhotoRuntime final : public Iv::Markdown::PhotoRuntime {
public:
	explicit AssetPhotoRuntime(std::shared_ptr<const MediaAsset> asset)
	: _asset(std::move(asset)) {
	}

	std::shared_ptr<Ui::DynamicImage> thumbnail(QSize size) const override {
		return full(size);
	}

	std::shared_ptr<Ui::DynamicImage> full(QSize size) const override {
		return _asset
			? std::make_shared<StaticImage>(_asset->decoded)
			: nullptr;
	}

	bool loaded() const override {
		return (_asset != nullptr);
	}

	bool loading() const override {
		return false;
	}

	double progress() const override {
		return _asset ? 1. : 0.;
	}

	void open(Qt::MouseButton button) const override {
		// Preview-only runtime: activation is reported through hit targets,
		// never handled inside the renderer.
	}

private:
	std::shared_ptr<const MediaAsset> _asset;

};

class AssetDocumentRuntime final : public Iv::Markdown::DocumentRuntime {
public:
	explicit AssetDocumentRuntime(std::shared_ptr<const MediaAsset> asset)
	: _asset(std::move(asset)) {
	}

	std::shared_ptr<Ui::DynamicImage> thumbnail(QSize size) const override {
		return full(size);
	}

	std::shared_ptr<Ui::DynamicImage> full(QSize size) const override {
		return (_asset && !_asset->decoded.isNull())
			? std::make_shared<StaticImage>(_asset->decoded)
			: nullptr;
	}

	bool loaded() const override {
		return (_asset != nullptr);
	}

	bool loading() const override {
		return false;
	}

	double progress() const override {
		return _asset ? 1. : 0.;
	}

	void open(Qt::MouseButton button) const override {
	}

private:
	std::shared_ptr<const MediaAsset> _asset;

};

} // namespace

bool MediaStore::registerAsset(
		const QString &ref,
		const QByteArray &bytes,
		const QString &mimeType) {
	auto buffer = QBuffer();
	buffer.setData(bytes);
	auto reader = QImageReader(&buffer);
	reader.setAutoTransform(true);
	const auto bounds = reader.size();
	if (bounds.isValid()
		&& (bounds.width() > kMaxDecodedDimension
			|| bounds.height() > kMaxDecodedDimension)) {
		return false;
	}
	auto decoded = reader.read();
	if (decoded.isNull()) {
		// Non-image media (audio bytes, undecodable video) may still be
		// registered so the block shows metadata; keep an empty image.
		decoded = QImage();
	} else if (decoded.width() > kMaxDecodedDimension
		|| decoded.height() > kMaxDecodedDimension) {
		return false;
	}
	_assets[ref] = std::make_shared<const MediaAsset>(MediaAsset{
		.decoded = std::move(decoded),
		.mimeType = mimeType,
	});
	return true;
}

void MediaStore::clear() {
	_assets.clear();
}

std::shared_ptr<const MediaAsset> MediaStore::lookup(
		const QString &ref) const {
	const auto found = _assets.find(ref);
	return (found != _assets.end()) ? found->second : nullptr;
}

SessionlessMediaRuntime::SessionlessMediaRuntime(
	std::shared_ptr<const MediaStore> store,
	std::vector<MediaIdBinding> bindings)
: _store(std::move(store)) {
	for (auto &binding : bindings) {
		_refsById.emplace(binding.id, std::move(binding.ref));
	}
}

std::shared_ptr<const MediaAsset> SessionlessMediaRuntime::assetById(
		uint64 id) const {
	const auto found = _refsById.find(id);
	return ((found != _refsById.end()) && _store)
		? _store->lookup(found->second)
		: nullptr;
}

std::shared_ptr<Ui::DynamicImage> SessionlessMediaRuntime::resolveInlineImage(
		uint64 documentId,
		QSize size) const {
	const auto asset = assetById(documentId);
	return (asset && !asset->decoded.isNull())
		? std::make_shared<StaticImage>(asset->decoded)
		: nullptr;
}

std::shared_ptr<Iv::Markdown::PhotoRuntime> SessionlessMediaRuntime::resolvePhoto(
		uint64 photoId) const {
	return std::make_shared<AssetPhotoRuntime>(assetById(photoId));
}

auto SessionlessMediaRuntime::resolveDocument(uint64 documentId) const
-> std::shared_ptr<Iv::Markdown::DocumentRuntime> {
	return std::make_shared<AssetDocumentRuntime>(assetById(documentId));
}

} // namespace Ttr
