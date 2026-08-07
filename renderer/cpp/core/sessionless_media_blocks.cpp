/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).
*/
#include "core/sessionless_media_blocks.h"

#include "core/sessionless_media_runtime.h"

#include "iv/markdown/iv_markdown_media_block.h"
#include "iv/markdown/iv_markdown_prepare.h"
#include "ui/dynamic_image.h"
#include "ui/painter.h"

#include "styles/palette.h"

#include <cmath>

namespace Ttr {
namespace {

// A block taller than 4:1 would let hostile width/height attributes force
// a huge layout; the declared ratio is clamped, not trusted.
constexpr auto kMaxHeightPerWidth = 4;

class StaticMediaBlock final : public Iv::Markdown::MediaBlock {
public:
	StaticMediaBlock(
		uint64 stableId,
		QSize intrinsic,
		std::shared_ptr<Ui::DynamicImage> image,
		TextWithEntities caption)
	: _stableId(stableId)
	, _intrinsic(intrinsic)
	, _image(std::move(image))
	, _caption(std::move(caption)) {
	}

	uint64 stableId() const override {
		return _stableId;
	}

	int resizeGetHeight(int width) override {
		const auto safe = std::max(width, 1);
		const auto ratio = (_intrinsic.width() > 0 && _intrinsic.height() > 0)
			? (double(_intrinsic.height()) / _intrinsic.width())
			: 1.;
		_height = std::clamp(
			int(std::lround(safe * ratio)),
			1,
			safe * kMaxHeightPerWidth);
		return _height;
	}

	void setGeometry(QRect geometry) override {
		_geometry = geometry;
	}

	QRect geometry() const override {
		return _geometry;
	}

	int firstLineBaseline() const override {
		return 0;
	}

	void paint(
			Painter &p,
			const Iv::Markdown::MarkdownArticlePaintContext &context) const
			override {
		const auto rect = _geometry;
		if (rect.isEmpty()) {
			return;
		}
		auto frame = QImage();
		if (_image) {
			const auto side = std::max(rect.width(), rect.height());
			const auto scale = std::max(mediaPixelScale(), 1.);
			frame = _image->image(int(std::ceil(side * scale)));
		}
		if (frame.isNull()) {
			// No registered bytes: a flat placeholder at the declared
			// aspect ratio (upstream has no sessionless placeholder visual
			// to mirror; the harness owns this fill).
			p.fillRect(rect, st::windowBgOver);
			return;
		}
		const auto fitted = frame.size().scaled(
			rect.size(),
			Qt::KeepAspectRatio);
		auto target = QRect(QPoint(), fitted);
		target.moveCenter(rect.center());
		if (target != rect) {
			p.fillRect(rect, st::imageBg);
		}
		const auto hq = PainterHighQualityEnabler(p);
		p.drawImage(target, frame);
	}

	ClickHandlerPtr linkAt(QPoint point) const override {
		return nullptr;
	}

	Iv::Markdown::MediaActivation activationAt(QPoint point) const override {
		return {};
	}

	Iv::Markdown::MediaBlockSelectionData selectionData() const override {
		return {
			.copyText = _caption.text,
			.caption = _caption,
		};
	}

private:
	const uint64 _stableId = 0;
	const QSize _intrinsic;
	const std::shared_ptr<Ui::DynamicImage> _image;
	const TextWithEntities _caption;
	QRect _geometry;
	int _height = 1;

};

[[nodiscard]] std::shared_ptr<Ui::DynamicImage> LoadedImage(
		const std::shared_ptr<Iv::Markdown::PhotoRuntime> &runtime,
		QSize size) {
	return (runtime && runtime->loaded()) ? runtime->full(size) : nullptr;
}

[[nodiscard]] std::shared_ptr<Ui::DynamicImage> LoadedImage(
		const std::shared_ptr<Iv::Markdown::DocumentRuntime> &runtime,
		QSize size) {
	return (runtime && runtime->loaded()) ? runtime->full(size) : nullptr;
}

class SessionlessMediaBlockFactory final
	: public Iv::Markdown::HostedMediaBlockFactory {
public:
	explicit SessionlessMediaBlockFactory(
		const SessionlessMediaRuntime *runtime)
	: _runtime(runtime) {
	}

	std::shared_ptr<Iv::Markdown::MediaBlock> createPhoto(
			const Iv::Markdown::PreparedPhotoBlockData &prepared) const
			override {
		const auto size = QSize(prepared.width, prepared.height);
		return std::make_shared<StaticMediaBlock>(
			prepared.id.value,
			size,
			LoadedImage(_runtime->resolvePhoto(prepared.photoId), size),
			prepared.caption);
	}

	std::shared_ptr<Iv::Markdown::MediaBlock> createVideo(
			const Iv::Markdown::PreparedVideoBlockData &prepared) const
			override {
		const auto size = QSize(prepared.media.width, prepared.media.height);
		return std::make_shared<StaticMediaBlock>(
			prepared.id.value,
			size,
			LoadedImage(_runtime->resolveDocument(prepared.media.id), size),
			prepared.caption);
	}

private:
	// The runtime owns this factory, so the back-pointer cannot dangle.
	const SessionlessMediaRuntime *_runtime = nullptr;

};

} // namespace

std::shared_ptr<Iv::Markdown::HostedMediaBlockFactory>
MakeSessionlessMediaBlockFactory(const SessionlessMediaRuntime *runtime) {
	return std::make_shared<SessionlessMediaBlockFactory>(runtime);
}

} // namespace Ttr
