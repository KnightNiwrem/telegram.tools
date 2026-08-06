/*
Part of the telegram.tools rich-message renderer (GPL-3.0 with the OpenSSL
exception, see renderer/LICENSE).

Native reference/test host (plan Phase 2): renders a canonical request JSON
to a PNG plus a metadata JSON, with no Telegram session or network access.

Usage:
  ttr_native_host --request request.json --out out.png --meta out.json \
      [--media ref=path.jpg]...
  ttr_native_host --version
*/
#include "adapters/canonical_page.h"
#include "core/harness_app.h"
#include "core/render_service.h"

#include <QtCore/QCommandLineParser>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QMimeDatabase>

#include <cstdio>

namespace {

[[nodiscard]] QByteArray ReadFile(const QString &path, bool *ok) {
	auto file = QFile(path);
	*ok = file.open(QIODevice::ReadOnly);
	return *ok ? file.readAll() : QByteArray();
}

} // namespace

int main(int argc, char *argv[]) {
	qputenv("QT_QPA_PLATFORM", "offscreen");

	Ttr::HarnessApp app(argc, argv);
	app.installNativeEventFilter(&app);

	Ttr::HarnessBaseIntegration base(argc, argv);
	base::Integration::Set(&base);

	Ttr::HarnessUiIntegration ui;
	Ui::Integration::Set(&ui);

	QCommandLineParser parser;
	parser.addHelpOption();
	const auto requestOption = QCommandLineOption(u"request"_q, {}, u"file"_q);
	const auto outOption = QCommandLineOption(u"out"_q, {}, u"file"_q);
	const auto metaOption = QCommandLineOption(u"meta"_q, {}, u"file"_q);
	const auto mediaOption = QCommandLineOption(u"media"_q, {}, u"ref=path"_q);
	const auto versionOption = QCommandLineOption(u"version"_q);
	parser.addOptions(
		{ requestOption, outOption, metaOption, mediaOption, versionOption });
	parser.process(app);

	if (parser.isSet(versionOption)) {
		const auto version = QJsonDocument(
			Ttr::RenderService::VersionInfo()).toJson();
		std::fputs(version.constData(), stdout);
		return 0;
	}
	if (!parser.isSet(requestOption) || !parser.isSet(outOption)) {
		std::fputs("--request and --out are required\n", stderr);
		return 2;
	}

	if (const auto status = Ttr::InitializeHarness(100); status != 0) {
		std::fputs("harness initialization failed\n", stderr);
		return 3;
	}

	auto readOk = false;
	const auto requestBytes = ReadFile(parser.value(requestOption), &readOk);
	if (!readOk) {
		std::fputs("cannot read request file\n", stderr);
		return 2;
	}
	auto parseError = QJsonParseError();
	const auto requestDocument
		= QJsonDocument::fromJson(requestBytes, &parseError);
	if (parseError.error != QJsonParseError::NoError
		|| !requestDocument.isObject()) {
		std::fputs("request is not a JSON object\n", stderr);
		return 2;
	}

	auto service = Ttr::RenderService();
	const auto mimeDatabase = QMimeDatabase();
	for (const auto &media : parser.values(mediaOption)) {
		const auto separator = media.indexOf(u'=');
		if (separator <= 0) {
			std::fputs("--media expects ref=path\n", stderr);
			return 2;
		}
		const auto ref = media.left(separator);
		const auto path = media.mid(separator + 1);
		auto mediaOk = false;
		const auto bytes = ReadFile(path, &mediaOk);
		if (!mediaOk) {
			std::fputs("cannot read media file\n", stderr);
			return 2;
		}
		const auto mime = mimeDatabase.mimeTypeForFileNameAndData(path, bytes);
		if (!service.registerMedia(ref, bytes, mime.name())) {
			std::fputs("media registration failed\n", stderr);
			return 2;
		}
	}

	const auto output = service.render(requestDocument.object());
	if (parser.isSet(metaOption)) {
		auto metaFile = QFile(parser.value(metaOption));
		if (!metaFile.open(QIODevice::WriteOnly)) {
			std::fputs("cannot write metadata file\n", stderr);
			return 2;
		}
		metaFile.write(QJsonDocument(output.metadata).toJson());
	}
	if (!output.ok) {
		std::fputs("render failed; see metadata diagnostics\n", stderr);
		return 1;
	}
	if (!output.image.save(parser.value(outOption), "PNG")) {
		std::fputs("cannot write output image\n", stderr);
		return 2;
	}
	return 0;
}
