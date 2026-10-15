// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/TrackListModel.h"

namespace hop {

TrackListModel::TrackListModel(QObject *parent) : QAbstractListModel(parent) {}

int TrackListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_tracks.size();
}

QVariant TrackListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_tracks.size()) return {};
    const Track &t = m_tracks[index.row()];
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:       return t.displayTitle();
    case TrackIdRole:     return t.id;
    case ArtistRole:      return t.displayArtist();
    case AlbumRole:       return t.displayAlbum();
    case AlbumArtistRole: return t.albumArtist.isEmpty() ? t.displayArtist() : t.albumArtist;
    case TrackNoRole:     return t.trackNo;
    case YearRole:        return t.year;
    case DurationRole:    return formatDuration(t.durationMs);
    case PathRole:        return t.path;
    case HeartedRole:     return m_stats ? m_stats->value(t.path).hearted : false;
    case PlayCountRole:   return m_stats ? m_stats->value(t.path).playCount : 0;
    }
    return {};
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    return {
        {TrackIdRole, "trackId"}, {TitleRole, "title"}, {ArtistRole, "artist"},
        {AlbumRole, "album"}, {AlbumArtistRole, "albumArtist"}, {TrackNoRole, "trackNo"},
        {YearRole, "year"}, {DurationRole, "duration"}, {PathRole, "path"},
        {HeartedRole, "hearted"}, {PlayCountRole, "playCount"},
    };
}

void TrackListModel::setTracks(const QList<Track> &tracks)
{
    beginResetModel();
    m_tracks = tracks;
    endResetModel();
    emit countChanged();
}

void TrackListModel::statsChanged(const QString &path)
{
    for (int i = 0; i < m_tracks.size(); ++i)
        if (m_tracks[i].path == path) emit dataChanged(index(i), index(i), {HeartedRole, PlayCountRole});
}

QString TrackListModel::formatDuration(qint64 ms)
{
    if (ms <= 0) return QStringLiteral("–");
    const qint64 s = ms / 1000;
    if (s >= 3600)
        return QStringLiteral("%1:%2:%3").arg(s / 3600).arg((s / 60) % 60, 2, 10, QLatin1Char('0'))
                                          .arg(s % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

} // namespace hop
