// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/SectionListModel.h"

#include "library/TrackListModel.h"

#include <QUrl>

namespace hop {

SectionListModel::SectionListModel(SectionKind kind, QObject *parent)
    : QAbstractListModel(parent), m_kind(kind) {}

int SectionListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QString SectionListModel::formatSize(qint64 bytes)
{
    if (bytes >= (1LL << 30)) return QStringLiteral("%1 GB").arg(double(bytes) / (1LL << 30), 0, 'f', 1);
    if (bytes >= (1LL << 20)) return QStringLiteral("%1 MB").arg(double(bytes) / (1LL << 20), 0, 'f', 1);
    return QStringLiteral("%1 KB").arg(qMax<qint64>(1, bytes / 1024));
}

QVariant SectionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) return {};
    const SectionItem &it = m_items[index.row()];
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:     return it.title;
    case ItemIdRole:    return it.id;
    case PathRole:      return it.path;
    case FileUrlRole:   return QUrl::fromLocalFile(it.path).toString();
    case GroupRole:     return it.group;
    case DurationRole:  return it.durationMs > 0 ? TrackListModel::formatDuration(it.durationMs) : QString();
    case ProgressRole:  return m_progress.value(it.path, 0.0);
    case SizeTextRole:  return formatSize(it.size);
    case PlayedRole:    return m_played.contains(it.path);
    }
    return {};
}

QHash<int, QByteArray> SectionListModel::roleNames() const
{
    return {
        {ItemIdRole, "itemId"}, {TitleRole, "title"}, {PathRole, "path"}, {FileUrlRole, "fileUrl"},
        {GroupRole, "group"}, {DurationRole, "duration"}, {ProgressRole, "progress"},
        {SizeTextRole, "sizeText"}, {PlayedRole, "played"},
    };
}

QString SectionListModel::fileUrlAt(int row) const
{
    return row >= 0 && row < m_items.size() ? QUrl::fromLocalFile(m_items[row].path).toString() : QString();
}

QString SectionListModel::titleAt(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items[row].title : QString();
}

QString SectionListModel::pathAt(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items[row].path : QString();
}

void SectionListModel::setItems(const QList<SectionItem> &items, const QHash<QString, double> &progress,
                                const QSet<QString> &played)
{
    beginResetModel();
    m_items = items;
    m_progress = progress;
    m_played = played;
    endResetModel();
    emit countChanged();
}

void SectionListModel::setPlayed(const QString &path, bool played)
{
    if (played) m_played.insert(path); else m_played.remove(path);
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].path == path) { emit dataChanged(index(i), index(i), {PlayedRole}); break; }
}

void SectionListModel::setProgress(const QString &path, double fraction)
{
    m_progress.insert(path, fraction);
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].path == path) { emit dataChanged(index(i), index(i), {ProgressRole}); break; }
}

} // namespace hop
