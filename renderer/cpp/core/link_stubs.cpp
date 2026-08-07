/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).

Link seams for the sessionless harness (plan Phase 2: "removing a required
upstream dependency causes an explicit build failure, not a silent alternate
renderer"). Two upstream translation units are replaced here, both because
their real homes drag in the application shell, and both with definitions
copied byte-identically from the pinned sources:

- LocationClickHandler::Url (history_location_manager.cpp transitively
  includes mainwidget.h) — called by iv_markdown_prepare_native_blocks for
  map-block URLs.
- The three Data::LocationPoint members the markdown path uses
  (data_location.cpp additionally defines thumbnail helpers needing
  cScale/st::locationSize from the app's style set).
- Lang::details::Current/Value (lang_instance.cpp includes
  core/application.h and local storage): the harness serves the generated
  base-English values, which is exactly what a fresh unmodified TDesktop
  renders before a language pack is applied.
- Iv::EncodeRichPageLinkUrl/DecodeRichPageLinkUrl (iv_rich_page.cpp mixes
  them into the session-bound ParseRichPage translation unit):
  DecodeRichPageLinkUrl is the one iv_rich_page.cpp symbol the markdown
  prepare path calls.
*/
#include "base/basic_types.h"
#include "ui/click_handler.h"

#include "scheme.h"

#include <rpl/rpl.h>

#include "base/qthelp_url.h"
#include "data/data_location.h"
#include "iv/iv_rich_page.h"
#include "history/history_location_manager.h"
#include "lang_auto.h"

namespace Data {
namespace {

// Byte-identical to data_location.cpp at the pinned revision.
[[nodiscard]] QString AsString(float64 value) {
	constexpr auto kPrecision = 6;
	return QString::number(value, 'f', kPrecision);
}

} // namespace

LocationPoint::LocationPoint(float64 lat, float64 lon, IgnoreAccessHash)
: _lat(lat)
, _lon(lon) {
}

QString LocationPoint::latAsString() const {
	return AsString(_lat);
}

QString LocationPoint::lonAsString() const {
	return AsString(_lon);
}

} // namespace Data

namespace Iv {

// Byte-identical to iv_rich_page.cpp at the pinned revision.
QString EncodeRichPageLinkUrl(
		const QString &url,
		uint64 webpageId) {
	return u"internal:wrapped?url="_q
		+ qthelp::url_encode(url)
		+ u"&context=iv&webpage_id="_q
		+ QString::number(webpageId);
}

std::optional<RichPageLinkUrl> DecodeRichPageLinkUrl(const QString &data) {
	const auto wrappedPrefix = u"internal:wrapped?"_q;
	if (!data.startsWith(wrappedPrefix)) {
		return std::nullopt;
	}
	const auto params = qthelp::url_parse_params(data.mid(wrappedPrefix.size()));
	if (params.value(u"context"_q) != u"iv"_q) {
		return std::nullopt;
	}
	const auto url = params.value(u"url"_q);
	if (url.isEmpty()) {
		return std::nullopt;
	}
	const auto webpageIdValue = params.value(u"webpage_id"_q);
	auto webpageId = uint64();
	if (!webpageIdValue.isEmpty()) {
		auto ok = false;
		webpageId = webpageIdValue.toULongLong(&ok);
		if (!ok) {
			return std::nullopt;
		}
	}
	return RichPageLinkUrl{
		.url = url,
		.webpageId = webpageId,
	};
}

} // namespace Iv

namespace Lang {
namespace details {

QString Current(ushort key) {
	return GetOriginalValue(key);
}

rpl::producer<QString> Value(ushort key) {
	return rpl::single(GetOriginalValue(key));
}

} // namespace details
} // namespace Lang

QString LocationClickHandler::Url(const Data::LocationPoint &point) {
	// Byte-identical to history_location_manager.cpp at the pinned revision.
	const auto latlon = point.latAsString() + ',' + point.lonAsString();
	return u"https://maps.google.com/maps?q="_q
		+ latlon
		+ u"&ll="_q
		+ latlon
		+ u"&z=16"_q;
}
