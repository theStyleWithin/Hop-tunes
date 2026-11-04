// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/DiagLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>

namespace hop::DiagLog {

namespace {
QMutex g_mutex;
QString g_dir;
QtMessageHandler g_previousHandler = nullptr;
constexpr qint64 kMaxBytes = 384 * 1024;   // above this the oldest half is dropped

QString logPath() { return g_dir + QStringLiteral("/hoptunes.log"); }

void append(const QString &line)
{
    QMutexLocker lock(&g_mutex);
    if (g_dir.isEmpty()) return;
    QFile f(logPath());
    if (f.size() > kMaxBytes && f.open(QIODevice::ReadWrite)) {   // keep the newest half
        f.seek(f.size() / 2);
        const QByteArray tail = f.readAll();
        f.resize(0);
        f.write(tail.mid(tail.indexOf('\n') + 1));
        f.close();
    }
    if (!f.open(QIODevice::Append)) return;
    f.write(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")).toUtf8());
    f.write(" ");
    f.write(line.toUtf8());
    f.write("\n");
}

void handler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    static const char *names[] = {"debug", "WARN", "CRIT", "FATAL", "info"};
    QString where;
    if (ctx.file && ctx.line > 0) where = QStringLiteral(" (%1:%2)").arg(QString::fromUtf8(ctx.file).section(QLatin1Char('/'), -1)).arg(ctx.line);
    append(QStringLiteral("[%1] %2%3").arg(QLatin1String(names[type < 5 ? type : 1]), msg, where));
    if (g_previousHandler) g_previousHandler(type, ctx, msg);
}

QString tail(const QString &file, int maxBytes)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return {};
    if (f.size() > maxBytes) f.seek(f.size() - maxBytes);
    return QString::fromUtf8(f.readAll());
}
} // namespace

void install(const QString &dir)
{
    {
        QMutexLocker lock(&g_mutex);
        g_dir = dir;
        QDir().mkpath(dir);
        QFile::remove(dir + QStringLiteral("/hoptunes.prev.log"));
        QFile::rename(dir + QStringLiteral("/hoptunes.log"), dir + QStringLiteral("/hoptunes.prev.log"));
        if (!g_previousHandler) g_previousHandler = qInstallMessageHandler(handler);
    }
    append(QStringLiteral("--- hop-tunes log started"));
}

bool installed() { QMutexLocker lock(&g_mutex); return !g_dir.isEmpty(); }
void note(const QString &line) { append(QStringLiteral("[note] ") + line); }
QString path() { QMutexLocker lock(&g_mutex); return g_dir.isEmpty() ? QString() : logPath(); }
QString current(int maxBytes) { QMutexLocker lock(&g_mutex); return g_dir.isEmpty() ? QString() : tail(logPath(), maxBytes); }
QString previous(int maxBytes) { QMutexLocker lock(&g_mutex); return g_dir.isEmpty() ? QString() : tail(g_dir + QStringLiteral("/hoptunes.prev.log"), maxBytes); }

} // namespace hop::DiagLog
