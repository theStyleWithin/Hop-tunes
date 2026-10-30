// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/ProfileStore.h"
#include "library/CollectionModels.h"
#include "library/EpubBook.h"
#include "library/LibraryWatcher.h"
#include "library/ProfileDb.h"
#include "library/SectionIndex.h"
#include "library/SectionListModel.h"
#include "library/ShareAuth.h"
#include "library/ShareClient.h"
#include "library/ShareServer.h"
#include "library/SectionScanner.h"
#include "library/Scanner.h"
#include "library/TrackIndex.h"
#include "library/TrackListModel.h"
#include "library/VideoIndex.h"
#include "library/VideoListModel.h"
#include "library/VideoScanner.h"
#include "playback/GstVideoPlayer.h"
#include "playback/Player.h"

#include <QJsonObject>
#include <QObject>
#include <memory>
#include <QSet>
#include <QStringList>
#include <QHash>
#include <QElapsedTimer>
#include <QTimer>
#include "library/PartySync.h"
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
    Q_PROPERTY(QStringList videoCategories READ videoCategories CONSTANT)   // movies, shows, cartoons, anime, other
    Q_PROPERTY(QString videoCategory READ videoCategory WRITE setVideoCategory NOTIFY videoCategoryChanged)   // the videos tab shows this one
    Q_PROPERTY(QVariantMap videoCategoryFolders READ videoCategoryFolders NOTIFY foldersChanged)   // category -> [paths]
    // Pictures, audiobooks, ebooks and podcasts: each has its own models and folder list.
    // The extra sections that are switched on, in tab order (single source of truth for the UI).
    Q_PROPERTY(QStringList sectionKinds READ sectionKinds CONSTANT)
    Q_PROPERTY(hop::SectionListModel *pictures READ pictures CONSTANT)
    Q_PROPERTY(QVariantList pictureAlbums READ pictureAlbums NOTIFY pictureAlbumsChanged)   // one per folder: dir, name, count, cover
    Q_PROPERTY(QString pictureAlbum READ pictureAlbum NOTIFY pictureAlbumChanged)   // the open album's folder, "" = the album list
    Q_PROPERTY(hop::SectionListModel *pictureAlbumItems READ pictureAlbumItems CONSTANT)   // the pictures of the open album
    Q_PROPERTY(hop::SectionListModel *audiobooks READ audiobooks CONSTANT)
    Q_PROPERTY(hop::SectionListModel *ebooks READ ebooks CONSTANT)
    Q_PROPERTY(hop::SectionListModel *podcasts READ podcasts CONSTANT)
    // The open ebook (EPUB reader).
    Q_PROPERTY(bool bookOpen READ bookOpen NOTIFY bookChanged)
    Q_PROPERTY(QString bookTitle READ bookTitle NOTIFY bookChanged)
    Q_PROPERTY(QString bookAuthor READ bookAuthor NOTIFY bookChanged)
    Q_PROPERTY(int bookChapterIndex READ bookChapterIndex NOTIFY bookChanged)
    Q_PROPERTY(int bookChapterCount READ bookChapterCount NOTIFY bookChanged)
    Q_PROPERTY(QString bookChapterHtml READ bookChapterHtml NOTIFY bookChanged)
    Q_PROPERTY(QStringList bookChapterTitles READ bookChapterTitles NOTIFY bookChanged)
    Q_PROPERTY(double bookResumeFraction READ bookResumeFraction NOTIFY bookChanged)   // scroll position within the chapter
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
    // The details page of one video (opened by clicking a tile; nothing plays until Play is pressed).
    Q_PROPERTY(int videoDetailRow READ videoDetailRow NOTIFY videoDetailChanged)       // -1 = not open
    Q_PROPERTY(QVariantMap videoDetail READ videoDetail NOTIFY videoDetailChanged)
    // How video is shown: "window" (video with the movie's details and an up-next list), "theater" (a wide video
    // with the details below) or "fullscreen". The profile's default; the player can switch for the moment.
    Q_PROPERTY(QString videoViewStyle READ videoViewStyle WRITE setVideoViewStyle NOTIFY videoViewStyleChanged)
    // A pretend live chat beside the video: every chatter is generated, nobody is real (off by default).
    Q_PROPERTY(QString statusNote READ statusNote WRITE setStatusNote NOTIFY statusNoteChanged)   // the profile's private status box
    Q_PROPERTY(int videoVolume READ videoVolume WRITE setVideoVolume NOTIFY videoVolumeChanged)   // the movie volume, 0-150 %, separate from the music volume
    // Library sharing (the host's side; docs/design-sharing.md). Off every time Hop-tunes starts.
    Q_PROPERTY(bool sharing READ sharing WRITE setSharing NOTIFY sharingChanged)
    Q_PROPERTY(bool sharingAvailable READ sharingAvailable CONSTANT)   // false when this computer has no TLS library
    Q_PROPERTY(bool sharingInternet READ sharingInternet WRITE setSharingInternet NOTIFY sharingChanged)   // also accept connections from outside the home network (off whenever sharing starts)
    Q_PROPERTY(QString sharingPublicAddress READ sharingPublicAddress WRITE setSharingPublicAddress NOTIFY sharingChanged)   // what guests outside should use in the invite
    Q_PROPERTY(QString sharingMessage READ sharingMessage NOTIFY sharingChanged)   // why sharing did not start, or ""
    Q_PROPERTY(bool partyHosting READ partyHosting WRITE setPartyHosting NOTIFY sharingChanged)   // guests who may see the film you play follow along
    Q_PROPERTY(QVariantMap sharedParty READ sharedParty NOTIFY sharedPartyChanged)   // {active, item, title}: what the host is watching
    Q_PROPERTY(bool partyFollowing READ partyFollowing NOTIFY sharedPartyChanged)
    Q_PROPERTY(int sharingPort READ sharingPort NOTIFY sharingChanged)
    Q_PROPERTY(QStringList sharingAddresses READ sharingAddresses NOTIFY sharingChanged)   // this computer's home-network addresses
    // "Sharing mode" in the collection: tick what one guest may see. It exists only while sharing is on and there is a guest.
    Q_PROPERTY(bool canChooseShares READ canChooseShares NOTIFY shareGuestsChanged)
    Q_PROPERTY(bool shareMode READ shareMode WRITE setShareMode NOTIFY shareModeChanged)
    Q_PROPERTY(qint64 shareTarget READ shareTarget WRITE setShareTarget NOTIFY shareModeChanged)   // the guest being chosen for
    Q_PROPERTY(QString shareTargetName READ shareTargetName NOTIFY shareModeChanged)
    Q_PROPERTY(int shareRevision READ shareRevision NOTIFY shareRevisionChanged)   // changes whenever a tick changes
    Q_PROPERTY(QVariantList shareGuests READ shareGuests NOTIFY shareGuestsChanged)   // [{id, name, connected}]
    // Libraries other people shared with this profile (the guest's side)
    Q_PROPERTY(QVariantList sharedHosts READ sharedHosts NOTIFY sharedHostsChanged)   // [{id, name, address, port}]
    Q_PROPERTY(qint64 sharedHost READ sharedHost NOTIFY sharedLibraryChanged)         // the one being browsed
    Q_PROPERTY(QVariantList sharedItems READ sharedItems NOTIFY sharedLibraryChanged) // [{id, title, kind, sizeText, playable}]
    Q_PROPERTY(QString sharedStatus READ sharedStatus NOTIFY sharedLibraryChanged)    // what to tell the guest, or ""
    Q_PROPERTY(QString shareJoinMessage READ shareJoinMessage NOTIFY shareJoinChanged)
    Q_PROPERTY(bool shareJoining READ shareJoining NOTIFY shareJoinChanged)
    Q_PROPERTY(bool videoChat READ videoChat WRITE setVideoChat NOTIFY videoChatChanged)
    Q_PROPERTY(QString videoComments READ videoComments WRITE setVideoComments NOTIFY videoCommentsChanged)   // "off" | "mine" | "populated"
    Q_PROPERTY(QVariantList myVideoComments READ myVideoComments NOTIFY myVideoCommentsChanged)   // this profile's own, for the playing video
    Q_PROPERTY(QString videoChatLook READ videoChatLook WRITE setVideoChatLook NOTIFY videoChatChanged)   // "clean" | "lively"
    Q_PROPERTY(QVariantMap videoPlayerInfo READ videoPlayerInfo NOTIFY videoPlayerChanged)   // the playing video's page (as videoDetail)
    Q_PROPERTY(bool videoPlayerOpen READ videoPlayerOpen NOTIFY videoPlayerChanged)
    Q_PROPERTY(QString videoPlayerPath READ videoPlayerPath NOTIFY videoPlayerChanged)
    Q_PROPERTY(QString videoPlayerTitle READ videoPlayerTitle NOTIFY videoPlayerChanged)
    Q_PROPERTY(QString videoPlayerSource READ videoPlayerSource NOTIFY videoPlayerChanged)   // what to open: the file, or a shared film's local address
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
    QStringList videoFolders() const { return m_videoFoldersOwn ? m_videoFolders : m_folders; }   // the "movies" folders
    QStringList allVideoFolders() const;   // movies + every other category's folders: what gets scanned
    static QStringList videoCategories();
    QString videoCategory() const { return m_videoCategory; }
    void setVideoCategory(const QString &c);
    QVariantMap videoCategoryFolders() const;
    QString categoryOfVideo(const QString &path) const;   // the category whose folder is the closest parent
    SectionListModel *pictures() { return &m_sec[0]->model; }
    QVariantList pictureAlbums() const { return m_pictureAlbums; }
    QString pictureAlbum() const { return m_pictureAlbum; }
    SectionListModel *pictureAlbumItems() { return &m_albumItems; }
    SectionListModel *audiobooks() { return &m_sec[1]->model; }
    SectionListModel *ebooks() { return &m_sec[2]->model; }
    SectionListModel *podcasts() { return &m_sec[3]->model; }
    QStringList sectionKinds() const { return {QStringLiteral("pictures"), QStringLiteral("audiobooks"), QStringLiteral("ebooks"), QStringLiteral("podcasts")}; }
    bool bookOpen() const { return m_book.isOpen(); }
    QString bookTitle() const { return m_book.title(); }
    QString bookAuthor() const { return m_book.author(); }
    int bookChapterIndex() const { return m_bookChapter; }
    int bookChapterCount() const { return m_book.chapterCount(); }
    QString bookChapterHtml() { return m_book.chapterHtml(m_bookChapter); }
    QStringList bookChapterTitles();
    double bookResumeFraction() const { return m_bookResumeFraction; }
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
    int videoDetailRow() const;
    QVariantMap videoDetail() const { return buildVideoDetail(m_detailPath); }
    QVariantMap videoPlayerInfo() const { return buildVideoDetail(m_videoPlayerPath); }
    QString videoViewStyle() const { return m_videoViewStyle; }
    void setVideoViewStyle(const QString &style);
    QString statusNote() const { return m_statusNote; }
    void setStatusNote(const QString &text);
    int videoVolume() const { return m_videoVolume; }
    void setVideoVolume(int percent);
    bool sharing() const { return bool(m_share); }
    void setSharing(bool on);
    static bool sharingAvailable() { return ShareServer::encryptionAvailable(); }
    bool partyHosting() const { return m_share && m_partyHosting; }
    void setPartyHosting(bool on);
    PartyInfo partyNow() const;
    QVariantMap sharedParty() const { return m_sharedParty; }
    bool partyFollowing() const { return m_partyFollowing; }
    bool sharingInternet() const { return m_share && m_share->server.allowInternet(); }
    void setSharingInternet(bool on);
    QString sharingPublicAddress() const { return m_sharePublicAddress; }
    void setSharingPublicAddress(const QString &text);
    QString sharingMessage() const { return m_sharingMessage; }
    int sharingPort() const { return m_share ? m_share->server.port() : 0; }
    QStringList sharingAddresses() const;
    QVariantList shareGuests() const;
    bool canChooseShares() const;
    bool shareMode() const { return m_shareMode; }
    void setShareMode(bool on);
    qint64 shareTarget() const { return m_shareTarget; }
    void setShareTarget(qint64 id);
    QString shareTargetName() const;
    int shareRevision() const { return m_shareRevision; }
    QVariantList sharedHosts() const;
    qint64 sharedHost() const { return m_sharedHost; }
    QVariantList sharedItems() const { return m_sharedItems; }
    QString sharedStatus() const { return m_sharedStatus; }
    QString shareJoinMessage() const { return m_shareJoinMessage; }
    bool shareJoining() const { return m_shareJoining; }
    bool videoChat() const { return m_videoChat; }
    void setVideoChat(bool on);
    QString videoChatLook() const { return m_videoChatLook; }
    QString videoComments() const { return m_videoComments; }
    void setVideoComments(const QString &mode);
    QVariantList myVideoComments() const;
    void setVideoChatLook(const QString &look);
    QString videoPlayerPath() const { return m_videoPlayerPath; }
    QString videoPlayerTitle() const;
    QString videoPlayerSource() const { return m_videoPlayerSource.isEmpty() ? m_videoPlayerPath : m_videoPlayerSource; }
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
    Q_INVOKABLE void openPictureAlbum(const QString &dir);
    Q_INVOKABLE void closePictureAlbum();
    Q_INVOKABLE void renamePictureAlbum(const QString &dir, const QString &name);   // "" goes back to the folder's name
    Q_INVOKABLE void addSectionFolder(const QString &kind, const QUrl &folder);   // kind: pictures | audiobooks | ebooks | podcasts
    Q_INVOKABLE void removeSectionFolder(const QString &kind, const QString &path);
    // Audiobooks and podcasts: plays the item, resuming where this profile left off.
    Q_INVOKABLE void playSectionItem(const QString &kind, int row);
    Q_INVOKABLE void toggleItemPlayed(const QString &path);
    // Ebooks: EPUBs open in the built-in reader, PDFs in the default PDF viewer.
    Q_INVOKABLE void openBook(int row);
    Q_INVOKABLE void goToBookChapter(int index);
    Q_INVOKABLE void saveBookProgress(double fractionInChapter);
    Q_INVOKABLE void closeBook(double fractionInChapter);   // mark an audiobook / episode played or unplayed
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void playSongs(int row);    // play the song list starting at row
    Q_INVOKABLE void shuffleAll();
    Q_INVOKABLE QString formatTime(qint64 ms) const { return TrackListModel::formatDuration(ms); }

    // videos (spec §5b)
    Q_INVOKABLE void openVideoDetail(int row);
    Q_INVOKABLE void closeVideoDetail();
    // A one-time invite code for a new guest, valid for `hours` (between 10 minutes and a day): {code, link, hours}.
    Q_INVOKABLE QVariantMap makeShareInvite(double hours);
    Q_INVOKABLE void removeShareGuest(qint64 id);
    // Party Watch. The host's player reports where it is about once a second; a guest's player asks what to do to stay with it.
    Q_INVOKABLE void reportPlayback(qint64 positionMs, bool paused);
    Q_INVOKABLE void setSharedWatching(bool on);   // the shared tab is on screen: look for a party now and then
    Q_INVOKABLE void joinParty();
    Q_INVOKABLE QVariantMap partyAdviceFor(qint64 localMs, bool localPaused) const;
    // The guest's side: join with an invite link (hop://address:port/CODE), browse a host, play what it approved.
    Q_INVOKABLE void joinShare(const QString &link);
    Q_INVOKABLE void openSharedHost(qint64 id);
    Q_INVOKABLE void leaveSharedHost(qint64 id);
    Q_INVOKABLE QString setSharedHostAddress(qint64 id, const QString &text);   // "address:port"; "" when it was taken, else why not
    Q_INVOKABLE void playShared(const QString &itemId);
    Q_INVOKABLE QString sharedStreamUrl(const QString &itemId);   // the local address a player can read this item from ("" if not possible)
    void openRemoteVideo(const QString &key, const QString &url, const QString &title, qint64 startMs = -1);
    // For the chosen guest: 0 none of these files approved, 1 some, 2 all. Toggle approves all of them, or (when all are) takes them back.
    Q_INVOKABLE int shareState(const QStringList &paths) const;
    Q_INVOKABLE void toggleShare(const QStringList &paths);
    Q_INVOKABLE void addVideoCategoryFolder(const QString &category, const QUrl &folder);
    Q_INVOKABLE void removeVideoCategoryFolder(const QString &category, const QString &path);
    // Fields are filled in by the user, never detected: title year rating score tagline overview genre director writers studio tags.
    Q_INVOKABLE void setVideoField(const QString &key, const QString &value);
    Q_INVOKABLE void addCastMember();
    Q_INVOKABLE void setCastMember(int index, const QString &name, const QString &role);
    Q_INVOKABLE void removeCastMember(int index);
    Q_INVOKABLE QString setCastPhoto(int index, const QUrl &image);   // "" ok, else a message
    Q_INVOKABLE void toggleVideoWatched();
    Q_INVOKABLE QString setVideoThumb(int row, const QUrl &image);   // "" ok, else a message
    Q_INVOKABLE void clearVideoThumb(int row);                       // back to the detected frame
    Q_INVOKABLE void playVideo(int row);                                  // opens the player, resumes if we have a position
    Q_INVOKABLE void closeVideoPlayer(qint64 positionMs, qint64 durationMs);
    Q_INVOKABLE void switchVideo(int row, qint64 positionMs, qint64 durationMs);   // from the up-next list: saves this one, opens that  // saves resume position, closes
    Q_INVOKABLE void addVideoComment(const QString &text);   // under the playing video
    Q_INVOKABLE void removeVideoComment(qint64 id);
    Q_INVOKABLE void saveVideoProgress(qint64 positionMs, qint64 durationMs); // periodic save while playing

