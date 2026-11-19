// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "net/AppNet.h"

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVector>

namespace hop {

// The DeviantArt app: browse artwork from DeviantArt's public RSS feeds (no account, nothing to register), through the gateway
// (net/AppNet), only while the app is open and only after the person allowed it. Unofficial: Hop-tunes is not made or endorsed by
// DeviantArt, and the feeds can change. The person's own lists (blocked artists, blocked words, favourites, the 18+ switch) stay in their
// profile on this device; nothing is kept otherwise and no picture is saved.
class DeviantArtClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY openChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)               // a plain sentence about what went wrong or why nothing shows
    Q_PROPERTY(QString query READ query NOTIFY changed)                   // what is on screen: "" (popular), a word, or "@artist"
    Q_PROPERTY(QVariantList works READ works NOTIFY worksChanged)         // what the filters let through
    Q_PROPERTY(int hiddenCount READ hiddenCount NOTIFY worksChanged)     // how many of this view your filters hid
    Q_PROPERTY(bool canLoadMore READ canLoadMore NOTIFY worksChanged)
    Q_PROPERTY(bool showMature READ showMature WRITE setShowMature NOTIFY filtersChanged)   // the 18+ switch (off at the start)
    Q_PROPERTY(QStringList blockedArtists READ blockedArtists NOTIFY filtersChanged)
    Q_PROPERTY(QStringList blockedTags READ blockedTags NOTIFY filtersChanged)
    Q_PROPERTY(QStringList favouriteArtists READ favouriteArtists NOTIFY listsChanged)
    Q_PROPERTY(QVariantList favouriteWorks READ favouriteWorks NOTIFY listsChanged)
public:
    static constexpr const char *kApp = "deviantart";
    static constexpr int kMaxFavouriteWorks = 500, kMaxBlocked = 500, kMaxWorksKept = 600;

    struct Work {
        QString id, title, artist, page, image, thumb, rating, license, copyright, description, published;
        bool mature = false;
    };
    // The feed's items; `next` gets the address of the following page when the feed names one (only ever an address on the feed's own host).
    static QVector<Work> parseRss(const QByteArray &xml, QString *next = nullptr);
    static QString licenceName(const QString &url);                       // "CC BY-NC-ND" from a Creative Commons address, else ""

    explicit DeviantArtClient(AppNet *net, QObject *parent = nullptr);

    bool isOpen() const { return m_open; }
    bool loading() const { return m_loading; }
    QString message() const { return m_message; }
    QString query() const { return m_query; }
    QVariantList works() const { return m_visible; }
    int hiddenCount() const { return m_hidden; }
    bool canLoadMore() const { return !m_next.isEmpty() && !m_loading; }
    bool showMature() const { return m_showMature; }
    void setShowMature(bool on);
    QStringList blockedArtists() const { return m_blockedArtists; }
    QStringList blockedTags() const { return m_blockedTags; }
    QStringList favouriteArtists() const { return m_favArtists; }
    QVariantList favouriteWorks() const { return m_favWorks; }

    Q_INVOKABLE void open();                                      // starts the connection (the permission was given) and shows what is popular
    Q_INVOKABLE void close();                                     // drops everything on screen and any answer still on its way
    Q_INVOKABLE void search(const QString &text);                 // "" = popular; "@name" = that artist's gallery; anything else = a word or tag
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void blockArtist(const QString &name);
    Q_INVOKABLE void unblockArtist(const QString &name);
    Q_INVOKABLE bool isBlockedArtist(const QString &name) const;
    Q_INVOKABLE bool blockTag(const QString &word);               // false when it is empty, a duplicate or the list is full
    Q_INVOKABLE void unblockTag(const QString &word);
    Q_INVOKABLE void toggleFavouriteArtist(const QString &name);
    Q_INVOKABLE bool isFavouriteArtist(const QString &name) const;
    Q_INVOKABLE void toggleFavouriteWork(const QVariantMap &work); // {id, title, artist, page, thumb}
    Q_INVOKABLE bool isFavouriteWork(const QString &id) const;

    QJsonObject listsJson() const;                                // what the profile keeps
    void setListsJson(const QJsonObject &o);                      // (an empty object for a guest)

    // the rules, public so the tests can drive them without a screen
    bool hides(const Work &w, QString *why = nullptr) const;

signals:
    void openChanged();
    void changed();
    void worksChanged();
    void filtersChanged();
    void listsChanged();
    void listsEdited();                                           // something worth saving in the profile changed

private:
    void fetch(const QString &url, bool append);
    void rebuild();
    void setMessage(const QString &m);
    static QString feedUrl(const QString &q);
    static QString clean(const QString &s);                       // trimmed, one line, bounded

    AppNet *m_net;
    bool m_open = false, m_loading = false, m_showMature = false;
    quint64 m_gen = 0;
    QString m_message, m_query, m_next;
    QVector<Work> m_all;
    QVariantList m_visible;
    int m_hidden = 0;
    QStringList m_blockedArtists, m_blockedTags, m_favArtists;
    QVariantList m_favWorks;
};

} // namespace hop
