// SPDX-License-Identifier: GPL-3.0-or-later
#include "App.h"

#include "core/Paths.h"
#include "library/Smart.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QImage>
#include <QJsonArray>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace hop {

App::App(QObject *parent) : QObject(parent), m_store(Paths::profilesDir())
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
        if (m_rescanPending) { m_rescanPending = false; rescan(); }
    });
    for (TrackListModel *m : {&m_songs, &m_albumSongs, &m_playlistSongs}) m->setStats(&m_stats);
    connect(&m_player, &Player::positionChanged, this, &App::onPosition);
    connect(&m_player, &Player::currentChanged, this, &App::onTrackChanged);
}

App::~App()
{
    saveProfileSettings();
    m_scanner.cancel();
}

bool App::init()
{
    Paths::ensureDirs();
    if (!m_index.open(Paths::indexDbPath())) {
        qWarning("hop-tunes: cannot open %s: %s", qPrintable(Paths::indexDbPath()), qPrintable(m_index.lastError()));
        return false;
    }
    if (!m_store.load()) {
        // Never overwrite an unreadable profile list: set it aside and start fresh.
        const QString f = m_store.rootDir() + QStringLiteral("/profiles.json");
        const QString aside = f + QStringLiteral(".unreadable-") + QString::number(QDateTime::currentSecsSinceEpoch());
        qWarning("hop-tunes: profiles.json unreadable (%s); moved to %s", qPrintable(m_store.lastError()), qPrintable(aside));
        QFile::rename(f, aside);
        m_store.load();
    }
    m_legacyFolders = QSettings().value(QStringLiteral("library/folders")).toStringList();

    // Developer hook: HOPTUNES_PROFILE=<name> opens that profile (created if missing, PIN
    // profiles are never bypassed); HOPTUNES_PROFILE=:guest opens guest mode.
    const QString want = QString::fromLocal8Bit(qgetenv("HOPTUNES_PROFILE"));
    if (want == QLatin1String(":guest")) {
        openGuest();
    } else if (!want.isEmpty()) {
        QString id;
        for (const Profile &p : m_store.profiles())
            if (p.name == want) id = p.id;
        if (id.isEmpty()) createProfile(want, accents().value(0), {}, QUrl::fromLocalFile(suggestedFolder()));
        else if (!profileNeedsPin(id)) openProfile(id);
    } else if (m_store.count() == 1 && !m_store.profiles().constFirst().hasPin) {
        openProfile(m_store.profiles().constFirst().id);   // one profile, no PIN: no picker
    }
    return true;
}

// ---- profiles ---------------------------------------------------------------------

QStringList App::accents() const
{
    return {QStringLiteral("#4fd1c5"), QStringLiteral("#ff6b9a"), QStringLiteral("#ff8a4c"),
            QStringLiteral("#f2c94c"), QStringLiteral("#7cc4ff"), QStringLiteral("#b69cff")};
}

QString App::suggestedFolder() const
{
    if (!m_legacyFolders.isEmpty()) return m_legacyFolders.constFirst();
    const QString music = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    return QFileInfo(music).isDir() ? QFileInfo(music).canonicalFilePath() : QString();
}

QVariantMap App::profileMap(const Profile &p) const
{
    QString avatar;
    if (p.hasAvatar) {
        // Cache-bust so QML reloads a changed picture.
        QUrl u = QUrl::fromLocalFile(m_store.avatarPath(p.id));
        u.setQuery(QStringLiteral("v=") + QString::number(QFileInfo(m_store.avatarPath(p.id)).lastModified().toMSecsSinceEpoch()));
        avatar = u.toString();
    }
    return {{QStringLiteral("id"), p.id},
            {QStringLiteral("name"), p.name},
            {QStringLiteral("initial"), p.name.left(1).toUpper()},
            {QStringLiteral("accent"), p.accent},
            {QStringLiteral("avatar"), avatar},
            {QStringLiteral("hasPin"), p.hasPin}};
}

QVariantMap App::profile() const
{
    if (m_guest)
        return {{QStringLiteral("id"), QString()}, {QStringLiteral("name"), QStringLiteral("guest")},
                {QStringLiteral("initial"), QStringLiteral("G")}, {QStringLiteral("accent"), accents().value(0)},
                {QStringLiteral("avatar"), QString()}, {QStringLiteral("hasPin"), false}};
    const Profile *p = m_store.find(m_profileId);
    return p ? profileMap(*p) : QVariantMap{};
}

