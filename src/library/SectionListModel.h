// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Section.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QSet>

namespace hop {

// List model for one collection section (pictures, audiobooks, ebooks, podcasts).
class SectionListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role {
        ItemIdRole = Qt::UserRole + 1,
        TitleRole,
        PathRole,
        FileUrlRole,    // file:// URL, for pictures to load directly
        GroupRole,      // album / show / author / folder
        DurationRole,   // "1:42:07", empty when unknown
        ProgressRole,   // 0..1 listened/read so far, 0 when untouched
        SizeTextRole,   // "2.4 MB"
        PlayedRole,     // finished (audiobooks, podcast episodes)
    };

    explicit SectionListModel(SectionKind kind, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    SectionKind kind() const { return m_kind; }
    int count() const { return m_items.size(); }
    void setItems(const QList<SectionItem> &items, const QHash<QString, double> &progress = {},
                  const QSet<QString> &played = {});
    void setPlayed(const QString &path, bool played);
    void setProgress(const QString &path, double fraction);   // updates one row in place
    const QList<SectionItem> &items() const { return m_items; }

    Q_INVOKABLE QString fileUrlAt(int row) const;
    Q_INVOKABLE QString titleAt(int row) const;
    Q_INVOKABLE QString pathAt(int row) const;

    static QString formatSize(qint64 bytes);

signals:
    void countChanged();

private:
    SectionKind m_kind;
    QList<SectionItem> m_items;
    QHash<QString, double> m_progress;
    QSet<QString> m_played;
};

} // namespace hop
