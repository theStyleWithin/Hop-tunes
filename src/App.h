// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/ProfileStore.h"
#include "library/CollectionModels.h"
#include "library/LibraryWatcher.h"
#include "library/ProfileDb.h"
#include "library/SectionIndex.h"
#include "library/SectionListModel.h"
#include "library/SectionScanner.h"
#include "library/Scanner.h"
#include "library/TrackIndex.h"
#include "library/TrackListModel.h"
#include "library/VideoIndex.h"
#include "library/VideoListModel.h"
#include "library/VideoScanner.h"
#include "playback/GstVideoPlayer.h"
#include "playback/Player.h"

#include <QObject>
#include <memory>
#include <QStringList>
#include <QHash>
#include <QTimer>
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
    // Folders the videos tab scans: its own list once the profile has set one, else the shared media folders.
    Q_PROPERTY(QStringList videoFolders READ videoFolders NOTIFY foldersChanged)
    // Pictures, audiobooks, ebooks and podcasts: each has its own models and folder list.
    // The extra sections that are switched on, in tab order (single source of truth for the UI).
    Q_PROPERTY(QStringList sectionKinds READ sectionKinds CONSTANT)
    Q_PROPERTY(hop::SectionListModel *pictures READ pictures CONSTANT)
    Q_PROPERTY(hop::SectionListModel *audiobooks READ audiobooks CONSTANT)
    Q_PROPERTY(hop::SectionListModel *ebooks READ ebooks CONSTANT)
    Q_PROPERTY(hop::SectionListModel *podcasts READ podcasts CONSTANT)
    Q_PROPERTY(QVariantMap sectionFolders READ sectionFolders NOTIFY sectionFoldersChanged)   // key -> [paths]
    Q_PROPERTY(QVariantMap sectionStatus READ sectionStatus NOTIFY sectionStatusChanged)      // key -> {scanning, done, total}
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
    // search (spec §4.2)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchChanged)
    Q_PROPERTY(int librarySize READ librarySize NOTIFY libraryChanged)
    // playlists (spec §7)
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY playlistsChanged)
    Q_PROPERTY(QString selectedPlaylist READ selectedPlaylist NOTIFY playlistSelectionChanged)
    Q_PROPERTY(QVariantMap selectedPlaylistInfo READ selectedPlaylistInfo NOTIFY playlistSelectionChanged)
    Q_PROPERTY(hop::TrackListModel *playlistSongs READ playlistSongs CONSTANT)
    // quickplay (spec §4.4)
    Q_PROPERTY(QVariantList quickplayPinned READ quickplayPinned NOTIFY quickplayChanged)
    Q_PROPERTY(QVariantList quickplayHistory READ quickplayHistory NOTIFY quickplayChanged)
    Q_PROPERTY(QVariantList quickplayNew READ quickplayNew NOTIFY quickplayChanged)
    Q_PROPERTY(QStringList djArtists READ djArtists NOTIFY libraryChanged)
    Q_PROPERTY(QString djSeed READ djSeed NOTIFY djChanged)
    Q_PROPERTY(bool currentHearted READ currentHearted NOTIFY statsChanged)
    // appearance (spec §4.5), per profile
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY appearanceChanged)        // dark | light | system
    Q_PROPERTY(bool lightTheme READ lightTheme NOTIFY appearanceChanged)
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY appearanceChanged)
    Q_PROPERTY(bool systemThemeAvailable READ systemThemeAvailable CONSTANT)
    // videos (spec §5b)
    Q_PROPERTY(hop::VideoListModel *videos READ videos CONSTANT)
    Q_PROPERTY(bool videoScanning READ videoScanning NOTIFY videoScanProgressChanged)
    Q_PROPERTY(int videoScanDone READ videoScanDone NOTIFY videoScanProgressChanged)
    Q_PROPERTY(int videoScanTotal READ videoScanTotal NOTIFY videoScanProgressChanged)
    Q_PROPERTY(bool videoPlayerOpen READ videoPlayerOpen NOTIFY videoPlayerChanged)
    Q_PROPERTY(QString videoPlayerPath READ videoPlayerPath NOTIFY videoPlayerChanged)
    Q_PROPERTY(QString videoPlayerTitle READ videoPlayerTitle NOTIFY videoPlayerChanged)
    Q_PROPERTY(qint64 videoPlayerResumeMs READ videoPlayerResumeMs NOTIFY videoPlayerChanged)
    // "mpv" | "gstreamer" | "none" — which engine VideoPlayerView.qml should instantiate.
    Q_PROPERTY(QString videoEngine READ videoEngine CONSTANT)
    Q_PROPERTY(hop::GstVideoPlayer *gstVideo READ gstVideo CONSTANT)