signals:
    void foldersChanged();
    void pictureAlbumsChanged();
    void pictureAlbumChanged();
    void videoCategoryChanged();
    void sectionFoldersChanged();
    void sectionStatusChanged();
    void bookChanged();
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
    void videoDetailChanged();
    void videoViewStyleChanged();
    void videoChatChanged();
    void sharingChanged();
    void shareGuestsChanged();
    void sharedHostsChanged();
    void sharedLibraryChanged();
    void sharedPartyChanged();
    void shareJoinChanged();
    void shareModeChanged();
    void shareRevisionChanged();
    void statusNoteChanged();
    void videoVolumeChanged();
    void videoCommentsChanged();
    void myVideoCommentsChanged();

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
    QVariantMap buildVideoDetail(const QString &path) const;
    QString m_videoViewStyle = QStringLiteral("window");
    QString m_videoComments = QStringLiteral("off");
    // Sharing exists only while it is switched on: nothing listens otherwise.
    struct Share {
        Share(ProfileDb &db, const QString &host) : auth(db), lib(db, auth, host), server(db, auth, lib) {}
        ShareAuth auth;
        GrantLibrary lib;
        ShareServer server;
    };
    ShareProxy m_shareProxy;
    qint64 m_sharedHost = 0;
    QVariantList m_sharedItems;
    QString m_sharedStatus;
    QString m_shareJoinMessage;
    bool m_shareJoining = false;
    bool m_partyHosting = false;
    qint64 m_partyPos = 0;
    bool m_partyPaused = false;
    QString m_partyPath;
    QElapsedTimer m_partyClock;
    QVariantMap m_sharedParty;
    bool m_partyFollowing = false, m_sharedWatching = false;
    qint64 m_partyTargetMs = 0;
    bool m_partyTargetPaused = false;
    QElapsedTimer m_partyTargetClock;
    QString m_partyItem;
    int m_partyMisses = 0;
    QTimer m_partyTimer;
    void pollParty();
    void updatePartyTimer();
    int m_shareGen = 0;   // bumped when the profile closes, so a late reply from a host is ignored
    QString m_sharePublicAddress;   // saved per profile (a name or number only the host knows)
    QString m_sharingMessage;
    std::unique_ptr<Share> m_share;
    bool m_shareMode = false;
    qint64 m_shareTarget = 0;
    int m_shareRevision = 0;
    void keepShareModeValid();   // sharing off or no guests: leave the mode; the chosen guest is removed: pick another
    QString m_statusNote;
    int m_videoVolume = 100;
    bool m_videoChat = false;
    QString m_videoChatLook = QStringLiteral("clean");
    QString m_detailPath;                              // the video whose details page is open
    QHash<QString, QJsonObject> m_videoMeta;           // what this profile filled in per video, by path
    void saveVideoMeta(const QString &path);
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
    QHash<QString, QStringList> m_videoCatFolders;   // shows | cartoons | anime | other -> folders
    QString m_videoCategory = QStringLiteral("movies");
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

    QHash<QString, QString> m_albumNames;   // folder -> the name this profile gave its album
    QVariantList m_pictureAlbums;
    QString m_pictureAlbum;
    SectionListModel m_albumItems{SectionKind::Pictures};
    void rebuildPictureAlbums(const QList<SectionItem> &items);
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
    EpubBook m_book;
    QString m_bookPath;
    int m_bookChapter = 0;
    double m_bookResumeFraction = 0.0;
    QSet<QString> m_played;   // this profile's finished audiobooks and episodes
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
    QString m_videoPlayerPath;          // identity of what is playing (a file path, or share:<host>:<item> for a shared film)
    QString m_videoPlayerSource;        // set only for a shared film: the local address the player reads
    QString m_videoPlayerRemoteTitle;
    qint64 m_videoPlayerResumeMs = 0;
};

} // namespace hop
