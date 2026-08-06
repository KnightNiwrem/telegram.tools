/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).

Headless application bootstrap for the sessionless renderer (plan Phase 2).
Adapted from Telegram/SourceFiles/tests/test_main.{h,cpp} at the pinned
revision: the same QApplication event-nesting bookkeeping crl requires, and
the same base/Ui Integration seams, minus any window creation.
*/
#pragma once

#include "base/integration.h"
#include "ui/integration.h"

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QPointer>

#include <rpl/event_stream.h>

#include <vector>

namespace Ttr {

class HarnessApp final
	: public QApplication
	, public QAbstractNativeEventFilter {
public:
	using native_event_filter_result = qintptr;

	HarnessApp(int &argc, char **argv);

	bool notify(QObject *receiver, QEvent *e) override;
	bool nativeEventFilter(
		const QByteArray &eventType,
		void *message,
		native_event_filter_result *result) override;

	void postponeCall(FnMut<void()> &&callable);
	void customEnterFromEventLoop(FnMut<void()> &&method);

	[[nodiscard]] rpl::producer<> widgetUpdateRequests() const;

	[[nodiscard]] static HarnessApp &Instance();

private:
	struct PostponedCall {
		int loopNestingLevel = 0;
		FnMut<void()> callable;
	};

	void incrementEventNestingLevel();
	void decrementEventNestingLevel();
	void registerEnterFromEventLoop();
	void checkForEmptyLoopNestingLevel();
	void processPostponedCalls(int level);

	struct EventNestingLevelGuard {
		explicit EventNestingLevelGuard(HarnessApp *app) : app(app) {
			app->incrementEventNestingLevel();
		}
		~EventNestingLevelGuard() {
			app->decrementEventNestingLevel();
		}
		HarnessApp *app = nullptr;
	};

	Qt::HANDLE _mainThreadId = nullptr;
	int _eventNestingLevel = 0;
	int _loopNestingLevel = 0;
	std::vector<int> _previousLoopNestingLevels;
	std::vector<PostponedCall> _postponedCalls;
	rpl::event_stream<> _widgetUpdateRequests;

};

class HarnessBaseIntegration final : public base::Integration {
public:
	using Integration::Integration;

	void enterFromEventLoop(FnMut<void()> &&method) override;
	bool logSkipDebug() override;
	void logMessageDebug(const QString &message) override;
	void logMessage(const QString &message) override;
	void logAssertionViolation(const QString &info) override;
};

class HarnessUiIntegration final : public Ui::Integration {
public:
	void postponeCall(FnMut<void()> &&callable) override;
	void registerLeaveSubscription(not_null<QWidget*> widget) override;
	void unregisterLeaveSubscription(not_null<QWidget*> widget) override;
	QString emojiCacheFolder() override;
	QString openglCheckFilePath() override;
	QString angleBackendFilePath() override;
	void touchCounterIncrement() override;
	int touchCounterNow() override;

private:
	int _touchCounter = 0;

};

// Initializes the Qt runtime, integrations, animation manager, style
// manager, and emoji configuration exactly once. Returns 0 on success.
// `scale` is the UI scale in percent (canonical profile: 100).
[[nodiscard]] int InitializeHarness(int scale);

} // namespace Ttr