QVariantList App::profiles() const
{
    QVariantList out;
    for (const Profile &p : m_store.profiles()) out.append(profileMap(p));
    return out;
}

QString App::accent() const
{
    const Profile *p = m_store.find(m_profileId);
    return p ? p->accent : accents().value(0);
}

bool App::profileNeedsPin(const QString &id) const
{
    const Profile *p = m_store.find(id);
    return p && p->hasPin;
}

QString App::createProfile(const QString &name, const QString &accent, const QString &pin, const QUrl &folder)
{
    if (!ProfileStore::isValidName(name)) return QStringLiteral("give the profile a name (up to 40 characters)");
    for (const Profile &p : m_store.profiles())
        if (p.name.compare(name.trimmed(), Qt::CaseInsensitive) == 0) return QStringLiteral("there is already a profile with that name");
    if (!pin.isEmpty() && !ProfileStore::isValidPin(pin)) return QStringLiteral("a PIN is 4 to 8 digits");
    const bool first = m_store.count() == 0;
    const QString id = m_store.create(name, accent, pin);
    if (id.isEmpty()) return m_store.lastError();

    QStringList folders;
    if (first) folders = m_legacyFolders;   // adopt the folders from before profiles existed
    const QString chosen = folder.isEmpty() ? QString()
        : QFileInfo(folder.isLocalFile() ? folder.toLocalFile() : folder.toString()).canonicalFilePath();
    if (!chosen.isEmpty() && !folders.contains(chosen)) folders.prepend(chosen);
    m_store.saveSettings(id, {{QStringLiteral("folders"), QJsonArray::fromStringList(folders)}});
    if (first && !m_legacyFolders.isEmpty()) QSettings().remove(QStringLiteral("library"));
    emit profilesChanged();
    activate(id, false);
    return {};
}