public:
    explicit App(QObject *parent = nullptr);
    ~App() override;
    bool init();   // opens the index and the profile list; opens a profile if that needs no choice

    TrackListModel *songs() { return &m_songs; }
    Player *player() { return &m_player; }
    QStringList folders() const { return m_folders; }
    QStringList videoFolders() const { return m_videoFoldersOwn ? m_videoFolders : m_folders; }
    SectionListModel *pictures() { return &m_sec[0]->model; }
    SectionListModel *audiobooks() { return &m_sec[1]->model; }
    SectionListModel *ebooks() { return &m_sec[2]->model; }
    SectionListModel *podcasts() { return &m_sec[3]->model; }
    QStringList sectionKinds() const { return {QStringLiteral("pictures"), QStringLiteral("audiobooks")}; }
    QVariantMap sectionFolders() const;
    QVariantMap sectionStatus() const;
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
    ProfileDb *profileDb() { return &m_db; }

    QString searchText() const { return m_search; }
    void setSearchText(const QString &text);
    int librarySize() const { return int(m_library.size()); }
    QVariantList playlists() const;
    QString selectedPlaylist() const { return m_selPlaylist; }
    QVariantMap selectedPlaylistInfo() const;
    TrackListModel *playlistSongs() { return &m_playlistSongs; }
    QVariantList quickplayPinned() const { return m_qpPinned; }
    QVariantList quickplayHistory() const { return m_qpHistory; }
    QVariantList quickplayNew() const { return m_qpNew; }
    QStringList djArtists() const;
    QString djSeed() const { return m_djSeed; }
    bool currentHearted() const;
    QString theme() const { return m_theme; }
    void setTheme(const QString &theme);
    bool lightTheme() const;
    bool reduceMotion() const { return m_reduceMotion; }
    void setReduceMotion(bool on);
    bool systemThemeAvailable() const;

    VideoListModel *videos() { return &m_videos; }
    bool videoScanning() const { return m_videoScanner.isRunning(); }
    int videoScanDone() const { return m_videoScanDone; }
    int videoScanTotal() const { return m_videoScanTotal; }
    bool videoPlayerOpen() const { return m_videoPlayerOpen; }
    QString videoPlayerPath() const { return m_videoPlayerPath; }
    QString videoPlayerTitle() const;
    qint64 videoPlayerResumeMs() const { return m_videoPlayerResumeMs; }
    QString videoEngine() const;
    GstVideoPlayer *gstVideo() { return &m_gstVideo; }

    // playlists: keys are "pl:<id>" for the profile's own and "auto:<name>" for built-ins.
    Q_INVOKABLE void selectPlaylist(const QString &key);
    Q_INVOKABLE void playPlaylist(int startRow = 0);
    Q_INVOKABLE QString createPlaylist(const QString &name, const QStringList &paths = {});   // returns key
    Q_INVOKABLE QString createSmartPlaylist(const QString &name, const QString &rulesJson);
    Q_INVOKABLE bool renamePlaylist(const QString &key, const QString &name);
    Q_INVOKABLE bool setSmartRules(const QString &key, const QString &rulesJson);
    Q_INVOKABLE bool deletePlaylist(const QString &key);
    Q_INVOKABLE bool addToPlaylist(const QString &key, const QStringList &paths);
    Q_INVOKABLE bool removeFromPlaylist(int row);                     // in the selected playlist
    Q_INVOKABLE bool movePlaylistItem(int from, int to);
    Q_INVOKABLE QString importPlaylist(const QUrl &file);             // "" or a message
    Q_INVOKABLE QString exportPlaylist(const QString &key, const QUrl &file);
    Q_INVOKABLE int smartPreviewCount(const QString &rulesJson) const;
    Q_INVOKABLE QStringList albumPaths(int allAlbumsRow) const;       // for "add album to playlist"
    Q_INVOKABLE QStringList selectedAlbumPaths() const;

    // Per-profile manual edits to an album's shown title/cover (never seen by another
    // profile) and a manual track order for the currently displayed album song list.
    Q_INVOKABLE QString setAlbumArt(const QString &ref, const QUrl &image);     // "" ok, else a message
    Q_INVOKABLE QString setAlbumTitle(const QString &ref, const QString &title); // "" ok, else a message
    Q_INVOKABLE bool moveAlbumSong(int from, int to);   // reorders the visible albumSongs list

    // A manual order for the whole-library "songs" tab, same per-profile privacy as above.
    Q_INVOKABLE bool moveSong(int from, int to);        // reorders the visible songs list

    // queue + hearts
    Q_INVOKABLE void playNextPaths(const QStringList &paths);
    Q_INVOKABLE void enqueuePaths(const QStringList &paths);
    Q_INVOKABLE void toggleHeart(const QString &path);
    Q_INVOKABLE void toggleHeartCurrent();

    // quickplay
    Q_INVOKABLE void pinTile(const QString &kind, const QString &ref, const QString &label);
    Q_INVOKABLE void unpinTile(const QString &kind, const QString &ref);
    Q_INVOKABLE bool isPinned(const QString &kind, const QString &ref) const;
    Q_INVOKABLE QString albumRef(int allAlbumsRow) const;
    Q_INVOKABLE QString selectedAlbumRef() const;
    Q_INVOKABLE void playTile(const QString &kind, const QString &ref);
    Q_INVOKABLE bool showAlbum(const QString &ref);                   // select it in collection › artists
    Q_INVOKABLE void startDj(const QString &artist);

    // Files or folders from the command line, a file manager or MPRIS OpenUri: they play
    // right away (they need not be in the library). Held until a profile is open.
    Q_INVOKABLE void openFiles(const QStringList &pathsOrUris);
    void openUri(const QString &uri);   // batches several OpenUri calls into one queue

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
    Q_INVOKABLE void addVideoFolder(const QUrl &folder);
    Q_INVOKABLE void removeVideoFolder(const QString &path);
    Q_INVOKABLE void addSectionFolder(const QString &kind, const QUrl &folder);   // kind: pictures | audiobooks | ebooks | podcasts
    Q_INVOKABLE void removeSectionFolder(const QString &kind, const QString &path);
    // Audiobooks and podcasts: plays the item, resuming where this profile left off.
    Q_INVOKABLE void playSectionItem(const QString &kind, int row);
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void playSongs(int row);    // play the song list starting at row
    Q_INVOKABLE void shuffleAll();
    Q_INVOKABLE QString formatTime(qint64 ms) const { return TrackListModel::formatDuration(ms); }

    // videos (spec §5b)
    Q_INVOKABLE QString setVideoThumb(int row, const QUrl &image);   // "" ok, else a message
    Q_INVOKABLE void clearVideoThumb(int row);                       // back to the detected frame
    Q_INVOKABLE void playVideo(int row);                                  // opens the player, resumes if we have a position
    Q_INVOKABLE void closeVideoPlayer(qint64 positionMs, qint64 durationMs);  // saves resume position, closes
    Q_INVOKABLE void saveVideoProgress(qint64 positionMs, qint64 durationMs); // periodic save while playing

