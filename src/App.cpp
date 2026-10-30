// SPDX-License-Identifier: GPL-3.0-or-later
#include "App.h"

#include "core/Paths.h"
#include "library/Smart.h"
#include "library/TagReader.h"
#include "library/VideoProbe.h"
#include "playback/GstVideoPlayer.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QColor>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QStyleHints>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include <QMap>
#include <algorithm>

namespace hop {

// Ref format shared with Pins (spec §4.4): "<artist>\x1f<title>", built from the album's
// raw tag-derived fields — never from a profile's display override — so a renamed album
// keeps the same ref its pins/overrides/track order were saved under.
static const QChar kRefSep(0x1f);

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
        m_watcher.setFolders(watchedFolders());   // pick up any subfolder the scan just found
    });

    // Video (spec §5b): kept as its own scanner/index (see VideoScanner's header comment),
    // but driven by the same "folders" list and the same rescan()/reload() call sites.
    m_videoScanner.setThumbCacheDir(Paths::videoThumbCacheDir());
    connect(&m_videoScanner, &VideoScanner::started, this, &App::videoScanProgressChanged);
    connect(&m_videoScanner, &VideoScanner::progress, this, [this](int done, int total) {
        m_videoScanDone = done;
        m_videoScanTotal = total;
        emit videoScanProgressChanged();
    });
    connect(&m_videoScanner, &VideoScanner::saved, this, [this] {
        // Show what has been found so far, at most every couple of seconds.
        if (m_videoReloadClock.isValid() && m_videoReloadClock.elapsed() < 2000) return;
        m_videoReloadClock.start();
        reloadVideos();
    });
    connect(&m_videoScanner, &VideoScanner::finished, this, [this](int added, int updated, int removed, int) {
        emit videoScanProgressChanged();
        if (added || updated || removed || m_videos.count() == 0) reloadVideos();
        if (m_videoScanRescanPending) { m_videoScanRescanPending = false; m_videoScanner.start(allVideoFolders()); }
        m_watcher.setFolders(watchedFolders());
    });
    // The other collection sections: one scanner per kind over the shared SectionIndex.
    for (int i = 0; i < 4; ++i) {
        m_sec[i] = std::make_unique<SectionState>(kAllSections[i], &m_sectionIndex);
        SectionState *st = m_sec[i].get();
        connect(&st->scanner, &SectionScanner::started, this, &App::sectionStatusChanged);
        connect(&st->scanner, &SectionScanner::progress, this, [this, st](int done, int total) {
            st->done = done;
            st->total = total;
            emit sectionStatusChanged();
        });
        connect(&st->scanner, &SectionScanner::finished, this, [this, st, i](int added, int updated, int removed, int) {
            emit sectionStatusChanged();
            if (added || updated || removed || st->model.count() == 0) reloadSection(i);
            if (st->rescanPending) { st->rescanPending = false; st->scanner.start(st->folders); }
            m_watcher.setFolders(watchedFolders());
        });
    }
    // Live library updates (spec §6): a local inotify watch, nothing more — see
    // LibraryWatcher's header comment. Debounced, so one rescan covers a whole copy.
    connect(&m_watcher, &LibraryWatcher::changed, this, [this] { rescan(); });
    for (TrackListModel *m : {&m_songs, &m_albumSongs, &m_playlistSongs}) m->setStats(&m_stats);
    m_openTimer.setSingleShot(true);
    m_openTimer.setInterval(250);
    connect(&m_openTimer, &QTimer::timeout, this, [this] { const QStringList b = m_openBatch; m_openBatch.clear(); openFiles(b); });
    connect(&m_player, &Player::positionChanged, this, &App::onPosition);
    connect(&m_player, &Player::trackStarted, this, &App::onTrackChanged);
    // Playback preferences are per profile.
    for (auto sig : {&Player::crossfadeChanged, &Player::replayGainChanged, &Player::repeatModeChanged, &Player::shuffleChanged})
        connect(&m_player, sig, this, [this] { if (!m_loadingSettings) saveProfileSettings(); });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (auto *hints = QGuiApplication::styleHints())
        connect(hints, &QStyleHints::colorSchemeChanged, this, [this] {
            if (m_theme == QLatin1String("system")) { emit appearanceChanged(); emit profileChanged(); }
        });
#endif
    connect(&m_player, &Player::currentChanged, this, [this] { emit statsChanged(); djTopUp(); });
}

// Moves a damaged SQLite file (and its -wal/-shm companions) to "<file>.damaged-<time>".
void App::setAside(const QString &file)
{
    const QString stamp = QString::number(QDateTime::currentSecsSinceEpoch());
    for (const QString &suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")})
        if (QFile::exists(file + suffix)) QFile::rename(file + suffix, file + QStringLiteral(".damaged-") + stamp + suffix);
}

App::~App()
{
    saveProfileSettings();
    m_scanner.cancel();
    m_videoScanner.cancel();
}

bool App::init()
{
    Paths::ensureDirs();
    if (!m_index.open(Paths::indexDbPath())) {
        // The index is only a cache of tags; if it is damaged, keep a copy aside and rebuild.
        qWarning("hop-tunes: track index unreadable (%s); rebuilding it", qPrintable(m_index.lastError()));
        m_index.close();
        setAside(Paths::indexDbPath());
        if (!m_index.open(Paths::indexDbPath())) {
            qWarning("hop-tunes: cannot create %s: %s", qPrintable(Paths::indexDbPath()), qPrintable(m_index.lastError()));
            return false;
        }
    }
    if (!m_videoIndex.open(Paths::videoIndexDbPath())) {
        qWarning("hop-tunes: video index unreadable (%s); rebuilding it", qPrintable(m_videoIndex.lastError()));
        m_videoIndex.close();
        setAside(Paths::videoIndexDbPath());
        if (!m_videoIndex.open(Paths::videoIndexDbPath()))
            qWarning("hop-tunes: cannot create %s: %s", qPrintable(Paths::videoIndexDbPath()), qPrintable(m_videoIndex.lastError()));
    }
    if (!m_sectionIndex.open(Paths::sectionIndexDbPath())) {
        qWarning("hop-tunes: sections index unreadable (%s); rebuilding it", qPrintable(m_sectionIndex.lastError()));
        m_sectionIndex.close();
        setAside(Paths::sectionIndexDbPath());
        if (!m_sectionIndex.open(Paths::sectionIndexDbPath()))
            qWarning("hop-tunes: cannot create %s: %s", qPrintable(Paths::sectionIndexDbPath()), qPrintable(m_sectionIndex.lastError()));
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
    // HOPTUNES_MUSIC_DIR names the folder offered on first run (tests, kiosks, portable setups).
    const QByteArray forced = qgetenv("HOPTUNES_MUSIC_DIR");
    const QString music = forced.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::MusicLocation)
                                           : QString::fromLocal8Bit(forced);
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
    const QString a = p ? p->accent : accents().value(0);
    // The accent is also a text colour; on a light background it needs to be darker to read.
    return lightTheme() ? QColor(a).darker(165).name() : a;
}

bool App::systemThemeAvailable() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return true;
#else
    return false;
#endif
}

bool App::lightTheme() const
{
    if (m_theme == QLatin1String("light")) return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (m_theme == QLatin1String("system"))
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light;
#endif
    return false;
}

void App::setTheme(const QString &theme)
{
    const QString t = (theme == QLatin1String("light") || theme == QLatin1String("system")) ? theme : QStringLiteral("dark");
    if (t == m_theme) return;
    m_theme = t;
    saveProfileSettings();
    emit appearanceChanged();
    emit profileChanged();   // accent
}

