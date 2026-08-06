/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).

Link seams for the sessionless harness (plan Phase 2: "removing a required
upstream dependency causes an explicit build failure, not a silent alternate
renderer"). Exactly one upstream symbol is stubbed here, and only because
its real implementation (history_location_manager.cpp) transitively includes
the whole application shell:

- LocationClickHandler::Url is called by
  iv_markdown_prepare_native_blocks.cpp (map blocks) to build the external
  map-provider URL for a coordinate. The stub reproduces the same URL format
  so map hit targets keep working without linking mainwidget.
*/
#include "data/data_location.h"
#include "history/history_location_manager.h"

QString LocationClickHandler::Url(const Data::LocationPoint &point) {
	// Byte-identical to history_location_manager.cpp at the pinned revision.
	const auto latlon = point.latAsString() + ',' + point.lonAsString();
	return u"https://maps.google.com/maps?q="_q
		+ latlon
		+ u"&ll="_q
		+ latlon
		+ u"&z=16"_q;
}
