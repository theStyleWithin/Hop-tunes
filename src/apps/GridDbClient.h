// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "net/AppNet.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <functional>

namespace hop {

// Cover pictures for the game shelf from SteamGridDB (steamgriddb.com), a community site of game artwork. Off until the person allows it and
// gives their own free API key (made at steamgriddb.com, in their preferences): there is no key built into Hop-tunes. Everything goes through the
// gateway (net/AppNet), only while the cover picker is open: a search by game name (or by Steam app id), the list of vertical covers, and the one
// the person picks, which is downloaded once and kept as a copy in their profile (nothing else is kept). What is sent: the search word or Steam id,
// the key, and the person's internet address, like any visitor; no account, no Hop-tunes details, no cookies. The key lives in this object's memory
// only; App keeps it in its own file in the profile, outside the profile export. Not made or endorsed by SteamGridDB; the covers belong to their
// authors and are shown and kept only for the person's own shelf.
class GridDbClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)                 // a plain sentence about the last step ("" when all is well)
    Q_PROPERTY(bool hasKey READ hasKey NOTIFY changed)
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)           // search results: [{id, name, verified}]
    Q_PROPERTY(QVariantList grids READ grids NOTIFY gridsChanged)           // covers of one game: [{id, url, thumb, width, height, score, author}]
public:
    static constexpr const char *kApp = "steamgriddb";
    static constexpr int kMaxGames = 20, kMaxGrids = 24, kMaxImageBytes = 12 * 1024 * 1024;

    explicit GridDbClient(AppNet *net, QObject *parent = nullptr);

    // the person's key (memory only); a key is letters, digits, "-" and "_", of a sensible length
    static bool validKey(const QString &key);
    void setKey(const QString &key);
    bool hasKey() const { return !m_key.isEmpty(); }

    bool isOpen() const { return m_open; }
    bool busy() const { return m_busy > 0; }
    QString message() const { return m_message; }
    QVariantList games() const { return m_games; }
    QVariantList grids() const { return m_grids; }

    Q_INVOKABLE void open();     // needs the person's permission (AppNet) and a key
    Q_INVOKABLE void close();    // drops everything on the way and forgets the lists
    Q_INVOKABLE void search(const QString &term);
    Q_INVOKABLE void gridsForGame(int gameId);
    Q_INVOKABLE void gridsForSteam(const QString &appId);
    // one picked cover: the picture's bytes, or why not. The address must be one of SteamGridDB's own picture servers.
    void download(const QString &url, std::function<void(QByteArray image, QString error)> done);

    // what the answers are made of (public so the tests can drive them without a screen)
    static QVariantList parseGames(const QByteArray &json);       // [] when it is not a usable answer
    static QVariantList parseGrids(const QByteArray &json);
    static QString searchUrl(const QString &term);
    static QString gridsUrl(int gameId);
    static QString gridsSteamUrl(const QString &appId);
    static QString explain(int status, const QString &error);     // a plain sentence for a failed request

signals:
    void changed();
    void gamesChanged();
    void gridsChanged();

private:
    void request(const QString &url, std::function<void(NetReply)> done);
    void setMessage(const QString &m);

    AppNet *m_net;
    QString m_key;
    bool m_open = false;
    int m_busy = 0;
    quint64 m_epoch = 0;
    QString m_message;
    QVariantList m_games, m_grids;
};

} // namespace hop
