/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).
*/
#include "wasm-host/ttr_abi.h"

#include "core/harness_app.h"
#include "core/render_service.h"
#include "core/version.h"

#include <QtCore/QByteArray>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <map>
#include <memory>
#include <string>

#if defined(__EMSCRIPTEN__)
namespace Ttr {
void RegisterEmbindPrimitives();
} // namespace Ttr
#endif // __EMSCRIPTEN__


namespace {

struct RendererContext {
	Ttr::RenderService service;
	// Keeps the last result JSON and pixel buffer alive per the ABI
	// ownership contract (valid until the next render or destroy).
	std::string lastResultJson;
	QImage lastImage;
};

struct AbiState {
	std::unique_ptr<Ttr::HarnessApp> app;
	std::unique_ptr<Ttr::HarnessBaseIntegration> baseIntegration;
	std::unique_ptr<Ttr::HarnessUiIntegration> uiIntegration;
	std::map<int, std::unique_ptr<RendererContext>> contexts;
	std::string versionJson;
	int nextHandle = 1;
	bool initialized = false;
	int initResult = -1;
};

AbiState *State() {
	static auto state = new AbiState();
	return state;
}

int FakeArgc = 1;
char FakeArg0[] = "ttr";
char *FakeArgv[] = { FakeArg0, nullptr };

RendererContext *Lookup(int handle) {
	auto &contexts = State()->contexts;
	const auto found = contexts.find(handle);
	return (found != contexts.end()) ? found->second.get() : nullptr;
}

} // namespace

// Qt's wasm target finalization links with INVOKE_RUN=0 and has the page
// loader call main explicitly; the renderer is driven purely through the
// ttr_* ABI, so main only has to exist and keep the runtime alive.
int main(int argc, char *argv[]) {
	return 0;
}

extern "C" {

int ttr_abi_version(void) {
	return Ttr::kAbiVersion;
}

int ttr_initialize(void) {
	auto state = State();
	if (state->initialized) {
		return state->initResult;
	}
	state->initialized = true;
#if defined(__EMSCRIPTEN__)
	Ttr::RegisterEmbindPrimitives();
#else // __EMSCRIPTEN__
	qputenv("QT_QPA_PLATFORM", "offscreen");
#endif // !__EMSCRIPTEN__
	state->app = std::make_unique<Ttr::HarnessApp>(FakeArgc, FakeArgv);
	state->app->installNativeEventFilter(state->app.get());
	state->baseIntegration = std::make_unique<Ttr::HarnessBaseIntegration>(
		FakeArgc,
		FakeArgv);
	base::Integration::Set(state->baseIntegration.get());
	state->uiIntegration = std::make_unique<Ttr::HarnessUiIntegration>();
	Ui::Integration::Set(state->uiIntegration.get());
	state->initResult = Ttr::InitializeHarness(100);
	return state->initResult;
}

const char *ttr_version(void) {
	auto state = State();
	if (state->versionJson.empty()) {
		state->versionJson = QJsonDocument(Ttr::RenderService::VersionInfo())
			.toJson(QJsonDocument::Compact)
			.toStdString();
	}
	return state->versionJson.c_str();
}

int ttr_create(void) {
	auto state = State();
	if (state->initResult != 0) {
		return 0;
	}
	const auto handle = state->nextHandle++;
	state->contexts.emplace(handle, std::make_unique<RendererContext>());
	return handle;
}

void ttr_destroy(int handle) {
	State()->contexts.erase(handle);
}

int ttr_register_media(
		int handle,
		const char *ref,
		const std::uint8_t *bytes,
		int size,
		const char *mime_type) {
	const auto context = Lookup(handle);
	if (!context || !ref || (!bytes && size > 0) || size < 0) {
		return 1;
	}
	const auto copied = QByteArray(
		reinterpret_cast<const char*>(bytes),
		size);
	return context->service.registerMedia(
		QString::fromUtf8(ref),
		copied,
		mime_type ? QString::fromUtf8(mime_type) : QString())
		? 0
		: 2;
}

void ttr_clear_media(int handle) {
	if (const auto context = Lookup(handle)) {
		context->service.clearMedia();
	}
}

const char *ttr_render(int handle, const char *request_json) {
	const auto context = Lookup(handle);
	if (!context || !request_json) {
		return nullptr;
	}
	auto parseError = QJsonParseError();
	const auto document = QJsonDocument::fromJson(
		QByteArray(request_json),
		&parseError);
	auto metadata = QJsonObject();
	if (parseError.error != QJsonParseError::NoError
		|| !document.isObject()) {
		metadata.insert(u"status"_q, u"error"_q);
		metadata.insert(u"diagnostics"_q, QJsonArray{ QJsonObject{
			{ u"severity"_q, u"error"_q },
			{ u"code"_q, u"abi.request-json"_q },
			{ u"message"_q, u"request is not a JSON object"_q },
		} });
	} else {
		auto output = context->service.render(document.object());
		metadata = output.metadata;
		if (output.ok) {
			context->lastImage = std::move(output.image);
			metadata.insert(
				u"pixelsOffset"_q,
				double(reinterpret_cast<std::uintptr_t>(
					context->lastImage.constBits())));
			metadata.insert(
				u"pixelsSize"_q,
				double(context->lastImage.width())
					* context->lastImage.height() * 4);
		}
	}
	context->lastResultJson = QJsonDocument(metadata)
		.toJson(QJsonDocument::Compact)
		.toStdString();
	return context->lastResultJson.c_str();
}

} // extern "C"