QString App::openProfile(const QString &id, const QString &pin)
{
    const Profile *p = m_store.find(id);
    if (!p) return QStringLiteral("that profile no longer exists");
    if (p->hasPin) {
        PinLock &lock = m_pinLocks[id];
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (lock.until > now) return QStringLiteral("too many tries. wait %1 s").arg((lock.until - now + 999) / 1000);
        if (!m_store.checkPin(id, pin)) {
            if (++lock.fails >= 5) { lock.fails = 0; lock.until = now + 30'000; }
            return QStringLiteral("wrong PIN");
        }
        lock = {};
    }
    activate(id, false);
    return {};
}

void App::openGuest() { activate({}, true); }

void App::activate(const QString &id, bool guest)
{
    leaveProfile();
    m_profileId = id;
    m_guest = guest;
    const QJsonObject s = guest ? QJsonObject{} : m_store.settings(id);
    m_folders.clear();
    if (guest) {
        if (!suggestedFolder().isEmpty()) m_folders << suggestedFolder();
    } else {
        for (const QJsonValue &v : s.value(QStringLiteral("folders")).toArray()) m_folders << v.toString();
    }
    m_player.setVolume(s.value(QStringLiteral("volume")).toDouble(1.0));
    if (!m_db.open(guest ? QStringLiteral(":memory:") : m_store.dirFor(id) + QStringLiteral("/profile.sqlite")))
        qWarning("hop-tunes: cannot open profile database: %s", qPrintable(m_db.lastError()));
    m_stats = m_db.allStats();
    m_search.clear();
    m_djSeed.clear();
    m_selPlaylist = QStringLiteral("auto:hearted");
    emit searchChanged();
    emit djChanged();
    emit playlistsChanged();
    emit statsChanged();
    emit profileChanged();
    emit foldersChanged();
    reload();
    rescan();
}

void App::leaveProfile()
{
    if (!profileReady()) return;
    saveProfileSettings();
    m_listen = {};                // plays are counted as they happen; leaving is not a skip
    m_player.setQueue({}, 0);
    m_listen = {};
    m_scanner.cancel();
    m_db.close();
    m_stats.clear();
    m_search.clear();
    m_selPlaylist.clear();
    m_djSeed.clear();
    m_profileId.clear();
    m_guest = false;
    m_folders.clear();
    reload();
    emit foldersChanged();
    emit profileChanged();
}

void App::saveProfileSettings()
{
    if (m_profileId.isEmpty()) return;   // guest mode keeps nothing
    QJsonObject s = m_store.settings(m_profileId);
    s.insert(QStringLiteral("folders"), QJsonArray::fromStringList(m_folders));
    s.insert(QStringLiteral("volume"), m_player.volume());
    m_store.saveSettings(m_profileId, s);
}

QString App::renameProfile(const QString &name)
{
    if (m_profileId.isEmpty()) return QStringLiteral("no profile open");
    if (!ProfileStore::isValidName(name)) return QStringLiteral("give the profile a name (up to 40 characters)");
    for (const Profile &p : m_store.profiles())
        if (p.id != m_profileId && p.name.compare(name.trimmed(), Qt::CaseInsensitive) == 0)
            return QStringLiteral("there is already a profile with that name");
    if (!m_store.rename(m_profileId, name)) return m_store.lastError();
    emit profileChanged();
    emit profilesChanged();
    return {};
}

QString App::setProfileAccent(const QString &accent)
{
    if (m_profileId.isEmpty() || !m_store.setAccent(m_profileId, accent)) return QStringLiteral("could not change the colour");
    emit profileChanged();
    emit profilesChanged();
    return {};
}

QString App::setProfilePin(const QString &currentPin, const QString &newPin)
{
    if (m_profileId.isEmpty()) return QStringLiteral("no profile open");
    if (profileNeedsPin(m_profileId) && !m_store.checkPin(m_profileId, currentPin)) return QStringLiteral("current PIN is wrong");
    if (!newPin.isEmpty() && !ProfileStore::isValidPin(newPin)) return QStringLiteral("a PIN is 4 to 8 digits");
    if (!m_store.setPin(m_profileId, newPin)) return m_store.lastError();
    emit profileChanged();
    emit profilesChanged();
    return {};
}

QString App::setProfileAvatar(const QUrl &image)
{
    if (m_profileId.isEmpty()) return QStringLiteral("no profile open");
    if (image.isEmpty()) {
        m_store.setHasAvatar(m_profileId, false);
    } else {
        QImage img(image.isLocalFile() ? image.toLocalFile() : image.toString());
        if (img.isNull()) return QStringLiteral("that file is not a picture hop-tunes can read");
        const int side = qMin(img.width(), img.height());
        img = img.copy((img.width() - side) / 2, (img.height() - side) / 2, side, side)
                 .scaled(256, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (!img.save(m_store.avatarPath(m_profileId), "PNG")) return QStringLiteral("could not save the picture");
        QFile::setPermissions(m_store.avatarPath(m_profileId), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        m_store.setHasAvatar(m_profileId, true);
    }
    emit profileChanged();
    emit profilesChanged();
    return {};
}

QString App::removeProfile()
{
    if (m_profileId.isEmpty()) return QStringLiteral("no profile open");
    const QString id = m_profileId;
    m_listen = {};
    m_player.setQueue({}, 0);
    m_listen = {};
    m_scanner.cancel();
    m_db.close();
    m_stats.clear();
    m_profileId.clear();          // nothing is written back for a profile being removed
    m_folders.clear();
    reload();
    emit foldersChanged();
    const bool ok = m_store.remove(id);
    emit profilesChanged();
    emit profileChanged();
    return ok ? QString() : m_store.lastError();
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
    if (m_scanner.isRunning()) { m_rescanPending = true; return; }   // e.g. right after a profile switch
    m_scanDone = m_scanTotal = 0;
    emit scanProgressChanged();
    m_scanner.start(m_folders);
}

void App::playSongs(int row)
{
    if (row < 0 || row >= m_songs.count()) return;
    m_djSeed.clear(); emit djChanged();
    m_player.setQueue(m_songs.tracks(), row);
}

void App::shuffleAll()
{
    QList<Track> q = m_songs.tracks();
    if (q.isEmpty()) return;
    std::shuffle(q.begin(), q.end(), *QRandomGenerator::global());
    m_djSeed.clear(); emit djChanged();
    m_player.setQueue(q, 0);
}

void App::reload()
{
    m_library = m_index.tracksUnder(m_folders);
    m_libraryIndex.clear();
    for (int i = 0; i < m_library.size(); ++i) m_libraryIndex.insert(m_library[i].path, i);
    m_fullCollection.build(m_library);
    m_mosaicArt.clear();
    for (const Album &a : m_fullCollection.albums())
        if (!a.art.isEmpty()) m_mosaicArt << QUrl::fromLocalFile(a.art).toString();
    applySearch();
    refreshPlaylist();
    refreshQuickplay();
}

void App::applySearch()
{
    const QString keepArtist = selectedArtistName(), keepAlbum = selectedAlbumTitle();
    const QStringList tokens = searchTokens(m_search);
    if (tokens.isEmpty()) {
        m_songs.setTracks(m_library);
    } else {
        QList<Track> hits;
        for (const Track &t : m_library)
            if (matchesSearch(t, tokens)) hits.append(t);
        m_songs.setTracks(hits);
    }
    m_collection.build(m_songs.tracks());
    m_artists.setArtists(m_collection.artists());
    QList<Album> all;
    for (const Artist &ar : m_collection.artists())
        for (int i : ar.albumIndices) all.append(m_collection.albums()[i]);
    m_allAlbums.setAlbums(all);
    emit libraryChanged();
    applySelection(keepArtist, keepAlbum);
}

void App::setSearchText(const QString &text)
{
    if (text == m_search) return;
    m_search = text;
    emit searchChanged();
    applySearch();
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
    m_djSeed.clear(); emit djChanged();
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

void App::saveFolders() { saveProfileSettings(); }

// ---- listening stats ----------------------------------------------------------------
// A play counts once half the song (or 4 minutes) has been heard; moving on before that
// counts as a skip. Hearts and counts feed smart playlists and the smart DJ.

void App::onPosition()
{
    if (m_listen.path.isEmpty() || m_listen.counted) return;
    m_listen.maxPos = qMax(m_listen.maxPos, m_player.position());
    const qint64 dur = m_listen.duration > 0 ? m_listen.duration : m_player.duration();
    if ((dur > 0 && m_listen.maxPos >= dur / 2) || m_listen.maxPos >= 240'000) {
        m_listen.counted = true;
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        m_db.recordPlay(m_listen.path, now);
        TrackStats &st = m_stats[m_listen.path];
        st.playCount++;
        st.lastPlayed = now;
        for (TrackListModel *m : {&m_songs, &m_albumSongs, &m_playlistSongs}) m->statsChanged(m_listen.path);
        refreshQuickplay();
    }
}

void App::onTrackChanged()
{
    if (!m_listen.path.isEmpty() && !m_listen.counted && m_listen.maxPos >= 1000) {
        m_db.recordSkip(m_listen.path);
        m_stats[m_listen.path].skipCount++;
    }
    const Track *t = m_player.currentTrack();
    m_listen = t ? Listen{t->path, 0, t->durationMs, false} : Listen{};
    emit statsChanged();   // currentHearted
    djTopUp();
}

bool App::currentHearted() const
{
    const Track *t = m_player.currentTrack();
    return t && m_stats.value(t->path).hearted;
}

void App::toggleHeart(const QString &path)
{
    if (path.isEmpty() || !profileReady()) return;
    TrackStats &st = m_stats[path];
    st.hearted = !st.hearted;
    m_db.setHearted(path, st.hearted);
    for (TrackListModel *m : {&m_songs, &m_albumSongs, &m_playlistSongs}) m->statsChanged(path);
    emit statsChanged();
    if (m_selPlaylist.startsWith(QLatin1String("auto:"))) refreshPlaylist();
    else emit playlistsChanged();
}

void App::toggleHeartCurrent()
{
    if (const Track *t = m_player.currentTrack()) toggleHeart(t->path);
}

// ---- queue ---------------------------------------------------------------------------

QList<Track> App::tracksForPaths(const QStringList &paths) const
{
    QList<Track> out;
    for (const QString &p : paths) {
        const int i = m_libraryIndex.value(p, -1);
        if (i >= 0) out.append(m_library[i]);
    }
    return out;
}

void App::playNextPaths(const QStringList &paths) { m_player.playNext(tracksForPaths(paths)); }
void App::enqueuePaths(const QStringList &paths) { m_player.enqueue(tracksForPaths(paths)); }

// ---- playlists -----------------------------------------------------------------------

static qint64 playlistId(const QString &key)
{
    return key.startsWith(QLatin1String("pl:")) ? key.mid(3).toLongLong() : 0;
}

QList<Track> App::playlistTracks(const QString &key) const
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (key.startsWith(QLatin1String("auto:"))) {
        for (const AutoList &a : autoLists())
            if (a.key == key) return a.rules.apply(m_library, m_stats, now);
        return {};
    }
    const Playlist p = m_db.playlist(playlistId(key));
    if (!p.id) return {};
    if (p.smart) return SmartRules::fromJson(p.rules).apply(m_library, m_stats, now, quint32(p.id));
    return tracksForPaths(m_db.playlistItems(p.id));
}

QVariantList App::playlists() const
{
    QVariantList out;
    for (const AutoList &a : autoLists())
        out.append(QVariantMap{{QStringLiteral("key"), a.key}, {QStringLiteral("name"), a.name},
                               {QStringLiteral("kind"), QStringLiteral("auto")}});
    for (const Playlist &p : m_db.playlists())
        out.append(QVariantMap{{QStringLiteral("key"), QStringLiteral("pl:") + QString::number(p.id)},
                               {QStringLiteral("name"), p.name},
                               {QStringLiteral("kind"), p.smart ? QStringLiteral("smart") : QStringLiteral("manual")}});
    return out;
}

QVariantMap App::selectedPlaylistInfo() const
{
    QVariantMap m{{QStringLiteral("key"), m_selPlaylist}, {QStringLiteral("count"), m_playlistSongs.count()}};
    qint64 ms = 0;
    for (const Track &t : m_playlistSongs.tracks()) ms += t.durationMs;
    m.insert(QStringLiteral("length"), TrackListModel::formatDuration(ms));
    if (m_selPlaylist.startsWith(QLatin1String("auto:"))) {
        for (const AutoList &a : autoLists())
            if (a.key == m_selPlaylist) { m.insert(QStringLiteral("name"), a.name); m.insert(QStringLiteral("kind"), QStringLiteral("auto")); }
        return m;
    }
    const Playlist p = m_db.playlist(playlistId(m_selPlaylist));
    if (!p.id) return {};
    m.insert(QStringLiteral("name"), p.name);
    m.insert(QStringLiteral("kind"), p.smart ? QStringLiteral("smart") : QStringLiteral("manual"));
    m.insert(QStringLiteral("rules"), p.rules);
    m.insert(QStringLiteral("pinned"), isPinned(QStringLiteral("playlist"), QString::number(p.id)));
    return m;
}

void App::refreshPlaylist()
{
    m_playlistSongs.setTracks(m_selPlaylist.isEmpty() ? QList<Track>{} : playlistTracks(m_selPlaylist));
    emit playlistSelectionChanged();
}

void App::selectPlaylist(const QString &key)
{
    m_selPlaylist = key;
    refreshPlaylist();
}

void App::playPlaylist(int startRow)
{
    if (m_playlistSongs.count() == 0) return;
    m_djSeed.clear(); emit djChanged();
    m_player.setQueue(m_playlistSongs.tracks(), startRow);
}

QString App::createPlaylist(const QString &name, const QStringList &paths)
{
    const qint64 id = m_db.createPlaylist(name);
    if (!id) return {};
    if (!paths.isEmpty()) m_db.setPlaylistItems(id, paths);
    emit playlistsChanged();
    return QStringLiteral("pl:") + QString::number(id);
}

QString App::createSmartPlaylist(const QString &name, const QString &rulesJson)
{
    const qint64 id = m_db.createPlaylist(name, true, SmartRules::fromJson(rulesJson).toJson());
    if (!id) return {};
    emit playlistsChanged();
    return QStringLiteral("pl:") + QString::number(id);
}

bool App::renamePlaylist(const QString &key, const QString &name)
{
    if (!m_db.renamePlaylist(playlistId(key), name)) return false;
    emit playlistsChanged();
    if (key == m_selPlaylist) emit playlistSelectionChanged();
    refreshQuickplay();
    return true;
}

bool App::setSmartRules(const QString &key, const QString &rulesJson)
{
    if (!m_db.setPlaylistRules(playlistId(key), SmartRules::fromJson(rulesJson).toJson())) return false;
    if (key == m_selPlaylist) refreshPlaylist();
    return true;
}

bool App::deletePlaylist(const QString &key)
{
    if (!m_db.deletePlaylist(playlistId(key))) return false;
    if (key == m_selPlaylist) { m_selPlaylist = QStringLiteral("auto:hearted"); refreshPlaylist(); }
    emit playlistsChanged();
    refreshQuickplay();
    return true;
}

bool App::addToPlaylist(const QString &key, const QStringList &paths)
{
    const Playlist p = m_db.playlist(playlistId(key));
    if (!p.id || p.smart || !m_db.addToPlaylist(p.id, paths)) return false;
    if (key == m_selPlaylist) refreshPlaylist();
    return true;
}

bool App::removeFromPlaylist(int row)
{
    const Playlist p = m_db.playlist(playlistId(m_selPlaylist));
    if (!p.id || p.smart) return false;
    // Rows can differ from stored positions when a file has gone missing; map by path.
    const QStringList items = m_db.playlistItems(p.id);
    if (row < 0 || row >= m_playlistSongs.count()) return false;
    const QString path = m_playlistSongs.tracks()[row].path;
    int seen = 0;
    for (int i = 0; i < items.size(); ++i) {
        if (!m_libraryIndex.contains(items[i])) continue;
        if (seen++ == row && items[i] == path) { m_db.removeFromPlaylist(p.id, i); break; }
    }
    refreshPlaylist();
    return true;
}

bool App::movePlaylistItem(int from, int to)
{
    const Playlist p = m_db.playlist(playlistId(m_selPlaylist));
    if (!p.id || p.smart) return false;
    QStringList visible;
    for (const Track &t : m_playlistSongs.tracks()) visible << t.path;
    if (from < 0 || to < 0 || from >= visible.size() || to >= visible.size()) return false;
    visible.move(from, to);
    // Missing files keep their place at the end so they come back if the drive returns.
    QStringList rest;
    for (const QString &i : m_db.playlistItems(p.id))
        if (!m_libraryIndex.contains(i)) rest << i;
    m_db.setPlaylistItems(p.id, visible + rest);
    refreshPlaylist();
    return true;
}

QString App::importPlaylist(const QUrl &file)
{
    if (!profileReady()) return QStringLiteral("no profile open");
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    const PlaylistIO::Parsed parsed = PlaylistIO::read(path);
    if (!parsed.error.isEmpty() && parsed.entries.isEmpty()) return QStringLiteral("could not read the playlist: ") + parsed.error;
    int missing = 0;
    const QStringList paths = PlaylistIO::resolve(parsed.entries, QFileInfo(path).absolutePath(), m_libraryIndex.keys(), &missing);
    if (paths.isEmpty()) return QStringLiteral("none of the %1 songs in that playlist are in your music folders").arg(parsed.entries.size());
    QString name = parsed.name;
    QStringList taken;
    for (const Playlist &p : m_db.playlists()) taken << p.name;
    for (int n = 2; taken.contains(name); ++n) name = parsed.name + QStringLiteral(" (%1)").arg(n);
    const QString key = createPlaylist(name, paths);
    if (key.isEmpty()) return QStringLiteral("could not save the playlist");
    selectPlaylist(key);
    return missing ? QStringLiteral("imported %1 songs; %2 were not found in your music folders").arg(paths.size()).arg(missing)
                   : QStringLiteral("imported %1 songs").arg(paths.size());
}

QString App::exportPlaylist(const QString &key, const QUrl &file)
{
    QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (!path.endsWith(QLatin1String(".m3u8"), Qt::CaseInsensitive) && !path.endsWith(QLatin1String(".m3u"), Qt::CaseInsensitive))
        path += QStringLiteral(".m3u8");
    QString name = key;
    for (const QVariant &v : playlists())
        if (v.toMap().value(QStringLiteral("key")) == key) name = v.toMap().value(QStringLiteral("name")).toString();
    QString err;
    if (!PlaylistIO::writeM3u8(path, name, playlistTracks(key), &err)) return QStringLiteral("could not save: ") + err;
    return QStringLiteral("saved ") + QFileInfo(path).fileName();
}

int App::smartPreviewCount(const QString &rulesJson) const
{
    return int(SmartRules::fromJson(rulesJson).apply(m_library, m_stats, QDateTime::currentSecsSinceEpoch()).size());
}

QStringList App::albumPaths(int allAlbumsRow) const
{
    QStringList out;
    if (const Album *a = m_allAlbums.at(allAlbumsRow))
        for (int row : a->trackRows) out << m_songs.tracks()[row].path;
    return out;
}

QStringList App::selectedAlbumPaths() const
{
    QStringList out;
    for (const Track &t : m_albumSongs.tracks()) out << t.path;
    return out;
}

// ---- quickplay -------------------------------------------------------------------------

static const QChar kRefSep(0x1f);

QString App::albumRef(int allAlbumsRow) const
{
    const Album *a = m_allAlbums.at(allAlbumsRow);
    return a ? a->artist + kRefSep + a->title : QString();
}

QString App::selectedAlbumRef() const
{
    const Album *a = m_artistAlbums.at(m_selAlbum);
    return a ? a->artist + kRefSep + a->title : QString();
}

QList<Track> App::tracksForAlbumRef(const QString &ref) const
{
    const QString artist = ref.section(kRefSep, 0, 0), title = ref.section(kRefSep, 1);
    for (const Album &a : m_fullCollection.albums()) {
        if (a.artist != artist || a.title != title) continue;
        QList<Track> out;
        for (int row : a.trackRows) out.append(m_library[row]);
        return out;
    }
    return {};
}

QVariantMap App::tile(const QString &kind, const QString &ref, const QString &label) const
{
    QVariantMap m{{QStringLiteral("kind"), kind}, {QStringLiteral("ref"), ref}, {QStringLiteral("title"), label},
                  {QStringLiteral("subtitle"), QString()}, {QStringLiteral("art"), QString()},
                  {QStringLiteral("pinned"), isPinned(kind, ref)}};
    if (kind == QLatin1String("album")) {
        const QString artist = ref.section(kRefSep, 0, 0), title = ref.section(kRefSep, 1);
        m[QStringLiteral("title")] = title.isEmpty() ? QStringLiteral("unknown album") : title;
        m[QStringLiteral("subtitle")] = artist;
        for (const Album &a : m_fullCollection.albums())
            if (a.artist == artist && a.title == title) {
                if (!a.art.isEmpty()) m[QStringLiteral("art")] = QUrl::fromLocalFile(a.art).toString();
                m[QStringLiteral("available")] = true;
            }
    } else if (kind == QLatin1String("artist")) {
        m[QStringLiteral("title")] = ref;
        m[QStringLiteral("subtitle")] = QStringLiteral("artist");
        for (const Artist &a : m_fullCollection.artists())
            if (a.name == ref) {
                if (!a.art.isEmpty()) m[QStringLiteral("art")] = QUrl::fromLocalFile(a.art).toString();
                m[QStringLiteral("subtitle")] = QStringLiteral("%1 songs").arg(a.songCount);
                m[QStringLiteral("available")] = true;
            }
    } else if (kind == QLatin1String("playlist")) {
        const Playlist p = m_db.playlist(ref.toLongLong());
        m[QStringLiteral("title")] = p.id ? p.name : label;
        m[QStringLiteral("subtitle")] = p.smart ? QStringLiteral("smart playlist") : QStringLiteral("playlist");
        m[QStringLiteral("available")] = p.id != 0;
        // Cover: the first song with art.
        for (const Track &t : playlistTracks(QStringLiteral("pl:") + ref))
            if (!t.art.isEmpty()) { m[QStringLiteral("art")] = QUrl::fromLocalFile(t.art).toString(); break; }
    }
    return m;
}

void App::refreshQuickplay()
{
    m_qpPinned.clear(); m_qpHistory.clear(); m_qpNew.clear();
    if (profileReady()) {
        for (const Pin &p : m_db.pins()) {
            QVariantMap t = tile(p.kind, p.ref, p.label);
            if (t.value(QStringLiteral("available")).toBool()) m_qpPinned.append(t);
        }
        // history: recently played albums, newest first, each once
        QStringList seen;
        for (const HistoryEntry &h : m_db.history(400)) {
            const int i = m_libraryIndex.value(h.path, -1);
            if (i < 0) continue;
            const Track &t = m_library[i];
            const QString ref = (t.albumArtist.isEmpty() ? t.displayArtist() : t.albumArtist) + kRefSep + t.album;
            if (seen.contains(ref)) continue;
            seen << ref;
            m_qpHistory.append(tile(QStringLiteral("album"), ref, t.album));
            if (m_qpHistory.size() >= 12) break;
        }
        // new: albums by when their newest file appeared
        QList<std::pair<qint64, const Album *>> byAdded;
        for (const Album &a : m_fullCollection.albums()) {
            qint64 newest = 0;
            for (int row : a.trackRows) newest = qMax(newest, m_library[row].added);
            byAdded.append({newest, &a});
        }
        std::stable_sort(byAdded.begin(), byAdded.end(), [](auto &x, auto &y) { return x.first > y.first; });
        for (int i = 0; i < byAdded.size() && i < 12; ++i)
            m_qpNew.append(tile(QStringLiteral("album"), byAdded[i].second->artist + kRefSep + byAdded[i].second->title, byAdded[i].second->title));
    }
    emit quickplayChanged();
}

void App::pinTile(const QString &kind, const QString &ref, const QString &label)
{
    if (m_db.pin({kind, ref, label})) { refreshQuickplay(); emit playlistSelectionChanged(); }
}

void App::unpinTile(const QString &kind, const QString &ref)
{
    if (m_db.unpin(kind, ref)) { refreshQuickplay(); emit playlistSelectionChanged(); }
}

bool App::isPinned(const QString &kind, const QString &ref) const { return m_db.isPinned(kind, ref); }

void App::playTile(const QString &kind, const QString &ref)
{
    QList<Track> q;
    if (kind == QLatin1String("album")) q = tracksForAlbumRef(ref);
    else if (kind == QLatin1String("playlist")) q = playlistTracks(QStringLiteral("pl:") + ref);
    else if (kind == QLatin1String("artist"))
        for (const Track &t : m_library)
            if ((t.albumArtist.isEmpty() ? t.displayArtist() : t.albumArtist) == ref || t.artist == ref) q.append(t);
    if (!q.isEmpty()) { m_djSeed.clear(); emit djChanged(); m_player.setQueue(q, 0); }
}

bool App::showAlbum(const QString &ref)
{
    if (!m_search.isEmpty()) setSearchText({});
    applySelection(ref.section(kRefSep, 0, 0), ref.section(kRefSep, 1));
    return selectedAlbumRef() == ref;
}

// ---- smart DJ --------------------------------------------------------------------------

QStringList App::djArtists() const
{
    QStringList out;
    for (const Artist &a : m_fullCollection.artists()) out << a.name;
    return out;
}

void App::startDj(const QString &artist)
{
    if (m_library.isEmpty()) return;
    m_djSeed = artist;
    m_djRound = QRandomGenerator::global()->generate();
    emit djChanged();
    m_player.setQueue(smartMix(artist, m_library, m_stats, 25, m_djRound), 0);
}

// Endless: when the mix is nearly used up, add another batch (skipping recent repeats).
void App::djTopUp()
{
    if (m_djSeed.isEmpty() || !m_player.hasTrack()) return;
    if (m_player.currentIndex() < m_player.queueLength() - 3) return;
    const QList<Track> q = m_player.queue();
    QSet<QString> recent;
    for (int i = qMax(0, int(q.size()) - 60); i < q.size(); ++i) recent.insert(q[i].path);
    QList<Track> more;
    for (const Track &t : smartMix(m_djSeed, m_library, m_stats, 60, ++m_djRound))
        if (!recent.contains(t.path) && more.size() < 25) more.append(t);
    if (more.isEmpty()) more = smartMix(m_djSeed, m_library, m_stats, 25, ++m_djRound);
    m_player.enqueue(more);
}

} // namespace hop