signals:
    void foldersChanged();
    void sectionFoldersChanged();
    void sectionStatusChanged();
    void scanningChanged();
    void scanProgressChanged();
    void selectionChanged();
    void libraryChanged();
    void profileChanged();
    void profilesChanged();
    void searchChanged();
    void playlistsChanged();
    void playlistSelectionChanged();
    void quickplayChanged();
    void djChanged();
    void statsChanged();
    void appearanceChanged();
    void videoScanProgressChanged();
    void videoPlayerChanged();

private:
    void reload();
    void reloadVideos();
    void reloadSections();
    void reloadSection(int i);
    int sectionIndexOf(const QString &kind) const;
    void openVideoFile(const QString &path);
    void saveFolders();
    void applySelection(const QString &artistName, const QString &albumTitle);
    QList<Track> albumTracks(const Album &a) const;
    void applyAlbumOverrides(QList<Album> &albums) const;
    void activate(const QString &id, bool guest);
    static void setAside(const QString &file);
    void applySearch();
    void refreshPlaylist();
    void refreshQuickplay();
    QList<Track> tracksForPaths(const QStringList &paths) const;
    QList<Track> playlistTracks(const QString &key) const;
    QList<Track> tracksForAlbumRef(const QString &ref) const;
    QVariantMap tile(const QString &kind, const QString &ref, const QString &label) const;
    void onPosition();
    void saveResume(bool force);
    void flushResume();
    void onTrackChanged();
    void djTopUp();
    void saveProfileSettings();
    QVariantMap profileMap(const Profile &p) const;

    ProfileStore m_store;
    ProfileDb m_db;
    QHash<QString, TrackStats> m_stats;
    QList<Track> m_library;                 // everything under the profile's folders
    QHash<QString, int> m_libraryIndex;     // path -> row in m_library
    Collection m_fullCollection;            // unfiltered, for quickplay
    QHash<QString, QString> m_videoThumbOverrides;   // this profile's picked video thumbnails, by video path
    QHash<QString, AlbumOverride> m_albumOverrides;   // this profile's title/cover edits, by ref
    QStringList m_songOrder;                // this profile's manual order for the songs tab
    QString m_search;
    QString m_selPlaylist;
    TrackListModel m_playlistSongs;
    QVariantList m_qpPinned, m_qpHistory, m_qpNew;
    QString m_djSeed;
    QString m_theme = QStringLiteral("dark");
    bool m_reduceMotion = false;
    bool m_loadingSettings = false;
    QStringList m_pendingOpen;
    QStringList m_openBatch;
    QTimer m_openTimer;
    quint32 m_djRound = 0;
    struct Listen { QString path; qint64 maxPos = 0; qint64 duration = 0; bool counted = false; } m_listen;
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
    QStringList m_videoFolders;       // used only when m_videoFoldersOwn
    bool m_videoFoldersOwn = false;   // this profile has set its own video folders
    QStringList watchedFolders() const;   // music + video folders, deduplicated
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

    struct SectionState {
        SectionState(SectionKind k, SectionIndex *idx) : kind(k), model(k), scanner(k, idx) {}
        SectionKind kind;
        SectionListModel model;
        SectionScanner scanner;
        QStringList folders;
        int done = 0;
        int total = 0;
        bool rescanPending = false;
    };
    // The audiobook / podcast currently playing, whose position is saved as it plays.
    QString m_resumePath;
    qint64 m_resumeSavedMs = 0;
    qint64 m_resumeLastMs = 0;
    qint64 m_resumeDurationMs = 0;
    SectionIndex m_sectionIndex;
    std::unique_ptr<SectionState> m_sec[4];

    VideoIndex m_videoIndex;
    VideoScanner m_videoScanner{&m_videoIndex};
    VideoListModel m_videos;
    GstVideoPlayer m_gstVideo;
    LibraryWatcher m_watcher;
    int m_videoScanDone = 0;
    int m_videoScanTotal = 0;
    bool m_videoScanRescanPending = false;
    bool m_videoPlayerOpen = false;
    QString m_videoPlayerPath;
    qint64 m_videoPlayerResumeMs = 0;
};

} // namespace hop
