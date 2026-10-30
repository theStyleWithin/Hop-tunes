// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/VideoListModel.h"

#include "library/TrackListModel.h"

#include <QUrl>

namespace hop {

namespace {
QString thumbUrl(const QString &path) { return path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString(); }
} // namespace

VideoListModel::VideoListModel(QObject *parent) : QAbstractListModel(parent) {}

int VideoListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_videos.size();
}

QString VideoListModel::resolutionBadge(int width, int height)
{
    const int longEdge = qMax(width, height);
    if (longEdge >= 3840) return QStringLiteral("4K");
    if (longEdge >= 2560) return QStringLiteral("1440p");
    if (longEdge >= 1920) return QStringLiteral("1080p");
    if (longEdge >= 1280) return QStringLiteral("720p");
    return longEdge > 0 ? QStringLiteral("SD") : QString();
}

QVariant VideoListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_videos.size()) return {};
    const Video &v = m_videos[index.row()];
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:            return v.shownTitle();
    case VideoIdRole:          return v.id;
    case PathRole:             return v.path;
    case ThumbRole:            return thumbUrl(v.thumb);
    case DurationRole:         return TrackListModel::formatDuration(v.durationMs);
    case WidthRole:            return v.width;
    case HeightRole:           return v.height;
    case ResolutionBadgeRole:  return resolutionBadge(v.width, v.height);
    case HdrRole:              return v.hdr;
    case CodecBadgeRole:       return v.codec.toUpper();
    case CustomThumbRole:      return v.customThumb;
    }
    return {};
}

QHash<int, QByteArray> VideoListModel::roleNames() const
{
    return {
        {VideoIdRole, "videoId"}, {TitleRole, "title"}, {PathRole, "path"},
        {ThumbRole, "thumb"}, {DurationRole, "duration"}, {WidthRole, "width"},
        {HeightRole, "height"}, {ResolutionBadgeRole, "resolutionBadge"},
        {HdrRole, "hdr"}, {CodecBadgeRole, "codecBadge"}, {CustomThumbRole, "customThumb"},
    };
}

void VideoListModel::setVideos(const QList<Video> &videos)
{
    beginResetModel();
    m_videos = videos;
    endResetModel();
    emit countChanged();
}

} // namespace hop
