// SPDX-License-Identifier: GPL-3.0-or-later
#include "App.h"

#include "core/Paths.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QImage>
#include <QJsonArray>
#include <QFileInfo>
#include <QRandomGenerator>
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
    emit profileChanged();
    emit foldersChanged();
    reload();
    rescan();
}

void App::leaveProfile()
{
    if (!profileReady()) return;
    saveProfileSettings();
    m_player.setQueue({}, 0);
    m_scanner.cancel();
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
    m_player.setQueue({}, 0);
    m_scanner.cancel();
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

void App::saveFolders() { saveProfileSettings(); }

} // namespace hop
