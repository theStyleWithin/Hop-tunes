// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/WinRuntime.h"

#include "core/Paths.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QString>

#include <cstdio>

#include <windows.h>

namespace hop::winruntime {

namespace {

QString exeDir()
{
    wchar_t buf[32768];
    const DWORD n = GetModuleFileNameW(nullptr, buf, DWORD(sizeof buf / sizeof buf[0]));
    if (n == 0 || n >= sizeof buf / sizeof buf[0]) return {};
    return QFileInfo(QString::fromWCharArray(buf, int(n))).absolutePath();
}

} // namespace

void setupBundledRuntime()
{
    const QString dir = exeDir();
    if (dir.isEmpty()) return;
    const QString plugins = dir + QStringLiteral("/lib/gstreamer-1.0");
    if (!QDir(plugins).exists()) return;

    // Plugins and the plugin scanner load DLLs that sit next to hop-tunes.exe; a child process only
    // searches its own folder, so put ours on PATH for it.
    qputenv("PATH", (QDir::toNativeSeparators(dir) + QLatin1Char(';') + QString::fromLocal8Bit(qgetenv("PATH"))).toLocal8Bit());
    qputenv("GST_PLUGIN_PATH_1_0", QDir::toNativeSeparators(plugins).toLocal8Bit());
    qputenv("GST_PLUGIN_SYSTEM_PATH_1_0", QByteArray());   // ignore any other GStreamer on this PC
    const QString scanner = dir + QStringLiteral("/libexec/gstreamer-1.0/gst-plugin-scanner.exe");
    if (QFileInfo::exists(scanner))
        qputenv("GST_PLUGIN_SCANNER_1_0", QDir::toNativeSeparators(scanner).toLocal8Bit());
    // The plugin registry is rebuilt per install and must be writable, so it lives in the cache.
    const QString cache = hop::Paths::cacheDir();
    QDir().mkpath(cache);
    qputenv("GST_REGISTRY_1_0", QDir::toNativeSeparators(cache + QStringLiteral("/gst-registry.bin")).toLocal8Bit());
}

void attachConsoleForTextOutput()
{
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out != nullptr && out != INVALID_HANDLE_VALUE) return;   // already redirected somewhere
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return;
    FILE *f = nullptr;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
}

} // namespace hop::winruntime
