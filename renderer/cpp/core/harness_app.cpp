/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE). Event-nesting bookkeeping adapted from
Telegram/SourceFiles/tests/test_main.cpp at the pinned revision.
*/
#include "core/harness_app.h"

#include "ui/effects/animations.h"
#include "ui/emoji_config.h"
#include "ui/style/style_core.h"

#include <QDir>
#include <QEvent>
#include <QStandardPaths>
#include <QThread>

namespace Ttr {
namespace {

HarnessApp *GlobalApp = nullptr;
bool HarnessInitialized = false;
int HarnessInitResult = -1;

} // namespace

HarnessApp::HarnessApp(int &argc, char **argv)
: QApplication(argc, argv)
, _mainThreadId(QThread::currentThreadId()) {
	GlobalApp = this;
}

HarnessApp &HarnessApp::Instance() {
	Expects(GlobalApp != nullptr);

	return *GlobalApp;
}

bool HarnessApp::nativeEventFilter(
		const QByteArray &eventType,
		void *message,
		native_event_filter_result *result) {
	registerEnterFromEventLoop();
	return false;
}

void HarnessApp::checkForEmptyLoopNestingLevel() {
	if (_loopNestingLevel == _eventNestingLevel) {
		Assert(_postponedCalls.empty()
			|| _postponedCalls.back().loopNestingLevel < _loopNestingLevel);
		Assert(!_previousLoopNestingLevels.empty());

		_loopNestingLevel = _previousLoopNestingLevels.back();
		_previousLoopNestingLevels.pop_back();
	}
}

void HarnessApp::postponeCall(FnMut<void()> &&callable) {
	Expects(callable != nullptr);
	Expects(_eventNestingLevel >= _loopNestingLevel);

	checkForEmptyLoopNestingLevel();
	_postponedCalls.push_back({
		_loopNestingLevel,
		std::move(callable)
	});
}

void HarnessApp::processPostponedCalls(int level) {
	while (!_postponedCalls.empty()) {
		auto &last = _postponedCalls.back();
		if (last.loopNestingLevel != level) {
			break;
		}
		auto taken = std::move(last);
		_postponedCalls.pop_back();
		taken.callable();
	}
}

void HarnessApp::incrementEventNestingLevel() {
	++_eventNestingLevel;
}

void HarnessApp::decrementEventNestingLevel() {
	Expects(_eventNestingLevel >= _loopNestingLevel);

	if (_eventNestingLevel == _loopNestingLevel) {
		_loopNestingLevel = _previousLoopNestingLevels.back();
		_previousLoopNestingLevels.pop_back();
	}
	const auto processTillLevel = _eventNestingLevel - 1;
	processPostponedCalls(processTillLevel);
	checkForEmptyLoopNestingLevel();
	_eventNestingLevel = processTillLevel;

	Ensures(_eventNestingLevel >= _loopNestingLevel);
}

void HarnessApp::registerEnterFromEventLoop() {
	Expects(_eventNestingLevel >= _loopNestingLevel);

	if (_eventNestingLevel > _loopNestingLevel) {
		_previousLoopNestingLevels.push_back(_loopNestingLevel);
		_loopNestingLevel = _eventNestingLevel;
	}
}

void HarnessApp::customEnterFromEventLoop(FnMut<void()> &&method) {
	registerEnterFromEventLoop();
	const auto wrap = EventNestingLevelGuard(this);
	method();
}

bool HarnessApp::notify(QObject *receiver, QEvent *e) {
	if (QThread::currentThreadId() != _mainThreadId) {
		return QApplication::notify(receiver, e);
	}
	const auto wrap = EventNestingLevelGuard(this);
	if (e->type() == QEvent::UpdateRequest) {
		const auto weak = QPointer<QObject>(receiver);
		_widgetUpdateRequests.fire({});
		if (!weak) {
			return true;
		}
	}
	return QApplication::notify(receiver, e);
}

rpl::producer<> HarnessApp::widgetUpdateRequests() const {
	return _widgetUpdateRequests.events();
}

void HarnessBaseIntegration::enterFromEventLoop(FnMut<void()> &&method) {
	HarnessApp::Instance().customEnterFromEventLoop(std::move(method));
}

bool HarnessBaseIntegration::logSkipDebug() {
	return true;
}

void HarnessBaseIntegration::logMessageDebug(const QString &message) {
}

void HarnessBaseIntegration::logMessage(const QString &message) {
}

void HarnessBaseIntegration::logAssertionViolation(const QString &info) {
}

void HarnessUiIntegration::postponeCall(FnMut<void()> &&callable) {
	HarnessApp::Instance().postponeCall(std::move(callable));
}

void HarnessUiIntegration::registerLeaveSubscription(
	not_null<QWidget*> widget) {
}

void HarnessUiIntegration::unregisterLeaveSubscription(
	not_null<QWidget*> widget) {
}

QString HarnessUiIntegration::emojiCacheFolder() {
	return QStandardPaths::writableLocation(
		QStandardPaths::TempLocation) + u"/ttr-emoji"_q;
}

QString HarnessUiIntegration::openglCheckFilePath() {
	return QStandardPaths::writableLocation(
		QStandardPaths::TempLocation) + u"/ttr-opengl"_q;
}

QString HarnessUiIntegration::angleBackendFilePath() {
	return QStandardPaths::writableLocation(
		QStandardPaths::TempLocation) + u"/ttr-angle"_q;
}

void HarnessUiIntegration::touchCounterIncrement() {
	++_touchCounter;
}

int HarnessUiIntegration::touchCounterNow() {
	return _touchCounter;
}

int InitializeHarness(int scale) {
	if (HarnessInitialized) {
		return HarnessInitResult;
	}
	HarnessInitialized = true;
	if (!GlobalApp) {
		// The host must construct HarnessApp (and the integrations) before
		// initialization; the WASM and native hosts both do.
		HarnessInitResult = 1;
		return HarnessInitResult;
	}
	style::SetDevicePixelRatio(1);
	new Ui::Animations::Manager();
	style::StartManager(std::clamp(
		scale,
		style::kScaleMin,
		style::MaxScaleForRatio(1)));
	Ui::Emoji::Init();
	HarnessInitResult = 0;
	return HarnessInitResult;
}

} // namespace Ttr

namespace crl {

rpl::producer<> on_main_update_requests() {
	return Ttr::HarnessApp::Instance().widgetUpdateRequests();
}

} // namespace crl
