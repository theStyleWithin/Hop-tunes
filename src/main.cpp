// SPDX-License-Identifier: GPL-3.0-or-later
#include "App.h"
#include "core/Paths.h"
#include "core/ProfileStore.h"
#ifdef HOPTUNES_HAVE_GST
#include "platform/Mpris.h"
#endif
#ifdef Q_OS_WIN
#include "platform/WinRuntime.h"
#endif
#include "ui/MpvVideoItem.h"

#include <QCommandLineParser>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>

#include <cstdio>

#ifdef HOPTUNES_HAVE_GST
#include <gst/gst.h>
#endif

int main(int argc, char *argv[])
{
    // QQuickFramebufferObject (MpvVideoItem, spec §5b) needs the OpenGL scenegraph backend;
    // must be set before QGuiApplication exists.
    // HOPTUNES_SOFTWARE_RENDER=1 draws without OpenGL (headless smoke tests, machines with no usable
    // GPU driver); video through libmpv needs the OpenGL path, everything else works.
    QQuickWindow::setGraphicsApi(qEnvironmentVariableIsSet("HOPTUNES_SOFTWARE_RENDER")
                                     ? QSGRendererInterface::Software : QSGRendererInterface::OpenGL);
#ifdef Q_OS_WIN
    hop::winruntime::setupBundledRuntime();
    for (int i = 1; i < argc; ++i) {
        const QByteArray a(argv[i]);
        if (a == "--version" || a == "-v" || a == "--help" || a == "-h" || a == "--forget-pin") {
            hop::winruntime::attachConsoleForTextOutput();
            break;
        }
    }
#endif
    // --help / --version answer without a display (packaging checks, servers).
    for (int i = 1; i < argc; ++i) {
        const QByteArray a(argv[i]);
        if (a == "--version" || a == "-v") { printf("hop-tunes %s\n", HOPTUNES_VERSION); return 0; }
        if (a == "--help" || a == "-h") {
            printf("Usage: hop-tunes [options] [files or folders...]\n"
                   "Hop-tunes: a local-first music player. Offline, no accounts.\n\n"
                   "Options:\n"
                   "  -h, --help                Show this help.\n"
                   "  -v, --version             Show the version.\n"
                   "  --forget-pin <profile>    Remove a forgotten PIN from a profile.\n\n"
                   "Files and folders given here start playing. If Hop-tunes is already running,\n"
                   "they are sent to that window instead.\n");
            return 0;
        }
        if (a == "--forget-pin") {
            // A PIN is a privacy curtain (spec §3.2), so whoever can run Hop-tunes as this
            // user may lift it. Nothing else in the profile changes.
            if (i + 1 >= argc) { fprintf(stderr, "usage: hop-tunes --forget-pin <profile name>\n"); return 2; }
            const QString name = QString::fromLocal8Bit(argv[i + 1]);
            hop::ProfileStore store(hop::Paths::profilesDir());
            if (!store.load()) { fprintf(stderr, "cannot read profiles: %s\n", qPrintable(store.lastError())); return 1; }
            for (const hop::Profile &p : store.profiles()) {
                if (p.name.compare(name, Qt::CaseInsensitive) != 0) continue;
                if (!p.hasPin) { printf("profile \"%s\" has no PIN\n", qPrintable(p.name)); return 0; }
                if (!store.setPin(p.id, {})) { fprintf(stderr, "could not save: %s\n", qPrintable(store.lastError())); return 1; }
                printf("removed the PIN from \"%s\"\n", qPrintable(p.name));
                return 0;
            }
            fprintf(stderr, "no profile called \"%s\"\n", qPrintable(name));
            return 1;
        }
    }
#ifdef HOPTUNES_HAVE_GST
    gst_init(&argc, &argv);
    // The GStreamer video fallback's QML type (GstVideoSurfaceView.qml, used when
    // HOPTUNES_HAVE_MPV is off) only self-registers once its plugin .so is actually
    // loaded — and VideoPlayerView.qml always parses that file's imports at startup,
    // whichever engine ends up active. Force the load now, before QGuiApplication exists
    // (verified necessary in exactly that order, not after).
    if (GstElement *probe = gst_element_factory_make("qml6glsink", nullptr)) gst_object_unref(probe);
#endif

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("hop-tunes"));
    QGuiApplication::setOrganizationName(QStringLiteral("hop-tunes"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Hop-tunes"));
    QGuiApplication::setApplicationVersion(QStringLiteral(HOPTUNES_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.thestylewithin.Hop-tunes"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Command line: hop-tunes [--help] [--version] [files or folders...]
    // Unknown options are ignored (desktop launchers sometimes add their own).
    QCommandLineParser parser;
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("Music files or folders to play."), QStringLiteral("[files...]"));
    parser.parse(QCoreApplication::arguments());
    QStringList files;
    for (const QString &f : parser.positionalArguments())
        files << (f.startsWith(QLatin1String("file:")) ? f : QFileInfo(f).absoluteFilePath());

    // One window per session: a second launch hands its files to the running one.
    const bool devRun = qEnvironmentVariableIsSet("HOPTUNES_SCREENSHOT") || qEnvironmentVariableIsSet("HOPTUNES_NO_MPRIS");
#ifdef HOPTUNES_HAVE_GST
    if (!devRun) {
        QStringList uris;
        for (const QString &f : files) uris << (f.startsWith(QLatin1String("file:")) ? f : QUrl::fromLocalFile(f).toString());
        if (hop::Mpris::forwardToRunningInstance(uris)) return 0;
    }
#else
    Q_UNUSED(devRun)
#endif

    // The design uses Jost (SIL OFL), bundled so the app looks the same on every distro.
    // Its internal family name is "Jost*"; the QML asks for "Jost".
    QString jostFamily;
    for (const char *f : {":/fonts/Jost-300-Light.ttf", ":/fonts/Jost-400-Book.ttf", ":/fonts/Jost-500-Medium.ttf"}) {
        const int id = QFontDatabase::addApplicationFont(QString::fromLatin1(f));
        if (id >= 0 && jostFamily.isEmpty()) jostFamily = QFontDatabase::applicationFontFamilies(id).value(0);
    }
    QStringList fallbacks{QStringLiteral("Inter"), QStringLiteral("Noto Sans"),
                          QStringLiteral("Cantarell"), QStringLiteral("DejaVu Sans")};
    if (!jostFamily.isEmpty() && jostFamily != QLatin1String("Jost")) fallbacks.prepend(jostFamily);
    if (jostFamily != QLatin1String("Jost")) QFont::insertSubstitutions(QStringLiteral("Jost"), fallbacks);

    hop::App hopApp;
    if (!hopApp.init()) return 1;

    hop::registerVideoQmlTypes();
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("hop"), &hopApp);
    // Developer hook: HOPTUNES_VIEW=collection|artists|albums|songs|nowplaying|quickplay|settings picks the first view.
    engine.rootContext()->setContextProperty(QStringLiteral("initialView"),
                                             QString::fromLocal8Bit(qgetenv("HOPTUNES_VIEW")));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;

#ifdef HOPTUNES_HAVE_GST
    // Media keys / desktop media widgets.
    hop::Mpris mpris(hopApp.player());
    QObject::connect(&mpris, &hop::Mpris::quitRequested, &app, &QCoreApplication::quit);
    QObject::connect(&mpris, &hop::Mpris::openUriRequested, &hopApp, &hop::App::openUri);
    QObject::connect(&mpris, &hop::Mpris::raiseRequested, &app, [&engine] {
        if (auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) { w->show(); w->raise(); w->requestActivate(); }
    });
    mpris.start();
#endif
    hopApp.openFiles(files);   // played now, or once a profile is open

    // Developer hook: HOPTUNES_AUTOPLAY=<row> starts playing that song once the library has loaded.
    bool autoplayOk = false;
    const int autoplayRow = qEnvironmentVariableIntValue("HOPTUNES_AUTOPLAY", &autoplayOk);
    if (autoplayOk) {
        auto tryPlay = [&hopApp, autoplayRow] {
            if (!hopApp.player()->hasTrack() && hopApp.songs()->count() > autoplayRow) hopApp.playSongs(autoplayRow);
        };
        QObject::connect(hopApp.songs(), &hop::TrackListModel::countChanged, &app, tryPlay);
        tryPlay();
    }

    // Developer hook: HOPTUNES_SCREENSHOT=/path/out.png saves the window after startup, then quits.
    const QString shot = QString::fromLocal8Bit(qgetenv("HOPTUNES_SCREENSHOT"));
    if (!shot.isEmpty()) {
        QTimer::singleShot(2500, &app, [&engine, &hopApp, shot] {
            bool saved = false;
            if (auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()))
                saved = w->grabWindow().save(shot);
            // One machine-readable line for automated smoke tests.
            const hop::Player *p = hopApp.player();
            fprintf(stdout, "hop-tunes selftest: version=%s profile=\"%s\" songs=%d playing=%d position_ms=%lld screenshot=%d error=\"%s\"\n",
                    HOPTUNES_VERSION, qPrintable(hopApp.profile().value(QStringLiteral("name")).toString()), hopApp.songs()->count(), p->isPlaying() ? 1 : 0,
                    static_cast<long long>(p->position()), saved ? 1 : 0, qPrintable(p->error()));
            fflush(stdout);
            QCoreApplication::exit(saved ? 0 : 2);
        });
    }
    return app.exec();
}
