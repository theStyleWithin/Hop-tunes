// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <memory>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QSslSocket>
#include <QString>
#include <QTcpServer>
#include <QTimer>

#include <functional>

namespace hop {

// Where a guest connects, and the shared key that proves who they are (an invite code's secret on a first visit,
// the guest's private key afterwards). See docs/design-sharing.md.
struct ShareEndpoint {
    QString address;
    quint16 port = 0;
    QByteArray identity;   // "invite:<label>" or "guest:<id>"
    QByteArray key;
};

// One request to a sharing host over its encrypted connection. The reply arrives in pieces so a big stream never sits in
// memory: responseStarted, then bodyData for each piece, then finished. A request can be paused (the reader is slow);
// the connection then stops being read and the host slows down by itself.
class ShareRequest : public QObject {
    Q_OBJECT
public:
    ShareRequest(const ShareEndpoint &endpoint, const QByteArray &method, const QByteArray &path,
                 const QByteArray &extraHeaders = {}, const QByteArray &body = {}, QObject *parent = nullptr);
    ~ShareRequest() override;
    void start();
    void abort();                 // stop at once; finished(false) is not sent
    void setPaused(bool paused);
    bool isFinished() const { return m_done; }

signals:
    void responseStarted(int status, const QHash<QByteArray, QByteArray> &headers);   // header names are lower case
    void bodyData(const QByteArray &chunk);
    void finished(bool ok, const QString &error);

private:
    void onReadyRead();
    void finish(bool ok, const QString &error);
    ShareEndpoint m_ep;
    QByteArray m_request;
    QSslSocket *m_socket = nullptr;
    QTimer m_timeout;
    QByteArray m_head;
    bool m_gotHead = false, m_paused = false, m_done = false, m_noBody = false;
    qint64 m_want = 0, m_have = 0;
};

// Small things the guest's app needs from a host.
namespace ShareClient {

struct Invite {
    QString address;
    quint16 port = 0;
    QString code;   // as typed, normalized by the caller when needed
};
// "hop://192.168.1.20:47800/ABCD-EFGH-JKMN-PQRS" (or an IPv6 address in brackets). False when it is not an invite.
bool parseInvite(const QString &text, Invite *out);

// A request whose reply is a small JSON object; calls back once with the HTTP status (0: could not connect) and the object.
void fetchJson(const ShareEndpoint &ep, const QByteArray &method, const QByteArray &path, const QByteArray &body,
               QObject *context, std::function<void(int status, const QJsonObject &obj, const QString &error)> done);

// Use an invite: connect with its code, give a name, get the guest's id and private key.
void redeem(const Invite &invite, const QString &guestName, QObject *context,
            std::function<void(bool ok, qint64 guestId, const QString &key, const QString &error)> done);

} // namespace ShareClient

// A tiny web server on this computer only (127.0.0.1) that lets the video player read a shared item as an ordinary
// address. It forwards each request, Range included, to the host over the encrypted connection and passes the bytes
// back; nothing is written to disk. Every address contains a random token, so only what the app asked for is reachable.
class ShareProxy : public QObject {
    Q_OBJECT
public:
    explicit ShareProxy(QObject *parent = nullptr);
    bool start();                                  // listens on a free local port; false if it cannot
    quint16 port() const { return m_server.serverPort(); }
    // The address a player can open for one item of one host (the host's /v1/stream/<id>).
    QString urlFor(const ShareEndpoint &host, const QString &itemId);
    void forgetAll();

private:
    void onConnection();
    friend class ProxyConnection;
    struct Target { ShareEndpoint ep; QString itemId; };
    QTcpServer m_server;
    QHash<QString, Target> m_targets;
};

} // namespace hop
