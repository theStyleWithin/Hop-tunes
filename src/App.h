// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/CollectionModels.h"
#include "library/Scanner.h"
#include "library/TrackIndex.h"
#include "library/TrackListModel.h"
#include "playback/Player.h"

#include <QObject>
#include <QStringList>
#include <QUrl>

namespace hop {

// The object QML talks to: library, scanning, playback. Profiles (spec §3) arrive in
// M3; until then the watched folders live in the app settings.
class App : public QObject {
    Q_OBJECT
    Q_PROPERTY(hop::TrackListModel *songs READ songs CONSTANT)
    Q_PROPERTY(hop::Player *player READ player CONSTANT)
    Q_PROPERTY(QStringList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int scanDone READ scanDone NOTIFY scanProgressChanged)
    Q_PROPERTY(int scanTotal READ scanTotal NOTIFY scanProgressChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    // collection › music › artists (three panes) and albums
    Q_PROPERTY(hop::ArtistListModel *artists READ artists CONSTANT)
    Q_PROPERTY(hop::AlbumListModel *artistAlbums READ artistAlbums CONSTANT)
    Q_PROPERTY(hop::AlbumListModel *allAlbums READ allAlbums CONSTANT)
    Q_PROPERTY(hop::TrackListModel *albumSongs READ albumSongs CONSTANT)
    Q_PROPERTY(int selectedArtist READ selectedArtist NOTIFY selectionChanged)
    Q_PROPERTY(int selectedAlbum READ selectedAlbum NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedArtistName READ selectedArtistName NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedAlbumTitle READ selectedAlbumTitle NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedAlbumArt READ selectedAlbumArt NOTIFY selectionChanged)
    Q_PROPERTY(int selectedAlbumYear READ selectedAlbumYear NOTIFY selectionChanged)
    Q_PROPERTY(QStringList mosaicArt READ mosaicArt NOTIFY libraryChanged)   // cover URLs for now playing

public:
    explicit App(QObject *parent = nullptr);
    bool init();   // opens the index, loads the library, starts a background rescan

    TrackListModel *songs() { return &m_songs; }
    Player *player() { return &m_player; }
    QStringList folders() const { return m_folders; }
    bool scanning() const { return m_scanner.isRunning(); }
    int scanDone() const { return m_scanDone; }
    int scanTotal() const { return m_scanTotal; }
    QString version() const;
    ArtistListModel *artists() { return &m_artists; }
    AlbumListModel *artistAlbums() { return &m_artistAlbums; }
    AlbumListModel *allAlbums() { return &m_allAlbums; }
    TrackListModel *albumSongs() { return &m_albumSongs; }
    int selectedArtist() const { return m_selArtist; }
    int selectedAlbum() const { return m_selAlbum; }
    QString selectedArtistName() const;
    QString selectedAlbumTitle() const;
    QString selectedAlbumArt() const;
    int selectedAlbumYear() const;
    QStringList mosaicArt() const { return m_mosaicArt; }

    Q_INVOKABLE void selectArtist(int row);
    Q_INVOKABLE void selectAlbum(int row);          // row in artistAlbums
    Q_INVOKABLE void openAlbum(int allAlbumsRow);   // from the albums grid: select its artist + album
    Q_INVOKABLE void playAlbum(int startRow = 0);   // play the selected album

    Q_INVOKABLE void addFolder(const QUrl &folder);
    Q_INVOKABLE void removeFolder(const QString &path);
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void playSongs(int row);    // play the song list starting at row
    Q_INVOKABLE void shuffleAll();
    Q_INVOKABLE QString formatTime(qint64 ms) const { return TrackListModel::formatDuration(ms); }

signals:
    void foldersChanged();
    void scanningChanged();
    void scanProgressChanged();
    void selectionChanged();
    void libraryChanged();

private:
    void reload();
    void saveFolders();
    void applySelection(const QString &artistName, const QString &albumTitle);
    QList<Track> albumTracks(const Album &a) const;

    TrackIndex m_index;
    Scanner m_scanner{&m_index};
    TrackListModel m_songs;
    Player m_player;
    QStringList m_folders;
    int m_scanDone = 0;
    int m_scanTotal = 0;

    Collection m_collection;
    ArtistListModel m_artists;
    AlbumListModel m_artistAlbums;
    AlbumListModel m_allAlbums;
    TrackListModel m_albumSongs;
    int m_selArtist = -1;
    int m_selAlbum = -1;
    QStringList m_mosaicArt;
};

} // namespace hop
