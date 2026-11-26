// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/GameListModel.h"

#include <QMap>
#include <QVariantMap>

#include <algorithm>

namespace hop {

GameListModel::GameListModel(QObject *parent) : QAbstractListModel(parent) {}

int GameListModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : int(m_shown.size()); }

QHash<int, QByteArray> GameListModel::roleNames() const
{
    return {{GameIdRole, "gameId"}, {TitleRole, "title"}, {PathRole, "path"}, {SystemRole, "system"}, {SystemNameRole, "systemName"},
            {SystemColorRole, "systemColor"}, {SourceRole, "source"}, {AppIdRole, "appId"}, {CoverRole, "cover"},
            {CustomCoverRole, "customCover"}, {RenamedRole, "renamed"}};
}

QVariant GameListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_shown.size()) return {};
    const Entry &e = m_all.at(m_shown.at(index.row()));
    switch (role) {
    case GameIdRole:      return e.game.id;
    case TitleRole:       return e.title;
    case PathRole:        return e.game.path;
    case SystemRole:      return e.game.system;
    case SystemNameRole:  return gameSystemName(e.game.system);
    case SystemColorRole: return gameSystemColor(e.game.system);
    case SourceRole:      return e.game.source;
    case AppIdRole:       return e.game.appId;
    case CoverRole:       return e.coverUrl;
    case CustomCoverRole: return e.customCover;
    case RenamedRole:     return e.renamed;
    }
    return {};
}

void GameListModel::setEntries(const QList<Entry> &entries)
{
    beginResetModel();
    m_all = entries;
    std::stable_sort(m_all.begin(), m_all.end(), [](const Entry &a, const Entry &b) {
        return QString::compare(a.title, b.title, Qt::CaseInsensitive) < 0;
    });
    m_shown.clear();
    rebuildSystems();
    if (!m_system.isEmpty()) {
        bool present = false;
        for (const Entry &e : m_all) if (e.game.system == m_system) { present = true; break; }
        if (!present) { m_system.clear(); emit filterChanged(); }      // the chosen system has no games any more
    }
    for (int i = 0; i < m_all.size(); ++i) {
        const Entry &e = m_all.at(i);
        if (!m_system.isEmpty() && e.game.system != m_system) continue;
        if (!m_text.isEmpty() && !e.title.contains(m_text, Qt::CaseInsensitive)) continue;
        m_shown.append(i);
    }
    endResetModel();
    emit countChanged();
    emit systemsChanged();
}

void GameListModel::refilter()
{
    beginResetModel();
    m_shown.clear();
    for (int i = 0; i < m_all.size(); ++i) {
        const Entry &e = m_all.at(i);
        if (!m_system.isEmpty() && e.game.system != m_system) continue;
        if (!m_text.isEmpty() && !e.title.contains(m_text, Qt::CaseInsensitive)) continue;
        m_shown.append(i);
    }
    endResetModel();
    emit countChanged();
}

void GameListModel::rebuildSystems()
{
    QMap<QString, int> counts;
    for (const Entry &e : m_all) counts[e.game.system]++;
    QVariantList out;
    auto add = [&](const QString &key) {
        QVariantMap m;
        m.insert(QStringLiteral("key"), key);
        m.insert(QStringLiteral("name"), gameSystemName(key));
        m.insert(QStringLiteral("color"), gameSystemColor(key));
        m.insert(QStringLiteral("count"), counts.value(key));
        out.append(m);
    };
    if (counts.contains(QStringLiteral("pc"))) add(QStringLiteral("pc"));
    QStringList keys = counts.keys();
    keys.removeAll(QStringLiteral("pc"));
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
        return QString::compare(gameSystemName(a), gameSystemName(b), Qt::CaseInsensitive) < 0;
    });
    for (const QString &k : keys) add(k);
    m_systems = out;
}

void GameListModel::setSystemFilter(const QString &key)
{
    if (key == m_system) return;
    m_system = key;
    refilter();
    emit filterChanged();
}

void GameListModel::setTextFilter(const QString &text)
{
    const QString t = text.trimmed();
    if (t == m_text) return;
    m_text = t;
    refilter();
    emit filterChanged();
}

const GameListModel::Entry *GameListModel::entryForPath(const QString &path) const
{
    for (const Entry &e : m_all) if (e.game.path == path) return &e;
    return nullptr;
}

QString GameListModel::pathAt(int row) const { return (row >= 0 && row < m_shown.size()) ? m_all.at(m_shown.at(row)).game.path : QString(); }
QString GameListModel::titleAt(int row) const { return (row >= 0 && row < m_shown.size()) ? m_all.at(m_shown.at(row)).title : QString(); }

} // namespace hop
