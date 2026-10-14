// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Track.h"

#include <QAbstractListModel>
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
    };

    explicit TrackListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_tracks.size(); }
    void setTracks(const QList<Track> &tracks);
    const QList<Track> &tracks() const { return m_tracks; }

    static QString formatDuration(qint64 ms);

signals:
    void countChanged();

private:
    QList<Track> m_tracks;
};

} // namespace hop