void App::setReduceMotion(bool on)
{
    if (on == m_reduceMotion) return;
    m_reduceMotion = on;
    saveProfileSettings();
    emit appearanceChanged();
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
        : QFileInfo(Paths::folderPath(folder)).canonicalFilePath();
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
    m_videoFolders.clear();
    m_videoFoldersOwn = false;
    m_albumNames.clear();
    m_videoCatFolders.clear();
    m_videoCategory = QStringLiteral("movies");
    for (auto &sec : m_sec) sec->folders.clear();
    if (guest) {
        if (!suggestedFolder().isEmpty()) m_folders << suggestedFolder();
    } else {
        for (const QJsonValue &v : s.value(QStringLiteral("folders")).toArray()) m_folders << v.toString();
        for (auto &sec : m_sec)
            for (const QJsonValue &v : s.value(sectionKey(sec->kind) + QStringLiteral("Folders")).toArray())
                sec->folders << v.toString();
        const QJsonObject an = s.value(QStringLiteral("pictureAlbumNames")).toObject();
        for (auto it = an.constBegin(); it != an.constEnd(); ++it) m_albumNames.insert(it.key(), it.value().toString());
        if (s.contains(QStringLiteral("videoFolders"))) {
            m_videoFoldersOwn = true;
            for (const QJsonValue &v : s.value(QStringLiteral("videoFolders")).toArray()) m_videoFolders << v.toString();
        }
        const QJsonObject cats = s.value(QStringLiteral("videoCategoryFolders")).toObject();
        for (const QString &c : videoCategories())
            for (const QJsonValue &v : cats.value(c).toArray()) m_videoCatFolders[c] << v.toString();
    }
    m_loadingSettings = true;
    m_player.setVolume(s.value(QStringLiteral("volume")).toDouble(1.0));
    m_player.setCrossfadeMs(s.value(QStringLiteral("crossfadeMs")).toInt(0));
    m_player.setReplayGainMode(s.value(QStringLiteral("replayGain")).toInt(Player::GainOff));
    m_player.setReplayGainPreamp(s.value(QStringLiteral("replayGainPreamp")).toDouble(0.0));
    m_player.setRepeatMode(s.value(QStringLiteral("repeat")).toInt(Player::RepeatOff));
    m_player.setShuffle(s.value(QStringLiteral("shuffle")).toBool(false));
    m_theme = s.value(QStringLiteral("theme")).toString(QStringLiteral("dark"));
    m_reduceMotion = s.value(QStringLiteral("reduceMotion")).toBool(false);
    m_videoViewStyle = s.value(QStringLiteral("videoViewStyle")).toString(QStringLiteral("window"));
    if (m_videoViewStyle != QLatin1String("theater") && m_videoViewStyle != QLatin1String("fullscreen")) m_videoViewStyle = QStringLiteral("window");
    m_navStyle = s.value(QStringLiteral("navStyle")).toString() == QLatin1String("drawer") ? QStringLiteral("drawer") : QStringLiteral("bar");
    m_statusNote = s.value(QStringLiteral("statusNote")).toString().left(300);
    m_videoVolume = qBound(0, s.value(QStringLiteral("videoVolume")).toInt(100), 150);
    m_videoChat = s.value(QStringLiteral("videoChat")).toBool(false);
    m_videoChatLook = s.value(QStringLiteral("videoChatLook")).toString() == QLatin1String("lively") ? QStringLiteral("lively") : QStringLiteral("clean");
    m_loadingSettings = false;
    emit videoChatChanged();
    emit navStyleChanged();
    emit statusNoteChanged();
    emit videoVolumeChanged();
    emit videoCategoryChanged();
    m_videoComments = [&] { const QString m = s.value(QStringLiteral("videoComments")).toString();
                            return m == QLatin1String("mine") || m == QLatin1String("populated") ? m : QStringLiteral("off"); }();
    emit videoCommentsChanged();
    emit videoViewStyleChanged();
    emit appearanceChanged();
    const QString dbPath = guest ? QStringLiteral(":memory:") : m_store.dirFor(id) + QStringLiteral("/profile.sqlite");
    if (!m_db.open(dbPath) && !guest) {
        // Damaged: keep it aside for recovery and start this profile's lists afresh,
        // rather than running without playlists and play counts at all.
        qWarning("hop-tunes: profile database unreadable (%s); set aside, starting a new one", qPrintable(m_db.lastError()));
        m_db.close();   // Windows will not rename a file that is still open
        setAside(dbPath);
        if (!m_db.open(dbPath)) qWarning("hop-tunes: cannot open profile database: %s", qPrintable(m_db.lastError()));
    }
    m_stats = m_db.allStats();
    m_albumOverrides = m_db.albumOverrides();
    m_played = m_db.playedItems();
    m_videoThumbOverrides = m_db.videoThumbOverrides();
    m_videoMeta.clear();
    const QHash<QString, QString> metas = m_db.videoMetas();
    for (auto it = metas.cbegin(); it != metas.cend(); ++it) {
        const QJsonDocument doc = QJsonDocument::fromJson(it.value().toUtf8());
        if (doc.isObject()) m_videoMeta.insert(it.key(), doc.object());
    }
    m_songOrder = m_db.songOrder();
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
    if (!m_pendingOpen.isEmpty()) { const QStringList p = m_pendingOpen; m_pendingOpen.clear(); openFiles(p); }
}

void App::leaveProfile()
{
    if (!profileReady()) return;
    saveProfileSettings();
    m_listen = {};                // plays are counted as they happen; leaving is not a skip
    flushResume();
    if (m_book.isOpen()) closeBook(0.0);
    m_player.setQueue({}, 0);
    m_listen = {};
    m_scanner.cancel();
    m_videoScanner.cancel();
    for (auto &sec : m_sec) sec->scanner.cancel();
    m_videoPlayerOpen = false;
    m_db.close();
    m_stats.clear();
    m_albumOverrides.clear();
    m_played.clear();
    m_videoThumbOverrides.clear();
    m_videoMeta.clear();
    m_detailPath.clear();
    m_songOrder.clear();
    m_search.clear();
    m_selPlaylist.clear();
    m_djSeed.clear();
    m_profileId.clear();
    m_guest = false;
    m_theme = QStringLiteral("dark");
    m_reduceMotion = false;
    m_videoViewStyle = QStringLiteral("window");
    m_navStyle = QStringLiteral("bar");
    m_statusNote.clear();
    m_videoChat = false;
    m_videoChatLook = QStringLiteral("clean");
    emit videoChatChanged();
    emit navStyleChanged();
    emit statusNoteChanged();
    m_videoVolume = 100;
    m_videoChat = false;
    m_videoChatLook = QStringLiteral("clean");
    emit videoChatChanged();
    emit videoVolumeChanged();
    m_videoComments = QStringLiteral("off");
    emit videoCommentsChanged();
    emit videoViewStyleChanged();
    emit appearanceChanged();
    m_folders.clear();
    m_videoFolders.clear();
    m_videoFoldersOwn = false;
    m_albumNames.clear();
    m_videoCatFolders.clear();
    m_videoCategory = QStringLiteral("movies");
    for (auto &sec : m_sec) { sec->scanner.cancel(); sec->folders.clear(); }
    reload();
    emit foldersChanged();
    emit sectionFoldersChanged();
    emit profileChanged();
}

