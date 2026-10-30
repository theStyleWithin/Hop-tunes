// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/ShareClient.h"

#include "library/ShareAuth.h"

#include <QJsonDocument>
#include <QRandomGenerator>
#include <QSslCipher>
#include <QSslConfiguration>
#include <QSslPreSharedKeyAuthenticator>
#include <QUrl>

namespace hop {

// ---- one request ---------------------------------------------------------------------------------------

ShareRequest::ShareRequest(const ShareEndpoint &endpoint, const QByteArray &method, const QByteArray &path,
                           const QByteArray &extraHeaders, const QByteArray &body, QObject *parent)
    : QObject(parent), m_ep(endpoint), m_noBody(method == "HEAD")
{
    m_request = method + " " + path + " HTTP/1.1\r\nHost: share\r\n" + extraHeaders;
    if (!body.isEmpty()) m_request += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    m_request += "\r\n" + body;
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] { finish(false, tr("the host did not answer")); });
}

ShareRequest::~ShareRequest() { abort(); }

void ShareRequest::start()
{
    QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
    conf.setProtocol(QSsl::TlsV1_2);
    QList<QSslCipher> ciphers;
    for (const char *n : {"ECDHE-PSK-CHACHA20-POLY1305", "DHE-PSK-AES256-GCM-SHA384", "DHE-PSK-AES128-GCM-SHA256", "DHE-PSK-CHACHA20-POLY1305"}) {
        const QSslCipher c(QString::fromLatin1(n), QSsl::TlsV1_2);
        if (!c.isNull()) ciphers << c;
    }
    if (ciphers.isEmpty() || !QSslSocket::supportsSsl()) { finish(false, tr("encryption is not available on this computer")); return; }
    conf.setCiphers(ciphers);
    conf.setPeerVerifyMode(QSslSocket::VerifyNone);   // the shared key is the authentication
    m_socket = new QSslSocket(this);
    m_socket->setSslConfiguration(conf);
    m_socket->setReadBufferSize(256 * 1024);          // a full buffer stops reading, which slows the host down by itself
    connect(m_socket, &QSslSocket::preSharedKeyAuthenticationRequired, this, [this](QSslPreSharedKeyAuthenticator *a) {
        a->setIdentity(m_ep.identity);
        a->setPreSharedKey(m_ep.key);
    });
    connect(m_socket, &QSslSocket::encrypted, this, [this] { m_socket->write(m_request); m_timeout.start(30000); });
    connect(m_socket, &QSslSocket::readyRead, this, [this] { onReadyRead(); });
    connect(m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (!m_done) finish(false, m_socket->isEncrypted() ? tr("the connection was lost")
                                                          : tr("could not connect (is the address right, and is the host sharing?)"));
    });
    connect(m_socket, &QAbstractSocket::disconnected, this, [this] {
        if (!m_done) finish(m_gotHead && m_have >= m_want, tr("the connection was lost"));
    });
    m_timeout.start(10000);
    m_socket->connectToHostEncrypted(m_ep.address, m_ep.port);
}

void ShareRequest::abort()
{
    m_done = true;
    m_timeout.stop();
    if (m_socket) { m_socket->disconnect(this); m_socket->abort(); m_socket->deleteLater(); m_socket = nullptr; }
}

void ShareRequest::setPaused(bool paused)
{
    if (m_paused == paused) return;
    m_paused = paused;
    if (!paused && m_socket && m_socket->bytesAvailable() > 0) QMetaObject::invokeMethod(this, [this] { onReadyRead(); }, Qt::QueuedConnection);
}

void ShareRequest::finish(bool ok, const QString &error)
{
    if (m_done) return;
    m_done = true;
    m_timeout.stop();
    if (m_socket) { m_socket->disconnect(this); m_socket->abort(); m_socket->deleteLater(); m_socket = nullptr; }
    emit finished(ok, ok ? QString() : error);
}

