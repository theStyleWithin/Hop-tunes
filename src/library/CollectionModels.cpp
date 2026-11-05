// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/CollectionModels.h"

#include <QHash>
#include <QSet>
#include <QUrl>

#include <algorithm>

namespace hop {

QString ArtistMap::resolve(const QString &artist, const QString &album) const
{
    QString out = artist;
    const QString albumKey = album.toCaseFolded();
    QSet<QString> seen;
    for (int step = 0; step < 32; ++step) {
        const QString key = out.toCaseFolded();
        if (seen.contains(key)) break;   // a loop: stop where we are
        seen.insert(key);
        const auto mv = albumMoves.constFind(key + QLatin1Char('\x1f') + albumKey);
        if (mv != albumMoves.constEnd()) { out = mv.value(); continue; }   // this one album goes elsewhere
        const auto it = merges.constFind(key);
        if (it == merges.constEnd()) break;
        out = it.value();
    }
    return out;
}

void Collection::build(const QList<Track> &tracks, const ArtistMap &map)
{
    m_albums.clear();
    m_artists.clear();

    QHash<QString, int> albumIndex;   // "artist\x1falbum" -> index in m_albums
    QSet<int> winnerArt;              // albums whose cover already came from the receiving artist
    for (int row = 0; row < tracks.size(); ++row) {
        const Track &t = tracks[row];
        const QString ownArtist = t.albumArtist.isEmpty() ? t.displayArtist() : t.albumArtist;
        const QString title = t.displayAlbum();
        const QString artist = map.isEmpty() ? ownArtist : map.resolve(ownArtist, title);
        const bool fromWinner = artist.compare(ownArtist, Qt::CaseInsensitive) == 0;
        const QString key = artist.toCaseFolded() + QLatin1Char('\x1f') + title.toCaseFolded();
        auto it = albumIndex.constFind(key);
        if (it == albumIndex.constEnd()) {
            it = albumIndex.insert(key, m_albums.size());
            m_albums.append(Album{title, artist, 0, {}, {}, {}, {}});
        }
        Album &a = m_albums[*it];
        a.trackRows.append(row);
        a.year = std::max(a.year, t.year);
        if (a.art.isEmpty()) a.art = t.art;
        else if (fromWinner && !t.art.isEmpty() && !winnerArt.contains(*it)) a.art = t.art;   // the receiving artist's cover wins
        if (fromWinner && !t.art.isEmpty()) winnerArt.insert(*it);
    }

    // Albums within an artist: newest first (like a discography), untagged years last.
    QHash<QString, int> artistIndex;
    for (int i = 0; i < m_albums.size(); ++i) {
        const QString key = m_albums[i].artist.toCaseFolded();
        auto it = artistIndex.constFind(key);
        if (it == artistIndex.constEnd()) {
            it = artistIndex.insert(key, m_artists.size());
            m_artists.append(Artist{m_albums[i].artist, {}, 0, {}});
        }
        Artist &ar = m_artists[*it];
        ar.albumIndices.append(i);
        ar.songCount += m_albums[i].trackRows.size();
    }
    for (Artist &ar : m_artists) {
        std::stable_sort(ar.albumIndices.begin(), ar.albumIndices.end(), [this](int x, int y) {
            const int yx = m_albums[x].year, yy = m_albums[y].year;
            if ((yx == 0) != (yy == 0)) return yy == 0;
            return yx > yy;
        });
        for (int i : ar.albumIndices)
            if (!m_albums[i].art.isEmpty()) { ar.art = m_albums[i].art; break; }
    }
    std::stable_sort(m_artists.begin(), m_artists.end(), [](const Artist &x, const Artist &y) {
        auto key = [](const QString &n) {
            QString k = n.toCaseFolded();
            if (k.startsWith(QLatin1String("the "))) k = k.mid(4);   // "The Lanternists" under L
            return k;
        };
        const bool ux = x.name == QLatin1String("unknown artist"), uy = y.name == QLatin1String("unknown artist");
        if (ux != uy) return uy;
        return key(x.name).localeAwareCompare(key(y.name)) < 0;
    });
}

static QString artUrl(const QString &path)
{
    return path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
}

QVariant ArtistListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size()) return {};
    const Artist &a = m_items[index.row()];
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:       return a.name;
    case AlbumCountRole: return int(a.albumIndices.size());
    case SongCountRole:  return a.songCount;
    case ArtRole:        return artUrl(a.art);
    }
    return {};
}

QHash<int, QByteArray> ArtistListModel::roleNames() const
{
    return {{NameRole, "name"}, {AlbumCountRole, "albumCount"}, {SongCountRole, "songCount"}, {ArtRole, "art"}};
}

void ArtistListModel::setArtists(const QList<Artist> &artists)
{
    beginResetModel();
    m_items = artists;
    endResetModel();
    emit countChanged();
}

QVariant AlbumListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size()) return {};
    const Album &a = m_items[index.row()];
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:     return a.displayTitle.isEmpty() ? a.title : a.displayTitle;
    case ArtistRole:    return a.artist;
    case YearRole:      return a.year;
    case ArtRole:       return artUrl(a.displayArt.isEmpty() ? a.art : a.displayArt);
    case SongCountRole: return int(a.trackRows.size());
    }
    return {};
}

QHash<int, QByteArray> AlbumListModel::roleNames() const
{
    return {{TitleRole, "title"}, {ArtistRole, "artist"}, {YearRole, "year"}, {ArtRole, "art"}, {SongCountRole, "songCount"}};
}

void AlbumListModel::setAlbums(const QList<Album> &albums)
{
    beginResetModel();
    m_items = albums;
    endResetModel();
    emit countChanged();
}

} // namespace hop