void App::saveProfileSettings()
{
    if (m_profileId.isEmpty()) return;   // guest mode keeps nothing
    QJsonObject s = m_store.settings(m_profileId);
    s.insert(QStringLiteral("folders"), QJsonArray::fromStringList(m_folders));
    if (m_videoFoldersOwn) s.insert(QStringLiteral("videoFolders"), QJsonArray::fromStringList(m_videoFolders));
    QJsonObject an;
    for (auto it = m_albumNames.constBegin(); it != m_albumNames.constEnd(); ++it) an.insert(it.key(), it.value());
    s.insert(QStringLiteral("pictureAlbumNames"), an);
    QJsonObject cats;
    for (auto it = m_videoCatFolders.constBegin(); it != m_videoCatFolders.constEnd(); ++it)
        if (!it.value().isEmpty()) cats.insert(it.key(), QJsonArray::fromStringList(it.value()));
    s.insert(QStringLiteral("videoCategoryFolders"), cats);
    for (const auto &sec : m_sec)
        s.insert(sectionKey(sec->kind) + QStringLiteral("Folders"), QJsonArray::fromStringList(sec->folders));
    s.insert(QStringLiteral("volume"), m_player.volume());
    s.insert(QStringLiteral("crossfadeMs"), m_player.crossfadeMs());
    s.insert(QStringLiteral("replayGain"), m_player.replayGainMode());
    s.insert(QStringLiteral("replayGainPreamp"), m_player.replayGainPreamp());
    s.insert(QStringLiteral("repeat"), m_player.repeatMode());
    s.insert(QStringLiteral("shuffle"), m_player.shuffle());
    s.insert(QStringLiteral("theme"), m_theme);
    s.insert(QStringLiteral("reduceMotion"), m_reduceMotion);
    s.insert(QStringLiteral("videoViewStyle"), m_videoViewStyle);
    s.insert(QStringLiteral("navStyle"), m_navStyle);
    s.insert(QStringLiteral("statusNote"), m_statusNote);
    s.insert(QStringLiteral("videoVolume"), m_videoVolume);
    s.insert(QStringLiteral("videoChat"), m_videoChat);
    s.insert(QStringLiteral("videoChatLook"), m_videoChatLook);
    s.insert(QStringLiteral("videoComments"), m_videoComments);
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
    m_videoScanner.cancel();
    m_videoPlayerOpen = false;
    m_db.close();
    m_stats.clear();
    m_profileId.clear();          // nothing is written back for a profile being removed
    m_folders.clear();
    m_videoFolders.clear();
    m_videoFoldersOwn = false;
    m_albumNames.clear();
    m_videoCatFolders.clear();
    m_videoCategory = QStringLiteral("movies");
    for (auto &sec : m_sec) sec->folders.clear();
    reload();
    emit foldersChanged();
    emit sectionFoldersChanged();
    const bool ok = m_store.remove(id);
    emit profilesChanged();
    emit profileChanged();
    return ok ? QString() : m_store.lastError();
}

QString App::version() const { return QCoreApplication::applicationVersion(); }

void App::addFolder(const QUrl &folder)
{
    const QString path = QFileInfo(Paths::folderPath(folder))
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

void App::addVideoFolder(const QUrl &folder)
{
    const QString path = QFileInfo(Paths::folderPath(folder))
                             .canonicalFilePath();
    const QStringList current = videoFolders();
    if (path.isEmpty() || current.contains(path)) return;
    // First video folder of its own: start from what videos already scan (the shared media
    // folders), so nothing disappears from the library; the user can then remove what they don't want.
    if (!m_videoFoldersOwn) { m_videoFolders = current; m_videoFoldersOwn = true; }
    m_videoFolders << path;
    saveFolders();
    emit foldersChanged();
    rescan();
}

int App::sectionIndexOf(const QString &kind) const
{
    SectionKind k;
    if (!sectionFromKey(kind, &k)) return -1;
    for (int i = 0; i < 4; ++i) if (m_sec[i]->kind == k) return i;
    return -1;
}

QVariantMap App::sectionFolders() const
{
    QVariantMap out;
    for (const auto &sec : m_sec) out.insert(sectionKey(sec->kind), sec->folders);
    return out;
}

QVariantMap App::sectionStatus() const
{
    QVariantMap out;
    for (const auto &sec : m_sec)
        out.insert(sectionKey(sec->kind), QVariantMap{{QStringLiteral("scanning"), sec->scanner.isRunning()},
                                                      {QStringLiteral("done"), sec->done},
                                                      {QStringLiteral("total"), sec->total}});
    return out;
}

void App::addSectionFolder(const QString &kind, const QUrl &folder)
{
    const int i = sectionIndexOf(kind);
    if (i < 0) return;
    const QString path = QFileInfo(Paths::folderPath(folder))
                             .canonicalFilePath();
    if (path.isEmpty() || m_sec[i]->folders.contains(path)) return;
    m_sec[i]->folders << path;
    saveFolders();
    emit sectionFoldersChanged();
    rescan();
}

void App::removeSectionFolder(const QString &kind, const QString &path)
{
    const int i = sectionIndexOf(kind);
    if (i < 0 || !m_sec[i]->folders.removeAll(path)) return;
    saveFolders();
    emit sectionFoldersChanged();
    reloadSection(i);
}

void App::playSectionItem(const QString &kind, int row)
{
    const int i = sectionIndexOf(kind);
    if (i < 0 || !profileReady()) return;
    const SectionKind k = m_sec[i]->kind;
    if (k != SectionKind::Audiobooks && k != SectionKind::Podcasts) return;
    const QString path = m_sec[i]->model.pathAt(row);
    if (path.isEmpty() || !QFileInfo::exists(path)) return;
    flushResume();
    const int lib = m_libraryIndex.value(path, -1);
    Track t = lib >= 0 ? m_library[lib] : TagReader::read(QFileInfo(path));
    t.path = path;
    const VideoProgress p = m_db.isOpen() ? m_db.videoProgress(path) : VideoProgress{};
    m_djSeed.clear();
    emit djChanged();
    m_resumePath = path;
    m_resumeSavedMs = m_resumeLastMs = p.positionMs;
    m_resumeDurationMs = p.durationMs;
    m_player.playFrom(t, p.positionMs);
}

QStringList App::bookChapterTitles()
{
    QStringList t;
    for (int i = 0; i < m_book.chapterCount(); ++i) t << m_book.chapterTitle(i);
    return t;
}

// Reading positions share the per-path resume table: chapter * 1e6 + fraction of the chapter,
// out of chapters * 1e6, so the list's progress bar is simply position / duration.
namespace { constexpr qint64 kChapterScale = 1'000'000; }

void App::openBook(int row)
{
    const int i = sectionIndexOf(QStringLiteral("ebooks"));
    if (i < 0 || !profileReady()) return;
    const QString path = m_sec[i]->model.pathAt(row);
    if (path.isEmpty() || !QFileInfo::exists(path)) return;
    if (QFileInfo(path).suffix().toLower() != QLatin1String("epub")) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));   // PDFs: the default viewer
        return;
    }
    if (!m_book.open(path, Paths::cacheDir() + QStringLiteral("/epub"))) {
        qWarning("hop-tunes: cannot open %s: %s", qPrintable(path), qPrintable(m_book.lastError()));
        return;
    }
    m_bookPath = path;
    m_bookChapter = 0;
    m_bookResumeFraction = 0.0;
    const VideoProgress p = m_db.isOpen() ? m_db.videoProgress(path) : VideoProgress{};
    if (p.positionMs > 0) {
        m_bookChapter = qBound<int>(0, int(p.positionMs / kChapterScale), m_book.chapterCount() - 1);
        m_bookResumeFraction = double(p.positionMs % kChapterScale) / double(kChapterScale - 1);
    }
    emit bookChanged();
}

void App::goToBookChapter(int index)
{
    if (!m_book.isOpen() || index < 0 || index >= m_book.chapterCount()) return;
    m_bookChapter = index;
    m_bookResumeFraction = 0.0;
    emit bookChanged();
    saveBookProgress(0.0);
}

