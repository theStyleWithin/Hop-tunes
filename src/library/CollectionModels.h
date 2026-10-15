// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Track.h"

#include <QAbstractListModel>
#include <QList>

namespace hop {

// One album as shown in the collection: grouped by (album artist, album title).
struct Album {
    QString title;
    QString artist;          // album artist, or track artist when no album artist is tagged
    int year = 0;
    QString art;             // first track's cover, if any
    QList<int> trackRows;    // indices into the library track list, in play order
};

struct Artist {
    QString name;
    QList<int> albumIndices; // indices into Collection::albums()
    int songCount = 0;
    QString art;
};

// Groups an album-ordered track list into albums and artists (pure logic, unit-tested).
class Collection {
public:
    void build(const QList<Track> &tracks);
    const QList<Album> &albums() const { return m_albums; }
    const QList<Artist> &artists() const { return m_artists; }

private:
    QList<Album> m_albums;
    QList<Artist> m_artists;
};

class ArtistListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role { NameRole = Qt::UserRole + 1, AlbumCountRole, SongCountRole, ArtRole };
    explicit ArtistListModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : m_items.size(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return m_items.size(); }
    void setArtists(const QList<Artist> &artists);
signals:
    void countChanged();
private:
    QList<Artist> m_items;
};

class AlbumListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role { TitleRole = Qt::UserRole + 1, ArtistRole, YearRole, ArtRole, SongCountRole };
    explicit AlbumListModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : m_items.size(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return m_items.size(); }
    void setAlbums(const QList<Album> &albums);
    const Album *at(int row) const { return row >= 0 && row < m_items.size() ? &m_items[row] : nullptr; }
signals:
    void countChanged();
private:
    QList<Album> m_items;
};

} // namespace hop
