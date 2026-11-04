// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <functional>

class QHostAddress;
class QSslServer;
class QSslSocket;
class QSslPreSharedKeyAuthenticator;

namespace hop {

class ProfileDb;
class ShareAuth;
class ShareConnection;

// What one guest may see: an opaque id (never a path), a title, a kind and a size.
struct ShareItem {
    QString id;
    QString title;
    QString kind;   // "video" | "audio" | "picture" | "book" | "file"
    qint64 size = 0;
    QString artist, album;   // music
    qint64 durationMs = 0;
    int trackNo = 0;
};

// What the host is watching right now, for guests who follow along (Party Watch).
struct PartyInfo {
    bool active = false;
    QString path;          // the host's own file; a guest only ever hears about it if it is approved for them
    QString title;
    qint64 positionMs = 0;
    bool paused = false;
};

// Where the server gets its library from. Only items the host approved for this guest are ever returned.
class ShareLibrary {
public:
    virtual ~ShareLibrary() = default;
    virtual QString hostName() const = 0;
    virtual QList<ShareItem> items(qint64 guestId) const = 0;
    virtual QString pathFor(qint64 guestId, const QString &itemId) const = 0;   // empty when not approved / unknown
    virtual QString idForPath(qint64, const QString &) const { return {}; }      // this guest's id for an approved file, else empty
};

// The simplest library: exactly the paths the host approved for the guest (the share_grant table), titled by file name.
class GrantLibrary : public ShareLibrary {
public:
    GrantLibrary(ProfileDb &db, ShareAuth &auth, QString hostName) : m_db(db), m_auth(auth), m_host(std::move(hostName)) {}
    QString hostName() const override { return m_host; }
    QList<ShareItem> items(qint64 guestId) const override;
    QString pathFor(qint64 guestId, const QString &itemId) const override;
    QString idForPath(qint64 guestId, const QString &path) const override;
    static QString kindOf(const QString &path);
    // Lets the app say more about a file than its name (tags for music, the title the host gave a film).
    void setDescriber(std::function<void(const QString &path, ShareItem &item)> f) { m_describe = std::move(f); }
private:
    QString idFor(qint64 guestId, const QString &path) const;
    ProfileDb &m_db;
    ShareAuth &m_auth;
    QString m_host;
    std::function<void(const QString &, ShareItem &)> m_describe;
};

// The host's side of library sharing (docs/design-sharing.md). It listens only while started. Every connection is
// encrypted with TLS using a pre-shared key (no certificates): an invite code for a guest's first visit, and the
// guest's private key after that. There is no endpoint that hands over a whole file as a download, nothing is
// served that the host did not approve for that guest, and approval is checked again as a stream goes on.
//
//   GET  /v1/hello           -> {"app":"hop-tunes","protocol":1,"name":<host>}
//   POST /v1/redeem          (invite connections only) {"name":<guest>} -> {"key":<hex>,"guest":<id>}
//   GET  /v1/library         -> {"items":[{id,title,kind,size}]}   only what is approved
//   GET  /v1/stream/<id>     -> the item, honouring Range
class ShareServer : public QObject {
    Q_OBJECT
public:
    ShareServer(ProfileDb &db, ShareAuth &auth, ShareLibrary &library, QObject *parent = nullptr);
    ~ShareServer() override;

    static constexpr int kMaxConnections = 8;
    static constexpr int kMaxHeaderBytes = 16 * 1024;
    static constexpr int kMaxBodyBytes = 4096;

    // False when encryption is not available on this machine (no TLS library) or the port is taken.
    bool start(quint16 port = 0, bool allowInternet = false);
    void stop();
    void setPartyProvider(std::function<PartyInfo()> f) { m_party = std::move(f); }
    void setAllowInternet(bool on) { m_allowInternet = on; }   // otherwise only home-network addresses may connect
    bool allowInternet() const { return m_allowInternet; }
    bool isListening() const;
    quint16 port() const;
    static bool encryptionAvailable();

    // Guests that have a live, authenticated connection right now (the host's "connected" dot).
    QList<qint64> connectedGuests() const;
    // Close every connection of this guest (they were removed, or an approval was withdrawn).
    void kick(qint64 guestId);

    // Whether a peer is on a home network (loopback, private, link-local); anything else needs "allow internet".
    static bool isLocalAddress(const QHostAddress &a);
    // Whether a connection from this address is let in right now: home-network addresses always, others only with "allow internet".
    bool acceptsPeer(const QHostAddress &a) const { return m_allowInternet || isLocalAddress(a); }

signals:
    void connectedGuestsChanged();
    void inviteRedeemed(qint64 guestId);

private:
    friend class ShareConnection;
    void onPeer(QSslSocket *socket);
    void onPsk(QSslSocket *socket, QSslPreSharedKeyAuthenticator *auth);
    void onSocketError(QSslSocket *socket);
    void connectionGone(ShareConnection *c);

    ProfileDb &m_db;
    ShareAuth &m_auth;
    ShareLibrary &m_lib;
    QSslServer *m_server = nullptr;
    bool m_allowInternet = false;
    QHash<QSslSocket *, QString> m_identities;   // handshake identity per socket, to count failed invite attempts
    QList<ShareConnection *> m_conns;
    std::function<PartyInfo()> m_party;
};

} // namespace hop