void App::saveBookProgress(double fraction)
{
    if (!m_book.isOpen() || m_bookPath.isEmpty() || !m_db.isOpen() || m_guest) return;
    fraction = qBound(0.0, fraction, 1.0);
    const int n = m_book.chapterCount();
    const qint64 pos = qint64(m_bookChapter) * kChapterScale + qint64(fraction * double(kChapterScale - 1));
    m_db.setVideoProgress(m_bookPath, pos, qint64(n) * kChapterScale);
    const bool finished = m_bookChapter == n - 1 && fraction >= 0.99;
    for (auto &sec : m_sec) sec->model.setProgress(m_bookPath, finished ? 0.0 : (double(m_bookChapter) + fraction) / double(n));
    if (finished && !m_played.contains(m_bookPath)) {
        m_played.insert(m_bookPath);
        m_db.setItemPlayed(m_bookPath, true);
        for (auto &sec : m_sec) sec->model.setPlayed(m_bookPath, true);
    }
}

void App::closeBook(double fraction)
{
    saveBookProgress(fraction);
    m_book.close();
    m_bookPath.clear();
    m_bookChapter = 0;
    m_bookResumeFraction = 0.0;
    emit bookChanged();
}

void App::reloadSections() { for (int i = 0; i < 4; ++i) reloadSection(i); }

void App::reloadSection(int i)
{
    SectionState &sec = *m_sec[i];
    const QList<SectionItem> items = m_sectionIndex.itemsUnder(sec.kind, sec.folders);
    QHash<QString, double> progress;
    if (sec.kind != SectionKind::Pictures && m_db.isOpen()) {
        for (const SectionItem &it : items) {
            const VideoProgress p = m_db.videoProgress(it.path);   // same per-path resume table videos use
            if (p.durationMs > 0 && p.positionMs > 0) progress.insert(it.path, double(p.positionMs) / double(p.durationMs));
        }
    }
    sec.model.setItems(items, progress, m_played);
    if (sec.kind == SectionKind::Pictures) rebuildPictureAlbums(items);
}

// Pictures are shown as albums, one per folder they sit in; clicking one opens its pictures.
void App::rebuildPictureAlbums(const QList<SectionItem> &items)
{
    QMap<QString, QList<SectionItem>> byDir;   // folder -> its pictures, in the index's order
    for (const SectionItem &it : items) byDir[QFileInfo(it.path).absolutePath()].append(it);
    QHash<QString, int> names;   // two folders can share a name: those also show their parent
    for (auto d = byDir.constBegin(); d != byDir.constEnd(); ++d) ++names[QFileInfo(d.key()).fileName().toLower()];
    QList<QVariantMap> albums;
    for (auto d = byDir.constBegin(); d != byDir.constEnd(); ++d) {
        const QFileInfo fi(d.key());
        QString name = fi.fileName().isEmpty() ? d.key() : fi.fileName();
        if (names.value(fi.fileName().toLower()) > 1) name += QStringLiteral(" (") + QFileInfo(fi.absolutePath()).fileName() + QLatin1Char(')');
        const bool named = m_albumNames.contains(d.key());
        if (named) name = m_albumNames.value(d.key());
        albums.append({{QStringLiteral("dir"), d.key()}, {QStringLiteral("name"), name}, {QStringLiteral("renamed"), named}, {QStringLiteral("count"), d.value().size()},
                       {QStringLiteral("cover"), QUrl::fromLocalFile(d.value().first().path).toString()}});
    }
    std::sort(albums.begin(), albums.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(QStringLiteral("name")).toString().compare(b.value(QStringLiteral("name")).toString(), Qt::CaseInsensitive) < 0;
    });
    m_pictureAlbums.clear();
    for (const QVariantMap &a : albums) m_pictureAlbums.append(a);
    emit pictureAlbumsChanged();
    // keep an open album in step with a rescan; if its folder is gone, go back to the list
    if (!m_pictureAlbum.isEmpty()) {
        if (byDir.contains(m_pictureAlbum)) m_albumItems.setItems(byDir.value(m_pictureAlbum));
        else closePictureAlbum();
    }
}

void App::openPictureAlbum(const QString &dir)
{
    QList<SectionItem> mine;
    for (const SectionItem &it : m_sec[0]->model.items())
        if (QFileInfo(it.path).absolutePath() == dir) mine.append(it);
    if (mine.isEmpty()) return;
    m_albumItems.setItems(mine);
    m_pictureAlbum = dir;
    emit pictureAlbumChanged();
}

void App::renamePictureAlbum(const QString &dir, const QString &name)
{
    if (m_guest) return;
    const QString n = name.simplified().left(60);
    if (n.isEmpty()) { if (!m_albumNames.remove(dir)) return; }
    else if (m_albumNames.value(dir) == n) return;
    else m_albumNames.insert(dir, n);
    saveProfileSettings();
    rebuildPictureAlbums(m_sec[0]->model.items());
}

void App::closePictureAlbum()
{
    if (m_pictureAlbum.isEmpty()) return;
    m_pictureAlbum.clear();
    m_albumItems.setItems({});
    emit pictureAlbumChanged();
}

QStringList App::videoCategories()
{
    return {QStringLiteral("movies"), QStringLiteral("shows"), QStringLiteral("cartoons"), QStringLiteral("anime"), QStringLiteral("other")};
}

QStringList App::allVideoFolders() const
{
    QStringList all = videoFolders();
    for (const QString &c : videoCategories())
        for (const QString &f : m_videoCatFolders.value(c)) if (!all.contains(f)) all << f;
    return all;
}

QVariantMap App::videoCategoryFolders() const
{
    QVariantMap out;
    for (const QString &c : videoCategories())
        out.insert(c, c == QLatin1String("movies") ? videoFolders() : m_videoCatFolders.value(c));
    return out;
}

// A video belongs to the category whose folder is its closest parent; anything else that is scanned is a movie.
QString App::categoryOfVideo(const QString &path) const
{
    QString best = QStringLiteral("movies");
    int bestLen = -1;
    for (const QString &c : videoCategories()) {
        const QStringList fs = c == QLatin1String("movies") ? videoFolders() : m_videoCatFolders.value(c);
        for (const QString &f : fs) {
            QString d = f;
            while (d.size() > 1 && d.endsWith(QLatin1Char('/'))) d.chop(1);
            if (path.startsWith(d + QLatin1Char('/')) && d.size() > bestLen) { best = c; bestLen = d.size(); }
        }
    }
    return best;
}

void App::setVideoCategory(const QString &c)
{
    if (!videoCategories().contains(c) || c == m_videoCategory) return;
    m_videoCategory = c;
    if (!m_detailPath.isEmpty()) closeVideoDetail();
    emit videoCategoryChanged();
    reloadVideos();
}

void App::addVideoCategoryFolder(const QString &category, const QUrl &folder)
{
    if (category == QLatin1String("movies")) { addVideoFolder(folder); return; }
    if (!videoCategories().contains(category)) return;
    const QString path = QFileInfo(Paths::folderPath(folder)).canonicalFilePath();
    if (path.isEmpty() || m_videoCatFolders.value(category).contains(path)) return;
    m_videoCatFolders[category] << path;
    saveFolders();
    emit foldersChanged();
    rescan();
}

void App::removeVideoCategoryFolder(const QString &category, const QString &path)
{
    if (category == QLatin1String("movies")) { removeVideoFolder(path); return; }
    if (!m_videoCatFolders[category].removeAll(path)) return;
    saveFolders();
    emit foldersChanged();
    reloadVideos();
}

void App::removeVideoFolder(const QString &path)
{
    if (!m_videoFoldersOwn) { m_videoFolders = m_folders; m_videoFoldersOwn = true; }
    if (!m_videoFolders.removeAll(path)) return;
    saveFolders();
    emit foldersChanged();
    reloadVideos();
}

