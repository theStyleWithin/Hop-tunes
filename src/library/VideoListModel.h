// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Video.h"

#include <QAbstractListModel>
#include <QList>

namespace hop {

// Flat video list for QML (collection › videos, spec §5b).
class VideoListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role {
        VideoIdRole = Qt::UserRole + 1,
        TitleRole,
        PathRole,
        ThumbRole,
        DurationRole,     // "1:42:07"
        WidthRole,
        HeightRole,
        ResolutionBadgeRole,   // "4K" | "1440p" | "1080p" | "720p" | "SD" | ""
        HdrRole,
        CodecBadgeRole,    // upper-case short codec name for the badge row, e.g. "HEVC"
    };

    explicit VideoListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_videos.size(); }
    void setVideos(const QList<Video> &videos);
    const QList<Video> &videos() const { return m_videos; }

    static QString resolutionBadge(int width, int height);

signals:
    void countChanged();

private:
    QList<Video> m_videos;
};

} // namespace hop
