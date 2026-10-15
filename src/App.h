// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/ProfileStore.h"
#include "library/CollectionModels.h"
#include "library/Scanner.h"
#include "library/TrackIndex.h"
#include "library/TrackListModel.h"
#include "playback/Player.h"

#include <QObject>
#include <QStringList>
#include <QHash>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace hop {

// The object QML talks to: profiles, library, scanning, playback. Nothing loads until a
// profile (or guest mode) is open; each profile has its own watched folders and settings
// over the one shared track index (spec §3).
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
    // profiles (spec §3)
    Q_PROPERTY(bool profileReady READ profileReady NOTIFY profileChanged)
    Q_PROPERTY(bool guest READ guest NOTIFY profileChanged)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY profileChanged)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QString accent READ accent NOTIFY profileChanged)
    Q_PROPERTY(QStringList accents READ accents CONSTANT)
    Q_PROPERTY(QString suggestedFolder READ suggestedFolder CONSTANT)
    Q_PROPERTY(QUrl suggestedFolderUrl READ suggestedFolderUrl CONSTANT)

public:
    explicit App(QObject *parent = nullptr);
    ~App() override;
    bool init();   // opens the index and the profile list; opens a profile if that needs no choice

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

    bool profileReady() const { return m_guest || !m_profileId.isEmpty(); }
    bool guest() const { return m_guest; }
    QVariantMap profile() const;
    QVariantList profiles() const;
    QString accent() const;
    QStringList accents() const;
    QString suggestedFolder() const;
    QUrl suggestedFolderUrl() const { const QString f = suggestedFolder(); return f.isEmpty() ? QUrl() : QUrl::fromLocalFile(f); }
    ProfileStore *profileStore() { return &m_store; }

    // Each returns "" on success or a short message for the UI.
    Q_INVOKABLE QString createProfile(const QString &name, const QString &accent, const QString &pin, const QUrl &folder);
    Q_INVOKABLE QString openProfile(const QString &id, const QString &pin = {});
    Q_INVOKABLE bool profileNeedsPin(const QString &id) const;
    Q_INVOKABLE void openGuest();
    Q_INVOKABLE void leaveProfile();                        // stop playback, back to the picker
    Q_INVOKABLE QString renameProfile(const QString &name);
    Q_INVOKABLE QString setProfileAccent(const QString &accent);
    Q_INVOKABLE QString setProfilePin(const QString &currentPin, const QString &newPin);   // newPin "" removes
    Q_INVOKABLE QString setProfileAvatar(const QUrl &image);    // empty url removes
    Q_INVOKABLE QString removeProfile();                    // the open profile; never touches music

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
    void profileChanged();
    void profilesChanged();

private:
    void reload();
    void saveFolders();
    void applySelection(const QString &artistName, const QString &albumTitle);
    QList<Track> albumTracks(const Album &a) const;
    void activate(const QString &id, bool guest);
    void saveProfileSettings();
    QVariantMap profileMap(const Profile &p) const;

    ProfileStore m_store;
    QString m_profileId;          // empty when no profile is open
    bool m_guest = false;
    QStringList m_legacyFolders;  // pre-profile (M1/M2) setting, adopted by the first profile
    struct PinLock { int fails = 0; qint64 until = 0; };
    QHash<QString, PinLock> m_pinLocks;

    TrackIndex m_index;
    Scanner m_scanner{&m_index};
    TrackListModel m_songs;
    Player m_player;
    QStringList m_folders;
    int m_scanDone = 0;
    int m_scanTotal = 0;
    bool m_rescanPending = false;

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
