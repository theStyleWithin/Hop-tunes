// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Game.h"

#include <QAbstractListModel>
#include <QList>
#include <QVariantList>

namespace hop {

// The shelf's model: every game found, filtered by system and a search word, ordered by title. App fills it (with the user's own titles and covers
// laid over what the scan found); the shelf only shows it.
class GameListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)               // shown (after the filter)
    Q_PROPERTY(int total READ total NOTIFY countChanged)               // all games
    Q_PROPERTY(QVariantList systems READ systems NOTIFY systemsChanged) // [{key, name, color, count}] for the filter chips, steam first
    Q_PROPERTY(QString systemFilter READ systemFilter WRITE setSystemFilter NOTIFY filterChanged)
    Q_PROPERTY(QString textFilter READ textFilter WRITE setTextFilter NOTIFY filterChanged)
public:
    enum Role {
        GameIdRole = Qt::UserRole + 1,
        TitleRole,        // the user's own title when they gave one
        PathRole,
        SystemRole,       // key
        SystemNameRole,
        SystemColorRole,
        SourceRole,       // "steam" | "rom"
        AppIdRole,
        CoverRole,        // file:// URL of a picture to show, or ""
        CustomCoverRole,  // the cover is the user's own pick
        RenamedRole,      // the title is the user's own
    };

    struct Entry { Game game; QString title; QString coverUrl; bool customCover = false; bool renamed = false; };

    explicit GameListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_shown.size(); }
    int total() const { return m_all.size(); }
    QVariantList systems() const { return m_systems; }
    QString systemFilter() const { return m_system; }
    QString textFilter() const { return m_text; }
    void setSystemFilter(const QString &key);
    void setTextFilter(const QString &text);

    void setEntries(const QList<Entry> &entries);
    void clear() { setEntries({}); }
    const Entry *entryForPath(const QString &path) const;   // among all, filter or not

    Q_INVOKABLE QVariantList shelfItems() const;   // the shown games as [{path, title, systemName, systemColor, cover}], for the bookshelf display
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE QString titleAt(int row) const;

signals:
    void countChanged();
    void systemsChanged();
    void filterChanged();

private:
    void refilter();
    void rebuildSystems();

    QList<Entry> m_all;
    QList<int> m_shown;        // indexes into m_all
    QVariantList m_systems;
    QString m_system;          // "" = all
    QString m_text;
};

} // namespace hop
