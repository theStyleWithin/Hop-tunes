// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

namespace hop {

class ProfileDb;

// One thing the host can approve for a guest: a video, or an album (several files at once).
struct ShareChoice {
    QString title;
    QString subtitle;
    QStringList paths;   // every file this row stands for
};

// The host's list for choosing what one guest may see (docs/design-sharing.md). Nothing is approved to begin with; a
// click approves or takes back a row (all its files), and the choice is saved in the profile straight away, so it is
// there the next time that guest visits. A row of several files shows as "partly" when only some are approved.
class ShareChoiceModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int approvedCount READ approvedCount NOTIFY approvedCountChanged)   // rows shown that are fully approved
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
public:
    enum Role { TitleRole = Qt::UserRole + 1, SubtitleRole, StateRole, FileCountRole };   // StateRole: 0 none, 1 partly, 2 all

    explicit ShareChoiceModel(ProfileDb &db, QObject *parent = nullptr) : QAbstractListModel(parent), m_db(db) {}

    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : m_shown.size(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_shown.size(); }
    int approvedCount() const;
    QString filter() const { return m_filter; }
    void setFilter(const QString &text);

    // Start choosing for a guest from this list of things (their current approvals are loaded from the profile).
    void setChoices(qint64 guestId, const QList<ShareChoice> &all);
    qint64 guestId() const { return m_guest; }

    Q_INVOKABLE void toggle(int row);         // approve everything in the row, or (if it is all approved) take it back
    Q_INVOKABLE void setShown(bool approve);  // the same for every row currently shown (after the filter)

signals:
    void countChanged();
    void approvedCountChanged();
    void filterChanged();

private:
    int stateOf(const ShareChoice &c) const;
    void rebuild();
    ProfileDb &m_db;
    qint64 m_guest = 0;
    QList<ShareChoice> m_all;
    QList<int> m_shown;        // indices into m_all that match the filter
    QSet<QString> m_grants;    // this guest's approved paths
    QString m_filter;
};

} // namespace hop
