// SPDX-License-Identifier: GPL-3.0-or-later
#include "App.h"

#include "core/Paths.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

namespace hop {

App::App(QObject *parent) : QObject(parent)
{
    m_scanner.setArtCacheDir(Paths::artCacheDir());
    connect(&m_scanner, &Scanner::started, this, &App::scanningChanged);
    connect(&m_scanner, &Scanner::progress, this, [this](int done, int total) {
        m_scanDone = done;
        m_scanTotal = total;
        emit scanProgressChanged();
    });
    connect(&m_scanner, &Scanner::finished, this, [this](int added, int updated, int removed, int) {
        emit scanningChanged();
        if (added || updated || removed || m_songs.count() == 0) reload();
    });
}

bool App::init()
{
    Paths::ensureDirs();
    if (!m_index.open(Paths::indexDbPath())) {
        qWarning("hop-tunes: cannot open %s: %s", qPrintable(Paths::indexDbPath()), qPrintable(m_index.lastError()));
        return false;
    }
    QSettings s;
    m_folders = s.value(QStringLiteral("library/folders")).toStringList();
    if (m_folders.isEmpty() && !s.contains(QStringLiteral("library/folders"))) {
        // First run: offer ~/Music automatically if it exists.
        const QString music = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
        if (QFileInfo(music).isDir()) m_folders << QFileInfo(music).canonicalFilePath();
        saveFolders();
    }
    reload();
    rescan();
    return true;
}

QString App::version() const { return QCoreApplication::applicationVersion(); }

void App::addFolder(const QUrl &folder)
{
    const QString path = QFileInfo(folder.isLocalFile() ? folder.toLocalFile() : folder.toString())
                             .canonicalFilePath();
    if (path.isEmpty() || m_folders.contains(path)) return;
    m_folders << path;
    saveFolders();
    emit foldersChanged();
    rescan();
}

void App::removeFolder(const QString &path)
{
    if (!m_folders.removeAll(path)) return;
    saveFolders();
    emit foldersChanged();
    reload();
}

void App::rescan()
{
    if (m_folders.isEmpty()) return;
    m_scanDone = m_scanTotal = 0;
    emit scanProgressChanged();
    m_scanner.start(m_folders);
}

void App::playSongs(int row)
{
    if (row < 0 || row >= m_songs.count()) return;
    m_player.setQueue(m_songs.tracks(), row);
}

void App::shuffleAll()
{
    QList<Track> q = m_songs.tracks();
    if (q.isEmpty()) return;
    std::shuffle(q.begin(), q.end(), *QRandomGenerator::global());
    m_player.setQueue(q, 0);
}

void App::reload()
{
    m_songs.setTracks(m_index.tracksUnder(m_folders));
}

void App::saveFolders()
{
    QSettings s;
    s.setValue(QStringLiteral("library/folders"), m_folders);
}

} // namespace hop
