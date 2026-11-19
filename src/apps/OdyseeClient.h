// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "net/AppNet.h"

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace hop {

// The Odysee app: browse, search and watch the videos on odysee.com through its public search interface (no account, nothing to register), via the
// gateway (net/AppNet), only while the app is open and only after the person allowed it. That interface is unofficial and can change or stop at any
// time. Odysee's search only answers a JSON request, so this is the one app that uses the gateway's single narrow POST (a read-only search to one
// address, see AppNet::postJson). Follows, blocked channels and words, favourites, playlists and the 18+ switch stay in the person's profile on this
// device; nothing else is kept and no video is saved. Not made or endorsed by Odysee; the videos belong to their creators and are only shown.
class OdyseeClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY openChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString query READ query NOTIFY changed)                   // "" (trending), a word, "@channel", or "following"
    Q_PROPERTY(QVariantList videos READ videos NOTIFY videosChanged)      // what the filters let through
    Q_PROPERTY(int hiddenCount READ hiddenCount NOTIFY videosChanged)
    Q_PROPERTY(bool canLoadMore READ canLoadMore NOTIFY videosChanged)
    Q_PROPERTY(bool showMature READ showMature WRITE setShowMature NOTIFY filtersChanged)   // the 18+ switch (off at the start)
    Q_PROPERTY(QStringList blockedChannels READ blockedChannels NOTIFY filtersChanged)
    Q_PROPERTY(QStringList blockedWords READ blockedWords NOTIFY filtersChanged)
    Q_PROPERTY(QVariantList follows READ follows NOTIFY listsChanged)     // [{name, id}]
    Q_PROPERTY(QVariantList favourites READ favourites NOTIFY listsChanged)
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY listsChanged) // [{name, videos: [video]}]
public:
    static constexpr const char *kApp = "odysee";
    static constexpr int kPage = 30, kMaxKept = 600, kMaxFollows = 200, kMaxBlocked = 500, kMaxFavourites = 500, kMaxPlaylists = 30, kMaxPlaylistVideos = 300;

    struct Video {
        QString id, name, title, channel, channelId, thumb, url, page;
        int duration = 0;
        qint64 released = 0;
        bool mature = false;
    };
    // The answer to a search: only video claims with a file the app can play or a page to open; thumbnails only on Odysee's picture host.
    static QVector<Video> parse(const QByteArray &json);
    // The search request, built only from these fixed fields (what is typed is cleaned first). `channel` is "@name" (or ""), `channelIds` for the
    // following view. Never anything else: the gateway also refuses any other method or address.
    static QByteArray request(const QString &text, const QString &channel, const QStringList &channelIds, int page, bool mature);
    static QString playUrl(const QString &name, const QString &claimId, const QString &sdHash, const QString &mediaType);
    static QString pageUrl(const QString &canonical);                    // "lbry://@a#3/b#c" -> "https://odysee.com/@a:3/b:c"
    static bool validChannel(const QString &c);                          // "@name"

    explicit OdyseeClient(AppNet *net, QObject *parent = nullptr);

    bool isOpen() const { return m_open; }
    bool loading() const { return m_loading; }
    QString message() const { return m_message; }
    QString query() const { return m_query; }
    QVariantList videos() const { return m_visible; }
    int hiddenCount() const { return m_hidden; }
    bool canLoadMore() const { return m_more && !m_loading; }
    bool showMature() const { return m_showMature; }
    void setShowMature(bool on);
    QStringList blockedChannels() const { return m_blockedChannels; }
    QStringList blockedWords() const { return m_blockedWords; }
    QVariantList follows() const { return m_follows; }
    QVariantList favourites() const { return m_favs; }
    QVariantList playlists() const;

    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    Q_INVOKABLE void search(const QString &text);                 // "" = trending; "@name" = that channel; anything else = a word
    Q_INVOKABLE void showFollowing();                             // the newest videos of the channels you follow
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE bool streamAllowed(const QString &url) const;
    Q_INVOKABLE void blockChannel(const QString &name);
    Q_INVOKABLE void unblockChannel(const QString &name);
    Q_INVOKABLE bool isBlockedChannel(const QString &name) const;
    Q_INVOKABLE bool blockWord(const QString &word);
    Q_INVOKABLE void unblockWord(const QString &word);
    Q_INVOKABLE void toggleFollow(const QString &name, const QString &channelId);
    Q_INVOKABLE bool isFollowing(const QString &name) const;
    Q_INVOKABLE void toggleFavourite(const QVariantMap &video);
    Q_INVOKABLE bool isFavourite(const QString &id) const;
    Q_INVOKABLE bool createPlaylist(const QString &name);
    Q_INVOKABLE void deletePlaylist(const QString &name);
    Q_INVOKABLE void togglePlaylistVideo(const QString &playlist, const QVariantMap &video);
    Q_INVOKABLE bool inPlaylist(const QString &playlist, const QString &id) const;

    QJsonObject listsJson() const;
    void setListsJson(const QJsonObject &o);                      // (an empty object for a guest)
    bool hides(const Video &v, QString *why = nullptr) const;

signals:
    void openChanged();
    void changed();
    void videosChanged();
    void filtersChanged();
    void listsChanged();
    void listsEdited();

private:
    void run(bool append);
    void rebuild();
    void setMessage(const QString &m);
    static QString clean(const QString &s, int max = 200);
    static QVariantMap keep(const QVariantMap &v);                // the fields a favourite or playlist entry keeps (validated)
    QStringList followIds() const;

    AppNet *m_net;
    bool m_open = false, m_loading = false, m_showMature = false, m_more = false;
    quint64 m_gen = 0;
    int m_page = 1;
    QString m_message, m_query, m_channel;
    bool m_following = false;
    QVector<Video> m_all;
    QVariantList m_visible, m_follows, m_favs;
    QVector<QPair<QString, QVariantList>> m_playlists;
    int m_hidden = 0;
    QStringList m_blockedChannels, m_blockedWords;
};

} // namespace hop