QStringList App::watchedFolders() const
{
    QStringList all = m_folders;
    for (const QString &f : allVideoFolders()) if (!all.contains(f)) all << f;
    for (const auto &sec : m_sec)
        for (const QString &f : sec->folders) if (!all.contains(f)) all << f;
    return all;
}

void App::rescan()
{
    for (auto &sec : m_sec) {
        if (sec->folders.isEmpty()) continue;
        if (sec->scanner.isRunning()) sec->rescanPending = true;
        else { sec->done = sec->total = 0; sec->scanner.start(sec->folders); }
    }
    if (m_folders.isEmpty() && allVideoFolders().isEmpty()) { m_watcher.setFolders(watchedFolders()); return; }
    if (!m_folders.isEmpty()) {
        if (m_scanner.isRunning()) m_rescanPending = true;   // e.g. right after a profile switch
        else {
            m_scanDone = m_scanTotal = 0;
            emit scanProgressChanged();
            m_scanner.start(m_folders);
        }
    }
    if (!allVideoFolders().isEmpty()) {
        if (m_videoScanner.isRunning()) m_videoScanRescanPending = true;
        else {
            m_videoScanDone = m_videoScanTotal = 0;
            emit videoScanProgressChanged();
            m_videoScanner.start(allVideoFolders());
        }
    }
    m_watcher.setFolders(watchedFolders());
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
    if (!m_songOrder.isEmpty()) {
        QHash<QString, Track> byPath;
        for (const Track &t : m_library) byPath.insert(t.path, t);
        QList<Track> reordered;
        QSet<QString> placed;
        for (const QString &p : m_songOrder) {
            const auto it = byPath.constFind(p);
            if (it == byPath.constEnd()) continue;
            reordered.append(it.value());
            placed.insert(p);
        }
        for (const Track &t : m_library)
            if (!placed.contains(t.path)) reordered.append(t);
        m_library = reordered;
    }
    m_libraryIndex.clear();
    for (int i = 0; i < m_library.size(); ++i) m_libraryIndex.insert(m_library[i].path, i);
    m_fullCollection.build(m_library);
    m_mosaicArt.clear();
    for (const Album &a : m_fullCollection.albums())
        if (!a.art.isEmpty()) m_mosaicArt << QUrl::fromLocalFile(a.art).toString();
    applySearch();
    refreshPlaylist();
    refreshQuickplay();
    reloadVideos();
    reloadSections();
}

void App::reloadVideos()
{
    QList<Video> all = m_videoIndex.videosUnder(allVideoFolders());
    QList<Video> list;
    for (Video &v : all) {
        if (categoryOfVideo(v.path) != m_videoCategory) continue;
        list.append(v);
    }
    for (Video &v : list) {
        v.customTitle = m_videoMeta.value(v.path).value(QStringLiteral("title")).toString();
        const auto it = m_videoThumbOverrides.constFind(v.path);
        if (it == m_videoThumbOverrides.constEnd() || !QFile::exists(it.value())) continue;
        v.thumb = it.value();
        v.customThumb = true;
    }
    m_videos.setVideos(list);
    if (!m_detailPath.isEmpty()) emit videoDetailChanged();   // a picked poster, a new scan
}

QString App::setVideoThumb(int row, const QUrl &image)
{
    if (!profileReady() || m_guest) return QStringLiteral("no profile open");
    if (row < 0 || row >= m_videos.count()) return QStringLiteral("no video selected");
    const QString videoPath = m_videos.videos()[row].path;
    QImage img(image.isLocalFile() ? image.toLocalFile() : image.toString());
    if (img.isNull()) return QStringLiteral("that file is not a picture hop-tunes can read");
    if (img.width() > 1280 || img.height() > 1280)
        img = img.scaled(1280, 1280, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QString dir = m_store.dirFor(m_profileId) + QStringLiteral("/video-thumbs");
    QDir().mkpath(dir);
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(videoPath.toUtf8(), QCryptographicHash::Sha1).toHex());
    const QString path = dir + QLatin1Char('/') + hash + QStringLiteral(".jpg");
    if (!img.save(path, "JPG", 85)) return QStringLiteral("could not save the picture");
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!m_db.setVideoThumbOverride(videoPath, path)) return QStringLiteral("could not save the change");
    m_videoThumbOverrides[videoPath] = path;
    reloadVideos();
    return {};
}

void App::clearVideoThumb(int row)
{
    if (!profileReady() || m_guest || row < 0 || row >= m_videos.count()) return;
    const QString videoPath = m_videos.videos()[row].path;
    if (!m_db.setVideoThumbOverride(videoPath, QString())) return;
    QFile::remove(m_videoThumbOverrides.take(videoPath));
    reloadVideos();
}

QString App::videoPlayerTitle() const
{
    if (m_videoPlayerPath.isEmpty()) return {};
    const QString mine = m_videoMeta.value(m_videoPlayerPath).value(QStringLiteral("title")).toString();
    return mine.isEmpty() ? QFileInfo(m_videoPlayerPath).completeBaseName() : mine;
}

QString App::videoEngine() const
{
#ifdef HOPTUNES_HAVE_MPV
    return QStringLiteral("mpv");
#else
    return GstVideoPlayer::available() ? QStringLiteral("gstreamer") : QStringLiteral("none");
#endif
}

void App::playVideo(int row)
{
    if (row < 0 || row >= m_videos.count()) return;
    openVideoFile(m_videos.videos()[row].path);
}

void App::openVideoFile(const QString &path)
{
    m_videoPlayerPath = path;
    m_videoPlayerResumeMs = m_db.isOpen() ? m_db.videoProgress(path).positionMs : 0;
    m_videoPlayerOpen = true;
    emit videoPlayerChanged(); emit myVideoCommentsChanged();
}

void App::switchVideo(int row, qint64 positionMs, qint64 durationMs)
{
    if (row < 0 || row >= m_videos.count()) return;
    saveVideoProgress(positionMs, durationMs);
    openVideoFile(m_videos.videos()[row].path);
}

void App::setNavStyle(const QString &style)
{
    const QString v = style == QLatin1String("drawer") ? QStringLiteral("drawer") : QStringLiteral("bar");
    if (v == m_navStyle) return;
    m_navStyle = v;
    saveProfileSettings();
    emit navStyleChanged();
}

void App::setStatusNote(const QString &text)
{
    const QString t = text.left(300);
    if (t == m_statusNote) return;
    m_statusNote = t;
    saveProfileSettings();   // a guest keeps nothing, like every other setting
    emit statusNoteChanged();
}

void App::setVideoVolume(int percent)
{
    const int v = qBound(0, percent, 150);
    if (v == m_videoVolume) return;
    m_videoVolume = v;
    saveProfileSettings();
    emit videoVolumeChanged();
}

void App::setVideoChat(bool on)
{
    if (on == m_videoChat) return;
    m_videoChat = on;
    saveProfileSettings();
    emit videoChatChanged();
}

void App::setVideoComments(const QString &mode)
{
    const QString m = mode == QLatin1String("mine") || mode == QLatin1String("populated") ? mode : QStringLiteral("off");
    if (m == m_videoComments) return;
    m_videoComments = m;
    saveProfileSettings();
    emit videoCommentsChanged();
}

QVariantList App::myVideoComments() const
{
    QVariantList out;
    if (m_videoPlayerPath.isEmpty()) return out;
    for (const VideoComment &c : m_db.videoComments(m_videoPlayerPath))
        out.append(QVariantMap{{QStringLiteral("id"), c.id}, {QStringLiteral("text"), c.text}, {QStringLiteral("at"), c.at}});
    return out;
}