void ShareRequest::onReadyRead()
{
    if (m_done || !m_socket) return;
    while (!m_paused && !m_done && m_socket && m_socket->bytesAvailable() > 0) {
        m_timeout.start(30000);
        const QByteArray data = m_socket->read(64 * 1024);
        if (data.isEmpty()) break;
        if (!m_gotHead) {
            m_head += data;
            const int e = m_head.indexOf("\r\n\r\n");
            if (e < 0) { if (m_head.size() > 32 * 1024) finish(false, tr("an unexpected reply")); continue; }
            const QList<QByteArray> lines = m_head.left(e).split('\n');
            const QList<QByteArray> first = lines.first().trimmed().split(' ');
            QHash<QByteArray, QByteArray> headers;
            for (int i = 1; i < lines.size(); ++i) {
                const int c = lines[i].indexOf(':');
                if (c > 0) headers.insert(lines[i].left(c).trimmed().toLower(), lines[i].mid(c + 1).trimmed());
            }
            if (first.size() < 2) { finish(false, tr("an unexpected reply")); return; }
            m_gotHead = true;
            m_want = m_noBody ? 0 : headers.value("content-length", "0").toLongLong();
            const QByteArray rest = m_head.mid(e + 4);
            m_head.clear();
            emit responseStarted(first[1].toInt(), headers);
            if (m_done) return;
            if (!rest.isEmpty()) { m_have += rest.size(); emit bodyData(rest); }
        } else {
            m_have += data.size();
            emit bodyData(data);
        }
        if (m_gotHead && m_have >= m_want) { finish(true, QString()); return; }
    }
    if (m_gotHead && !m_done && m_have >= m_want) finish(true, QString());
}

// ---- helpers for the guest's app -----------------------------------------------------------------------

namespace ShareClient {

bool parseInvite(const QString &text, Invite *out)
{
    const QUrl u(text.trimmed());
    if (!u.isValid() || u.scheme() != QLatin1String("hop") || u.host().isEmpty() || u.port() <= 0 || u.port() > 65535) return false;
    const QString code = ShareAuth::normalizeCode(u.path().mid(1));
    if (code.size() != ShareAuth::kCodeLength) return false;
    if (out) { out->address = u.host(); out->port = quint16(u.port()); out->code = code; }
    return true;
}

void fetchJson(const ShareEndpoint &ep, const QByteArray &method, const QByteArray &path, const QByteArray &body,
               QObject *context, std::function<void(int, const QJsonObject &, const QString &)> done)
{
    auto *req = new ShareRequest(ep, method, path, {}, body, context);
    auto status = std::make_shared<int>(0);
    auto data = std::make_shared<QByteArray>();
    QObject::connect(req, &ShareRequest::responseStarted, req, [status](int s, const QHash<QByteArray, QByteArray> &) { *status = s; });
    QObject::connect(req, &ShareRequest::bodyData, req, [data](const QByteArray &d) { *data += d; });
    QObject::connect(req, &ShareRequest::finished, req, [req, status, data, done](bool ok, const QString &err) {
        done(ok ? *status : 0, ok ? QJsonDocument::fromJson(*data).object() : QJsonObject(), err);
        req->deleteLater();
    });
    req->start();
}

void redeem(const Invite &invite, const QString &guestName, QObject *context,
            std::function<void(bool, qint64, const QString &, const QString &)> done)
{
    const QString code = ShareAuth::normalizeCode(invite.code);
    ShareEndpoint ep{invite.address, invite.port, "invite:" + code.left(ShareAuth::kLabelLength).toLatin1(), ShareAuth::secretFromCode(code)};
    const QByteArray body = QJsonDocument(QJsonObject{{"name", guestName.left(60)}}).toJson(QJsonDocument::Compact);
    fetchJson(ep, "POST", "/v1/redeem", body, context, [done](int status, const QJsonObject &o, const QString &err) {
        const QString key = o.value("key").toString();
        if (status == 200 && key.size() == 64) done(true, qint64(o.value("guest").toDouble()), key, QString());
        else done(false, 0, QString(), err.isEmpty() ? QObject::tr("the host did not accept the invite") : err);
    });
}

} // namespace ShareClient

// ---- the local proxy ------------------------------------------------------------------------------------

