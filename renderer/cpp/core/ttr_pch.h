/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).

Precompiled ambient environment matching what the Telegram target's own
stdafx provides to the pinned sources compiled into ttr_core
(data_location.cpp and the harness TUs): base typedefs, ClickHandler, and
the generated MTProto scheme types referenced by data/ headers.
*/
#pragma once

#include "base/assertion.h"
#include "base/basic_types.h"
#include "ui/click_handler.h"

#include "scheme.h"

// Provided by data/data_types.h in the Telegram target's own PCH; the
// harness only needs the forward declaration for data/data_location.h.
struct GeoPointLocation;