void App::addVideoComment(const QString &text)
{
    const QString t = text.trimmed().left(500);
    if (t.isEmpty() || m_videoPlayerPath.isEmpty() || m_videoComments == QLatin1String("off")) return;
    if (m_db.addVideoComment(m_videoPlayerPath, t, QDateTime::currentMSecsSinceEpoch())) emit myVideoCommentsChanged();
}

void App::removeVideoComment(qint64 id)
{
    if (m_db.removeVideoComment(id)) emit myVideoCommentsChanged();
}

void App::setVideoChatLook(const QString &look)
{
    const QString l = look == QLatin1String("lively") ? look : QStringLiteral("clean");
    if (l == m_videoChatLook) return;
    m_videoChatLook = l;
    saveProfileSettings();
    emit videoChatChanged();
}

void App::setVideoViewStyle(const QString &style)
{
    const QString s = (style == QLatin1String("theater") || style == QLatin1String("fullscreen")) ? style : QStringLiteral("window");
    if (s == m_videoViewStyle) return;
    m_videoViewStyle = s;
    saveProfileSettings();
    emit videoViewStyleChanged();
}

void App::closeVideoPlayer(qint64 positionMs, qint64 durationMs)
{
    saveVideoProgress(positionMs, durationMs);
    m_videoPlayerOpen = false;
    m_videoPlayerPath.clear();
    emit videoPlayerChanged(); emit myVideoCommentsChanged();
}

void App::saveVideoProgress(qint64 positionMs, qint64 durationMs)
{
    if (m_db.isOpen() && !m_videoPlayerPath.isEmpty() && positionMs > 0) {
        m_db.setVideoProgress(m_videoPlayerPath, positionMs, durationMs);
        // Watched to (nearly) the end: remember it as watched; the resume position is forgotten then.
        if (durationMs > 0 && positionMs >= durationMs - 5000 && !m_played.contains(m_videoPlayerPath)) {
            m_played.insert(m_videoPlayerPath);
            m_db.setItemPlayed(m_videoPlayerPath, true);
        }
        if (m_videoPlayerPath == m_detailPath) emit videoDetailChanged();
    }
}

// ---- movie details: filled in by the user, saved per profile ------------------------------------------

