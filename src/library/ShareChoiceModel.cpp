// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/ShareChoiceModel.h"

#include "library/ProfileDb.h"

namespace hop {

QHash<int, QByteArray> ShareChoiceModel::roleNames() const
{
    return {{TitleRole, "title"}, {SubtitleRole, "subtitle"}, {StateRole, "state"}, {FileCountRole, "fileCount"}};
}

int ShareChoiceModel::stateOf(const ShareChoice &c) const
{
    int in = 0;
    for (const QString &p : c.paths) if (m_grants.contains(p)) ++in;
    return in == 0 ? 0 : in == c.paths.size() ? 2 : 1;
}

QVariant ShareChoiceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_shown.size()) return {};
    const ShareChoice &c = m_all[m_shown[index.row()]];
    switch (role) {
    case TitleRole: return c.title;
    case SubtitleRole: return c.subtitle;
    case StateRole: return stateOf(c);
    case FileCountRole: return c.paths.size();
    }
    return {};
}

int ShareChoiceModel::approvedCount() const
{
    int n = 0;
    for (int i : m_shown) if (stateOf(m_all[i]) == 2) ++n;
    return n;
}

void ShareChoiceModel::setFilter(const QString &text)
{
    if (text == m_filter) return;
    m_filter = text;
    rebuild();
    emit filterChanged();
}

void ShareChoiceModel::setChoices(qint64 guestId, const QList<ShareChoice> &all)
{
    m_guest = guestId;
    m_all = all;
    m_grants = guestId > 0 ? m_db.shareGrants(guestId) : QSet<QString>();
    rebuild();
}

void ShareChoiceModel::rebuild()
{
    beginResetModel();
    m_shown.clear();
    const QString f = m_filter.trimmed();
    for (int i = 0; i < m_all.size(); ++i)
        if (f.isEmpty() || m_all[i].title.contains(f, Qt::CaseInsensitive) || m_all[i].subtitle.contains(f, Qt::CaseInsensitive))
            m_shown.append(i);
    endResetModel();
    emit countChanged();
    emit approvedCountChanged();
}

void ShareChoiceModel::toggle(int row)
{
    if (row < 0 || row >= m_shown.size() || m_guest <= 0) return;
    const ShareChoice &c = m_all[m_shown[row]];
    const bool approve = stateOf(c) != 2;
    if (!m_db.setShareGrants(m_guest, c.paths, approve)) return;
    for (const QString &p : c.paths) { if (approve) m_grants.insert(p); else m_grants.remove(p); }
    emit dataChanged(index(row), index(row), {StateRole});
    emit approvedCountChanged();
}

void ShareChoiceModel::setShown(bool approve)
{
    if (m_guest <= 0 || m_shown.isEmpty()) return;
    QStringList paths;
    for (int i : m_shown) paths += m_all[i].paths;
    if (!m_db.setShareGrants(m_guest, paths, approve)) return;
    for (const QString &p : paths) { if (approve) m_grants.insert(p); else m_grants.remove(p); }
    emit dataChanged(index(0), index(m_shown.size() - 1), {StateRole});
    emit approvedCountChanged();
}

} // namespace hop
