// SPDX-License-Identifier: GPL-3.0-or-later
#include "App.h"

#include "core/Paths.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

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
    const QString keepArtist = selectedArtistName(), keepAlbum = selectedAlbumTitle();
    m_songs.setTracks(m_index.tracksUnder(m_folders));
    m_collection.build(m_songs.tracks());
    m_artists.setArtists(m_collection.artists());
    QList<Album> all;
    for (const Artist &ar : m_collection.artists())
        for (int i : ar.albumIndices) all.append(m_collection.albums()[i]);
    m_allAlbums.setAlbums(all);
    m_mosaicArt.clear();
    for (const Album &a : all)
        if (!a.art.isEmpty()) m_mosaicArt << QUrl::fromLocalFile(a.art).toString();
    emit libraryChanged();
    applySelection(keepArtist, keepAlbum);
}

QList<Track> App::albumTracks(const Album &a) const
{
    QList<Track> out;
    for (int row : a.trackRows) out.append(m_songs.tracks()[row]);
    return out;
}

void App::applySelection(const QString &artistName, const QString &albumTitle)
{
    const auto &artists = m_collection.artists();
    int ar = artists.isEmpty() ? -1 : 0;
    for (int i = 0; i < artists.size(); ++i)
        if (artists[i].name == artistName) { ar = i; break; }
    m_selArtist = ar;

    QList<Album> albums;
    if (ar >= 0)
        for (int i : artists[ar].albumIndices) albums.append(m_collection.albums()[i]);
    m_artistAlbums.setAlbums(albums);

    m_selAlbum = albums.isEmpty() ? -1 : 0;
    for (int i = 0; i < albums.size(); ++i)
        if (albums[i].title == albumTitle) { m_selAlbum = i; break; }
    m_albumSongs.setTracks(m_selAlbum >= 0 ? albumTracks(albums[m_selAlbum]) : QList<Track>{});
    emit selectionChanged();
}

void App::selectArtist(int row)
{
    if (row < 0 || row >= m_collection.artists().size()) return;
    applySelection(m_collection.artists()[row].name, {});
}

void App::selectAlbum(int row)
{
    const Album *a = m_artistAlbums.at(row);
    if (!a) return;
    m_selAlbum = row;
    m_albumSongs.setTracks(albumTracks(*a));
    emit selectionChanged();
}

void App::openAlbum(int allAlbumsRow)
{
    const Album *a = m_allAlbums.at(allAlbumsRow);
    if (a) applySelection(a->artist, a->title);
}

void App::playAlbum(int startRow)
{
    if (m_albumSongs.count() == 0) return;
    m_player.setQueue(m_albumSongs.tracks(), startRow);
}

QString App::selectedArtistName() const
{
    return (m_selArtist >= 0 && m_selArtist < m_collection.artists().size()) ? m_collection.artists()[m_selArtist].name : QString();
}

QString App::selectedAlbumTitle() const
{
    const Album *a = m_artistAlbums.at(m_selAlbum);
    return a ? a->title : QString();
}

QString App::selectedAlbumArt() const
{
    const Album *a = m_artistAlbums.at(m_selAlbum);
    return (a && !a->art.isEmpty()) ? QUrl::fromLocalFile(a->art).toString() : QString();
}

int App::selectedAlbumYear() const
{
    const Album *a = m_artistAlbums.at(m_selAlbum);
    return a ? a->year : 0;
}

void App::saveFolders()
{
    QSettings s;
    s.setValue(QStringLiteral("library/folders"), m_folders);
}

} // namespace hop
