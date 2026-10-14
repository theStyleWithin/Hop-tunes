// SPDX-License-Identifier: GPL-3.0-or-later
#include "App.h"

#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include <gst/gst.h>

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("hop-tunes"));
    QGuiApplication::setOrganizationName(QStringLiteral("hop-tunes"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Hop-tunes"));
    QGuiApplication::setApplicationVersion(QStringLiteral(HOPTUNES_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("hop-tunes"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    hop::App hopApp;
    if (!hopApp.init()) return 1;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("hop"), &hopApp);
    // Developer hook: HOPTUNES_VIEW=collection|nowplaying|quickplay|settings picks the first view.
    engine.rootContext()->setContextProperty(QStringLiteral("initialView"),
                                             QString::fromLocal8Bit(qgetenv("HOPTUNES_VIEW")));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;

    // Developer hook: HOPTUNES_SCREENSHOT=/path/out.png saves the window after startup, then quits.
    const QString shot = QString::fromLocal8Bit(qgetenv("HOPTUNES_SCREENSHOT"));
    if (!shot.isEmpty()) {
        QTimer::singleShot(2500, &app, [&engine, shot] {
            if (auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()))
                w->grabWindow().save(shot);
            QCoreApplication::quit();
        });
    }
    return app.exec();
}
