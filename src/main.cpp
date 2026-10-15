// SPDX-License-Identifier: GPL-3.0-or-later
#include "App.h"
#include "platform/Mpris.h"

#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include <cstdio>

#include <gst/gst.h>

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("hop-tunes"));
    QGuiApplication::setOrganizationName(QStringLiteral("hop-tunes"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Hop-tunes"));
    QGuiApplication::setApplicationVersion(QStringLiteral(HOPTUNES_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.thestylewithin.hoptunes"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

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

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("hop"), &hopApp);
    // Developer hook: HOPTUNES_VIEW=collection|artists|albums|songs|nowplaying|quickplay|settings picks the first view.
    engine.rootContext()->setContextProperty(QStringLiteral("initialView"),
                                             QString::fromLocal8Bit(qgetenv("HOPTUNES_VIEW")));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;

    // Media keys / desktop media widgets.
    hop::Mpris mpris(hopApp.player());
    QObject::connect(&mpris, &hop::Mpris::quitRequested, &app, &QCoreApplication::quit);
    QObject::connect(&mpris, &hop::Mpris::raiseRequested, &app, [&engine] {
        if (auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) { w->show(); w->raise(); w->requestActivate(); }
    });
    mpris.start();

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
            fprintf(stdout, "hop-tunes selftest: version=%s songs=%d playing=%d position_ms=%lld screenshot=%d error=\"%s\"\n",
                    HOPTUNES_VERSION, hopApp.songs()->count(), p->isPlaying() ? 1 : 0,
                    static_cast<long long>(p->position()), saved ? 1 : 0, qPrintable(p->error()));
            fflush(stdout);
            QCoreApplication::exit(saved ? 0 : 2);
        });
    }
    return app.exec();
}