namespace {

const QStringList kVideoFields = {QStringLiteral("title"),    QStringLiteral("year"),     QStringLiteral("rating"),
                                  QStringLiteral("score"),    QStringLiteral("tagline"),  QStringLiteral("overview"),
                                  QStringLiteral("genre"),    QStringLiteral("director"), QStringLiteral("writers"),
                                  QStringLiteral("studio"),   QStringLiteral("tags")};

QString hoursMinutes(qint64 ms)
{
    const qint64 mins = (ms + 30'000) / 60'000;
    if (mins >= 60) return QStringLiteral("%1h %2m").arg(mins / 60).arg(mins % 60);
    return QStringLiteral("%1m").arg(qMax<qint64>(1, mins));
}

} // namespace

int App::videoDetailRow() const
{
    if (m_detailPath.isEmpty()) return -1;
    const QList<Video> &vs = m_videos.videos();
    for (int i = 0; i < vs.size(); ++i) if (vs[i].path == m_detailPath) return i;
    return -1;
}

QVariantMap App::buildVideoDetail(const QString &path) const
{
    QVariantMap d;
    if (path.isEmpty()) return d;
    const QList<Video> &vs = m_videos.videos();
    int row = -1;
    for (int i = 0; i < vs.size(); ++i) if (vs[i].path == path) { row = i; break; }
    const Video *v = row >= 0 ? &vs[row] : nullptr;   // a file opened from outside the library has no row
    d[QStringLiteral("row")] = row;
    d[QStringLiteral("path")] = path;
    d[QStringLiteral("fileName")] = v ? v->displayTitle() : QFileInfo(path).completeBaseName();
    d[QStringLiteral("thumb")] = v && !v->thumb.isEmpty() ? QUrl::fromLocalFile(v->thumb).toString() : QString();
    d[QStringLiteral("customThumb")] = v && v->customThumb;
    d[QStringLiteral("resolution")] = v ? VideoListModel::resolutionBadge(v->width, v->height) : QString();
    d[QStringLiteral("width")] = v ? v->width : 0;
    d[QStringLiteral("height")] = v ? v->height : 0;
    d[QStringLiteral("codec")] = v ? v->codec.toUpper() : QString();
    d[QStringLiteral("hdr")] = v && v->hdr;
    d[QStringLiteral("container")] = v ? v->container.toUpper() : QString();
    d[QStringLiteral("length")] = v && v->durationMs > 0 ? hoursMinutes(v->durationMs) : QString();
    d[QStringLiteral("size")] = v ? SectionListModel::formatSize(v->size) : QString();
    const VideoProgress p = m_db.isOpen() ? m_db.videoProgress(path) : VideoProgress{};
    d[QStringLiteral("resumeText")] = p.positionMs > 0 ? TrackListModel::formatDuration(p.positionMs) : QString();
    d[QStringLiteral("played")] = m_played.contains(path);

    const QJsonObject mo = m_videoMeta.value(path);
    QVariantMap meta;
    for (const QString &k : kVideoFields) meta[k] = mo.value(k).toString();
    d[QStringLiteral("meta")] = meta;
    QVariantList cast;
    for (const QJsonValue &c : mo.value(QStringLiteral("cast")).toArray()) {
        const QJsonObject co = c.toObject();
        const QString photo = co.value(QStringLiteral("photo")).toString();
        cast.append(QVariantMap{{QStringLiteral("name"), co.value(QStringLiteral("name")).toString()},
                                {QStringLiteral("role"), co.value(QStringLiteral("role")).toString()},
                                {QStringLiteral("photo"), photo.isEmpty() || !QFile::exists(photo) ? QString() : QUrl::fromLocalFile(photo).toString()}});
    }
    d[QStringLiteral("cast")] = cast;
    return d;
}

void App::openVideoDetail(int row)
{
    if (row < 0 || row >= m_videos.count()) return;
    m_detailPath = m_videos.videos()[row].path;
    emit videoDetailChanged();
}

void App::closeVideoDetail()
{
    if (m_detailPath.isEmpty()) return;
    m_detailPath.clear();
    emit videoDetailChanged();
}

// Writes this video's details; an object with nothing filled in is removed altogether.
void App::saveVideoMeta(const QString &path)
{
    QJsonObject &o = m_videoMeta[path];
    for (const QString &k : kVideoFields) if (o.value(k).toString().trimmed().isEmpty()) o.remove(k);
    if (o.value(QStringLiteral("cast")).toArray().isEmpty()) o.remove(QStringLiteral("cast"));
    if (o.isEmpty()) {
        m_videoMeta.remove(path);
        if (m_db.isOpen()) m_db.setVideoMeta(path, QString());
    } else if (m_db.isOpen()) {
        m_db.setVideoMeta(path, QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
    }
    emit videoDetailChanged();
    if (path == m_videoPlayerPath) emit videoPlayerChanged();   // the player shows these details too
}

void App::setVideoField(const QString &key, const QString &value)
{
    if (m_detailPath.isEmpty() || !kVideoFields.contains(key)) return;
    const QString v = value.trimmed();
    if (m_videoMeta.value(m_detailPath).value(key).toString() == v) return;   // nothing changed: no write
    m_videoMeta[m_detailPath].insert(key, v);
    saveVideoMeta(m_detailPath);
    if (key == QLatin1String("title")) reloadVideos();   // the grid shows the title typed here
}

void App::addCastMember()
{
    if (m_detailPath.isEmpty()) return;
    QJsonObject &o = m_videoMeta[m_detailPath];
    QJsonArray cast = o.value(QStringLiteral("cast")).toArray();
    cast.append(QJsonObject{{QStringLiteral("name"), QString()}, {QStringLiteral("role"), QString()}});
    o.insert(QStringLiteral("cast"), cast);
    // An empty row is kept so the user can type into it; it is dropped again only by "remove".
    if (m_db.isOpen()) m_db.setVideoMeta(m_detailPath, QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
    emit videoDetailChanged();
}

void App::setCastMember(int index, const QString &name, const QString &role)
{
    if (m_detailPath.isEmpty()) return;
    QJsonObject &o = m_videoMeta[m_detailPath];
    QJsonArray cast = o.value(QStringLiteral("cast")).toArray();
    if (index < 0 || index >= cast.size()) return;
    QJsonObject c = cast[index].toObject();
    if (c.value(QStringLiteral("name")).toString() == name.trimmed() && c.value(QStringLiteral("role")).toString() == role.trimmed()) return;
    c[QStringLiteral("name")] = name.trimmed();
    c[QStringLiteral("role")] = role.trimmed();
    cast[index] = c;
    o.insert(QStringLiteral("cast"), cast);
    if (m_db.isOpen()) m_db.setVideoMeta(m_detailPath, QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
    emit videoDetailChanged();
}

void App::removeCastMember(int index)
{
    if (m_detailPath.isEmpty()) return;
    QJsonObject &o = m_videoMeta[m_detailPath];
    QJsonArray cast = o.value(QStringLiteral("cast")).toArray();
    if (index < 0 || index >= cast.size()) return;
    const QString photo = cast[index].toObject().value(QStringLiteral("photo")).toString();
    if (!photo.isEmpty()) QFile::remove(photo);
    cast.removeAt(index);
    o.insert(QStringLiteral("cast"), cast);
    saveVideoMeta(m_detailPath);
}

QString App::setCastPhoto(int index, const QUrl &image)
{
    if (!profileReady() || m_guest) return QStringLiteral("no profile open");
    if (m_detailPath.isEmpty()) return QStringLiteral("no movie open");
    QJsonObject &o = m_videoMeta[m_detailPath];
    QJsonArray cast = o.value(QStringLiteral("cast")).toArray();
    if (index < 0 || index >= cast.size()) return QStringLiteral("no such person");
    QImage img(image.isLocalFile() ? image.toLocalFile() : image.toString());
    if (img.isNull()) return QStringLiteral("that file is not a picture hop-tunes can read");
    if (img.width() > 600 || img.height() > 600) img = img.scaled(600, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QString dir = m_store.dirFor(m_profileId) + QStringLiteral("/video-cast");
    QDir().mkpath(dir);
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(m_detailPath.toUtf8(), QCryptographicHash::Sha1).toHex());
    const QString path = dir + QLatin1Char('/') + hash + QLatin1Char('_') + QString::number(QDateTime::currentMSecsSinceEpoch()) + QStringLiteral(".jpg");
    if (!img.save(path, "JPG", 85)) return QStringLiteral("could not save the picture");
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QJsonObject c = cast[index].toObject();
    const QString old = c.value(QStringLiteral("photo")).toString();
    if (!old.isEmpty()) QFile::remove(old);
    c[QStringLiteral("photo")] = path;
    cast[index] = c;
    o.insert(QStringLiteral("cast"), cast);
    if (m_db.isOpen()) m_db.setVideoMeta(m_detailPath, QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
    emit videoDetailChanged();
    return {};
}

void App::toggleVideoWatched()
{
    if (m_detailPath.isEmpty() || !m_db.isOpen()) return;
    const bool now = !m_played.contains(m_detailPath);
    if (now) {
        m_played.insert(m_detailPath);
        m_db.setVideoProgress(m_detailPath, 1, 1);   // at the end: drops any resume position
    } else {
        m_played.remove(m_detailPath);
    }
    m_db.setItemPlayed(m_detailPath, now);
    emit videoDetailChanged();
}

void App::applySearch()
{
    // The raw (never-overridden) title, so re-selection below matches correctly even when
    // this album has a profile display-title override (see applyAlbumOverrides).
    const Album *keptAlbum = m_artistAlbums.at(m_selAlbum);
    const QString keepArtist = selectedArtistName(), keepAlbum = keptAlbum ? keptAlbum->title : QString();
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
    applyAlbumOverrides(all);
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
    const QStringList order = m_db.albumTrackOrder(a.artist + kRefSep + a.title);
    if (order.isEmpty()) return out;
    // Profile's manual order first (paths that still exist), then anything new or missing
    // from that saved order, in its natural (tag-derived) position — never dropped.
    QHash<QString, Track> byPath;
    for (const Track &t : out) byPath.insert(t.path, t);
    QList<Track> reordered;
    QSet<QString> placed;
    for (const QString &p : order) {
        const auto it = byPath.constFind(p);
        if (it == byPath.constEnd()) continue;
        reordered.append(it.value());
        placed.insert(p);
    }
    for (const Track &t : out)
        if (!placed.contains(t.path)) reordered.append(t);
    return reordered;
}

void App::applyAlbumOverrides(QList<Album> &albums) const
{
    if (m_albumOverrides.isEmpty()) return;
    for (Album &a : albums) {
        const auto it = m_albumOverrides.constFind(a.artist + kRefSep + a.title);
        if (it == m_albumOverrides.constEnd()) continue;
        a.displayTitle = it->title;
        a.displayArt = it->art;
    }
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
    applyAlbumOverrides(albums);
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
    if (!a) return {};
    return a->displayTitle.isEmpty() ? a->title : a->displayTitle;
}

QString App::selectedAlbumArt() const
{
    const Album *a = m_artistAlbums.at(m_selAlbum);
    if (!a) return {};
    const QString art = a->displayArt.isEmpty() ? a->art : a->displayArt;
    return art.isEmpty() ? QString() : QUrl::fromLocalFile(art).toString();
}

int App::selectedAlbumYear() const
{
    const Album *a = m_artistAlbums.at(m_selAlbum);
    return a ? a->year : 0;
}

void App::saveFolders() { saveProfileSettings(); m_watcher.setFolders(watchedFolders()); }

// ---- listening stats ----------------------------------------------------------------
// A play counts once half the song (or 4 minutes) has been heard; moving on before that
// counts as a skip. Hearts and counts feed smart playlists and the smart DJ.

void App::saveResume(bool force)
{
    if (m_resumePath.isEmpty() || !m_db.isOpen() || m_guest) return;
    if (!force && qAbs(m_resumeLastMs - m_resumeSavedMs) < 5000) return;
    m_resumeSavedMs = m_resumeLastMs;
    m_db.setVideoProgress(m_resumePath, m_resumeLastMs, m_resumeDurationMs);   // the same per-path resume table videos use
    const double frac = m_resumeDurationMs > 0 && m_resumeLastMs < m_resumeDurationMs - 5000
        ? double(m_resumeLastMs) / double(m_resumeDurationMs) : 0.0;
    for (auto &sec : m_sec) sec->model.setProgress(m_resumePath, frac);
    // Reaching the end counts as played; the resume position is forgotten at that point.
    if (m_resumeDurationMs > 0 && m_resumeLastMs >= m_resumeDurationMs - 5000 && !m_played.contains(m_resumePath)) {
        m_played.insert(m_resumePath);
        m_db.setItemPlayed(m_resumePath, true);
        for (auto &sec : m_sec) sec->model.setPlayed(m_resumePath, true);
    }
}

void App::toggleItemPlayed(const QString &path)
{
    if (path.isEmpty() || !m_db.isOpen() || m_guest) return;
    const bool now = !m_played.contains(path);
    if (now) m_played.insert(path); else m_played.remove(path);
    m_db.setItemPlayed(path, now);
    for (auto &sec : m_sec) sec->model.setPlayed(path, now);
}

// The tracked item stopped being the current track (or the profile is closing): save and forget it.
void App::flushResume()
{
    saveResume(true);
    m_resumePath.clear();
    m_resumeSavedMs = m_resumeLastMs = m_resumeDurationMs = 0;
}

void App::onPosition()
{
    if (!m_resumePath.isEmpty()) {
        const Track *cur = m_player.currentTrack();
        if (cur && cur->path == m_resumePath) {
            m_resumeLastMs = m_player.position();
            m_resumeDurationMs = m_player.duration();
            saveResume(false);
        }
    }
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
    if (!m_resumePath.isEmpty()) {
        const Track *cur = m_player.currentTrack();
        if (!cur || cur->path != m_resumePath) flushResume();
    }
    if (!m_listen.path.isEmpty() && !m_listen.counted && m_listen.maxPos >= 1000) {
        m_db.recordSkip(m_listen.path);
        m_stats[m_listen.path].skipCount++;
    }
    const Track *t = m_player.currentTrack();
    m_listen = t ? Listen{t->path, 0, t->durationMs, false} : Listen{};
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

// ---- per-profile album overrides (never seen by another profile) --------------------

QString App::setAlbumArt(const QString &ref, const QUrl &image)
{
    if (!profileReady() || m_guest) return QStringLiteral("no profile open");
    if (ref.isEmpty()) return QStringLiteral("no album selected");
    QImage img(image.isLocalFile() ? image.toLocalFile() : image.toString());
    if (img.isNull()) return QStringLiteral("that file is not a picture hop-tunes can read");
    // Bound the file size; AlbumArt.qml crops/scales for display regardless of aspect ratio.
    if (img.width() > 1200 || img.height() > 1200)
        img = img.scaled(1200, 1200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QString dir = m_store.dirFor(m_profileId) + QStringLiteral("/album-art");
    QDir().mkpath(dir);
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(ref.toUtf8(), QCryptographicHash::Sha1).toHex());
    const QString path = dir + QLatin1Char('/') + hash + QStringLiteral(".jpg");
    if (!img.save(path, "JPG", 85)) return QStringLiteral("could not save the picture");
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!m_db.setAlbumOverrideArt(ref, path)) return QStringLiteral("could not save the change");
    m_albumOverrides[ref].art = path;
    applySearch();
    return {};
}

QString App::setAlbumTitle(const QString &ref, const QString &title)
{
    if (!profileReady() || m_guest) return QStringLiteral("no profile open");
    if (ref.isEmpty()) return QStringLiteral("no album selected");
    // Clears the override (back to the tagged title) when left blank.
    if (!m_db.setAlbumOverrideTitle(ref, title.trimmed())) return QStringLiteral("could not save the change");
    m_albumOverrides[ref].title = title.trimmed();
    applySearch();
    return {};
}

bool App::moveAlbumSong(int from, int to)
{
    const Album *a = m_artistAlbums.at(m_selAlbum);
    if (!a) return false;
    QList<Track> tracks = m_albumSongs.tracks();
    if (from < 0 || to < 0 || from >= tracks.size() || to >= tracks.size()) return false;
    tracks.move(from, to);
    // Reflect the new order right away regardless of whether it can be saved (guest mode
    // saves nothing, same as everywhere else — the reorder still works for this session).
    m_albumSongs.setTracks(tracks);
    if (profileReady() && !m_guest) {
        QStringList order;
        for (const Track &t : tracks) order << t.path;
        m_db.setAlbumTrackOrder(a->artist + kRefSep + a->title, order);
    }
    return true;
}

bool App::moveSong(int from, int to)
{
    QList<Track> visible = m_songs.tracks();
    if (from < 0 || to < 0 || from >= visible.size() || to >= visible.size()) return false;
    visible.move(from, to);
    // The songs tab can be search-filtered: reinsert the (possibly filtered) visible list in
    // its new order and leave every hidden track's relative position alone, the same trick
    // App::movePlaylistItem() uses for items missing from its current view.
    QSet<QString> seen;
    for (const Track &t : visible) seen.insert(t.path);
    QList<Track> reordered = visible;
    for (const Track &t : m_library)
        if (!seen.contains(t.path)) reordered.append(t);
    m_library = reordered;
    if (profileReady() && !m_guest) {
        QStringList order;
        for (const Track &t : m_library) order << t.path;
        m_db.setSongOrder(order);
        m_songOrder = order;
    }
    // Unlike moveAlbumSong(), this reorders the whole library, so artists/albums grouped
    // from it (and their trackRows into m_songs) need rebuilding too — applySearch() does
    // that and keeps the current selection, without touching the player's own queue.
    applySearch();
    return true;
}

QVariantMap App::tile(const QString &kind, const QString &ref, const QString &label) const
{
    QVariantMap m{{QStringLiteral("kind"), kind}, {QStringLiteral("ref"), ref}, {QStringLiteral("title"), label},
                  {QStringLiteral("subtitle"), QString()}, {QStringLiteral("art"), QString()},
                  {QStringLiteral("pinned"), isPinned(kind, ref)}};
    if (kind == QLatin1String("album")) {
        const QString artist = ref.section(kRefSep, 0, 0), title = ref.section(kRefSep, 1);
        const auto ov = m_albumOverrides.constFind(ref);
        const bool hasOv = ov != m_albumOverrides.constEnd();
        m[QStringLiteral("title")] = (hasOv && !ov->title.isEmpty()) ? ov->title
                                    : (title.isEmpty() ? QStringLiteral("unknown album") : title);
        m[QStringLiteral("subtitle")] = artist;
        for (const Album &a : m_fullCollection.albums())
            if (a.artist == artist && a.title == title) {
                const QString art = (hasOv && !ov->art.isEmpty()) ? ov->art : a.art;
                if (!art.isEmpty()) m[QStringLiteral("art")] = QUrl::fromLocalFile(art).toString();
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

// ---- opening files -------------------------------------------------------------------

void App::openUri(const QString &uri)
{
    m_openBatch << uri;
    m_openTimer.start();
}

void App::openFiles(const QStringList &pathsOrUris)
{
    if (pathsOrUris.isEmpty()) return;
    if (!profileReady()) { m_pendingOpen += pathsOrUris; return; }
    QList<Track> q;
    QString firstVideo;   // the video player shows one video, not a queue (spec §5b)
    auto add = [this, &q, &firstVideo](const QFileInfo &fi) {
        if (!fi.isFile()) return;
        if (TagReader::isAudioFile(fi)) {
            const QString path = fi.canonicalFilePath();
            const int i = m_libraryIndex.value(path, -1);
            if (i >= 0) { q.append(m_library[i]); return; }
            Track t = TagReader::read(QFileInfo(path));
            t.path = path;
            q.append(t);
        } else if (firstVideo.isEmpty() && VideoProbe::isVideoFile(fi)) {
            firstVideo = fi.canonicalFilePath();
        }
    };
    for (const QString &item : pathsOrUris) {
        const QString path = item.startsWith(QLatin1String("file:"), Qt::CaseInsensitive) ? QUrl(item).toLocalFile() : item;
        const QFileInfo fi(path);
        if (fi.isDir()) {
            QStringList files;
            QDirIterator it(fi.absoluteFilePath(), QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
            while (it.hasNext()) files << it.next();
            files.sort(Qt::CaseInsensitive);
            for (const QString &f : files) add(QFileInfo(f));
        } else {
            add(fi);
        }
    }
    if (!firstVideo.isEmpty()) openVideoFile(firstVideo);
    if (q.isEmpty()) return;
    m_djSeed.clear();
    emit djChanged();
    m_player.setQueue(q, 0);
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