// One player request being passed through to the host.
class ProxyConnection : public QObject {
public:
    ProxyConnection(ShareProxy *proxy, QTcpSocket *client) : QObject(proxy), m_proxy(proxy), m_client(client)
    {
        client->setParent(this);
        connect(client, &QTcpSocket::readyRead, this, [this] { m_buf += m_client->readAll(); parse(); });
        connect(client, &QTcpSocket::disconnected, this, [this] { deleteLater(); });
        connect(client, &QTcpSocket::bytesWritten, this, [this] { resume(); });
    }
private:
    ShareProxy *m_proxy;
    QTcpSocket *m_client;
    QByteArray m_buf;
    QPointer<ShareRequest> m_req;
    bool m_started = false, m_headSent = false;

    void reply(int status, const QByteArray &why)
    {
        m_client->write("HTTP/1.1 " + QByteArray::number(status) + " " + why + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        m_client->disconnectFromHost();
    }
    void resume() { if (m_req && m_client->bytesToWrite() < 128 * 1024) m_req->setPaused(false); }

    void parse()
    {
        if (m_started) return;
        const int e = m_buf.indexOf("\r\n\r\n");
        if (e < 0) { if (m_buf.size() > 16 * 1024) reply(431, "Too Large"); return; }
        const QList<QByteArray> lines = m_buf.left(e).split('\n');
        const QList<QByteArray> first = lines.first().trimmed().split(' ');
        if (first.size() != 3 || (first[0] != "GET" && first[0] != "HEAD")) { reply(405, "Method Not Allowed"); return; }
        QByteArray range;
        for (int i = 1; i < lines.size(); ++i)
            if (lines[i].trimmed().toLower().startsWith("range:")) range = lines[i].mid(lines[i].indexOf(':') + 1).trimmed();
        const QString path = QString::fromUtf8(first[1]);
        if (!path.startsWith(QLatin1String("/s/"))) { reply(404, "Not Found"); return; }
        const auto it = m_proxy->m_targets.constFind(path.mid(3));
        if (it == m_proxy->m_targets.constEnd()) { reply(404, "Not Found"); return; }
        m_started = true;
        const QByteArray extra = range.isEmpty() ? QByteArray() : "Range: " + range + "\r\n";
        m_req = new ShareRequest(it->ep, first[0], "/v1/stream/" + it->itemId.toLatin1(), extra, {}, this);
        connect(m_req, &ShareRequest::responseStarted, this, [this](int status, const QHash<QByteArray, QByteArray> &h) {
            QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + (status == 206 ? " Partial Content" : status == 200 ? " OK" : " Error") + "\r\n";
            for (const char *name : {"content-type", "content-length", "content-range", "accept-ranges"})
                if (h.contains(name)) head += QByteArray(name) + ": " + h.value(name) + "\r\n";
            head += "Connection: close\r\n\r\n";
            m_headSent = true;
            m_client->write(head);
        });
        connect(m_req, &ShareRequest::bodyData, this, [this](const QByteArray &d) {
            m_client->write(d);
            if (m_client->bytesToWrite() > 512 * 1024) m_req->setPaused(true);   // the player is slow: let the host wait
        });
        connect(m_req, &ShareRequest::finished, this, [this](bool, const QString &) {
            if (!m_headSent) reply(502, "Bad Gateway");   // the host could not be reached: say so rather than just hang up
            else m_client->disconnectFromHost();
        });
        m_req->start();
    }
};

ShareProxy::ShareProxy(QObject *parent) : QObject(parent) { connect(&m_server, &QTcpServer::newConnection, this, &ShareProxy::onConnection); }

bool ShareProxy::start()
{
    return m_server.isListening() || m_server.listen(QHostAddress::LocalHost, 0);   // this computer only
}

void ShareProxy::onConnection()
{
    while (QTcpSocket *s = m_server.nextPendingConnection()) new ProxyConnection(this, s);
}

QString ShareProxy::urlFor(const ShareEndpoint &host, const QString &itemId)
{
    QByteArray raw(16, 0);
    for (int i = 0; i < raw.size(); ++i) raw[i] = char(QRandomGenerator::system()->bounded(256));
    const QString token = QString::fromLatin1(raw.toHex());
    m_targets.insert(token, Target{host, itemId});
    return QStringLiteral("http://127.0.0.1:%1/s/%2").arg(port()).arg(token);
}

void ShareProxy::forgetAll() { m_targets.clear(); }

} // namespace hop
