/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).

Platform layer for WebAssembly (plan Phase 3). The pinned tree has no
"wasm" platform; nice_target_sources excludes every win/mac/linux source
under Emscripten, and this TU supplies the platform symbols the renderer
subset genuinely reaches. Time functions mirror crl/linux/crl_linux_time.cpp
byte-for-byte (Emscripten's clock_gettime maps to performance.now()).
Everything else answers what a browser page truthfully is: no X11, no
Wayland, no compositor margins, no screen-reader bus.
*/
#ifdef __EMSCRIPTEN__

#include <rpl/rpl.h>

#include "base/platform/base_platform_info.h"
#include "base/platform/linux/base_screen_reader_state_linux.h"
#include "ui/platform/ui_platform_utility.h"

#include <crl/crl_time.h>

#include <time.h>

namespace crl::details {

void init() {
}

inner_time_type current_value() {
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	const auto seconds = inner_time_type(ts.tv_sec);
	const auto milliseconds = inner_time_type(ts.tv_nsec) / 1000000;
	return seconds * 1000 + milliseconds;
}

time convert(inner_time_type value) {
	return time(value);
}

inner_profile_type current_profile_value() {
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	const auto seconds = inner_profile_type(ts.tv_sec);
	const auto milliseconds = inner_profile_type(ts.tv_nsec) / 1000;
	return seconds * 1000000 + milliseconds;
}

profile_time convert_profile(inner_profile_type value) {
	return profile_time(value);
}

} // namespace crl::details

namespace Platform {

bool IsX11() {
	return false;
}

bool IsWayland() {
	return false;
}

} // namespace Platform

namespace base::Platform {

QString CurrentExecutablePath(int argc, char *argv[]) {
	return QStringLiteral("/ttr-renderer.wasm");
}

struct LinuxScreenReaderState::Private {
};

LinuxScreenReaderState::LinuxScreenReaderState()
: _private(nullptr) {
}

LinuxScreenReaderState::~LinuxScreenReaderState() = default;

bool LinuxScreenReaderState::active() const {
	return _isActive.current();
}

rpl::producer<bool> LinuxScreenReaderState::activeValue() const {
	return _isActive.value();
}

void LinuxScreenReaderState::accessibilityActiveChanged(bool active) {
	_isActive = active;
}

void LinuxScreenReaderState::refreshStatusProxy() {
}

void LinuxScreenReaderState::updateActive() {
}

} // namespace base::Platform

namespace Ui::Platform {

bool TranslucentWindowsSupported() {
	return false;
}

void SetWindowMargins(not_null<QWidget*> widget, const QMargins &margins) {
}

} // namespace Ui::Platform

#endif // __EMSCRIPTEN__
