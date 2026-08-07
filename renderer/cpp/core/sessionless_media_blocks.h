/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).

Sessionless hosted media blocks (plan Phase 8): TDesktop's only upstream
HostedMediaBlockFactory is session-bound (HistoryView), so photo/video
blocks laid out through the markdown article need this harness-owned
implementation to occupy space and paint. Layout is pure arithmetic from
the prepared block dimensions; painting draws the registered static asset
aspect-fit, or a flat placeholder fill when no bytes are registered.
Audio, map, channel, and grouped-media blocks are not implemented yet and
keep their upstream fallback (no block, zero height).
*/
#pragma once

#include "iv/markdown/iv_markdown_common.h"

namespace Ttr {

class SessionlessMediaRuntime;

[[nodiscard]] std::shared_ptr<Iv::Markdown::HostedMediaBlockFactory>
MakeSessionlessMediaBlockFactory(const SessionlessMediaRuntime *runtime);

} // namespace Ttr
