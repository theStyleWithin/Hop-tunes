// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Track.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>

namespace hop {

// Flat song list for QML (collection › songs, queues).
class TrackListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role {
        TrackIdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        AlbumRole,
        AlbumArtistRole,
        TrackNoRole,
        YearRole,
        DurationRole,      // "3:42"
        PathRole,
        HeartedRole,
        PlayCountRole,
    };

    explicit TrackListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_tracks.size(); }
    void setTracks(const QList<Track> &tracks);
    const QList<Track> &tracks() const { return m_tracks; }

    // Per-profile stats for the hearted / play-count roles (owned by the App; may be null).
    void setStats(const QHash<QString, TrackStats> *stats) { m_stats = stats; }
    void statsChanged(const QString &path);   // refresh rows showing this file

    static QString formatDuration(qint64 ms);

signals:
    void countChanged();

private:
    QList<Track> m_tracks;
    const QHash<QString, TrackStats> *m_stats = nullptr;
};

} // namespace hop
