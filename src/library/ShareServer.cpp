// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/ShareServer.h"

#include "library/ProfileDb.h"
#include "library/ShareAuth.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QMimeDatabase>
#include <QPointer>
#include <QRandomGenerator>
#include <QSslCipher>
#include <QSslConfiguration>
#include <QSslPreSharedKeyAuthenticator>
#include <QSslServer>
#include <QSslSocket>
#include <QTimer>

#include <algorithm>

namespace hop {

// ---- the library the host approved for a guest ---------------------------------------------------------

QString GrantLibrary::kindOf(const QString &path)
{
    static const QSet<QString> video{"mkv", "mp4", "webm", "mov", "avi", "wmv", "m4v", "mpg", "mpeg", "ogv", "flv", "ts"};
    static const QSet<QString> audio{"mp3", "flac", "ogg", "oga", "opus", "m4a", "m4b", "aac", "wav", "wma", "aiff", "alac"};
    static const QSet<QString> picture{"jpg", "jpeg", "png", "webp", "gif", "bmp", "tif", "tiff", "avif", "heic"};
    static const QSet<QString> book{"epub", "pdf"};
    const QString e = QFileInfo(path).suffix().toLower();
    return video.contains(e) ? "video" : audio.contains(e) ? "audio" : picture.contains(e) ? "picture"
         : book.contains(e) ? "book" : "file";
}

// Ids are per guest, so one guest's id means nothing to another, and no path ever leaves this machine.
QString GrantLibrary::idFor(qint64 guestId, const QString &path) const
{
    const QByteArray secret = m_auth.guestSecret(guestId);
    if (secret.isEmpty()) return {};
    return QString::fromLatin1(QMessageAuthenticationCode::hash(path.toUtf8(), secret, QCryptographicHash::Sha256).toHex().left(32));
}

QList<ShareItem> GrantLibrary::items(qint64 guestId) const
{
    QList<ShareItem> out;
    for (const QString &p : m_db.shareGrants(guestId)) {
        const QFileInfo fi(p);
        if (!fi.isFile()) continue;
        out.append(ShareItem{idFor(guestId, p), fi.completeBaseName(), kindOf(p), fi.size()});
    }
    std::sort(out.begin(), out.end(), [](const ShareItem &a, const ShareItem &b) {
        const int c = a.title.compare(b.title, Qt::CaseInsensitive);
        return c ? c < 0 : a.id < b.id;
    });
    return out;
}

QString GrantLibrary::pathFor(qint64 guestId, const QString &itemId) const
{
    if (itemId.size() != 32) return {};
    for (const QString &p : m_db.shareGrants(guestId))
        if (idFor(guestId, p) == itemId) return QFileInfo(p).isFile() ? p : QString();
    return {};
}

// ---- one guest's (or invite's) connection --------------------------------------------------------------

class ShareConnection : public QObject {
public:
    ShareConnection(ShareServer *server, QSslSocket *socket, const QString &identity)
        : QObject(server), m_server(server), m_socket(socket), m_identity(identity)
    {
        if (identity.startsWith(QLatin1String("guest:"))) m_guestId = identity.mid(6).toLongLong();
        else if (identity.startsWith(QLatin1String("invite:"))) m_label = identity.mid(7);
        socket->setParent(this);
        m_idle.setInterval(60 * 1000);
        m_idle.setSingleShot(true);
        connect(&m_idle, &QTimer::timeout, this, [this] { m_socket->disconnectFromHost(); });
        connect(socket, &QSslSocket::readyRead, this, [this] { m_idle.start(); m_buf += m_socket->readAll(); parse(); });
        connect(socket, &QSslSocket::bytesWritten, this, [this] { m_idle.start(); pump(); });
        connect(socket, &QSslSocket::disconnected, this, [this] { m_server->connectionGone(this); });
        m_idle.start();
    }
    qint64 guestId() const { return m_guestId; }
    bool isGuest() const { return m_guestId > 0; }
    QSslSocket *socket() const { return m_socket; }

private:
    ShareServer *m_server;
    QSslSocket *m_socket;
    QString m_identity;
    qint64 m_guestId = 0;
    QString m_label;
    QByteArray m_buf;
    QTimer m_idle;
    // a stream in progress
    bool m_streaming = false;
    QFile m_file;
    QString m_streamPath;
    qint64 m_pos = 0, m_end = -1;

    void send(int status, const QByteArray &type, const QByteArray &body, const QByteArray &extra = {}, bool close = false,
              bool head = false, qint64 length = -1)
    {
        static const QHash<int, QByteArray> reason{{200, "OK"}, {206, "Partial Content"}, {400, "Bad Request"}, {401, "Unauthorized"},
            {403, "Forbidden"}, {404, "Not Found"}, {405, "Method Not Allowed"}, {413, "Payload Too Large"},
            {416, "Range Not Satisfiable"}, {431, "Request Header Fields Too Large"}, {500, "Internal Server Error"}};
        QByteArray h = "HTTP/1.1 " + QByteArray::number(status) + " " + reason.value(status, "Error") + "\r\n";
        h += "Content-Type: " + type + "\r\nContent-Length: " + QByteArray::number(length >= 0 ? length : body.size()) + "\r\n";
        h += "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n";
        h += close ? "Connection: close\r\n" : "Connection: keep-alive\r\n";
        h += extra + "\r\n";
        m_socket->write(h);
        if (!head) m_socket->write(body);
        if (close) m_socket->disconnectFromHost();
    }
    void json(int status, const QJsonObject &o, bool close = false)
    {
        send(status, "application/json", QJsonDocument(o).toJson(QJsonDocument::Compact), {}, close);
    }
    void fail(int status, bool close = false) { json(status, QJsonObject{{"error", status}}, close); }

    void parse()
    {
        while (!m_streaming && !m_buf.isEmpty() && m_socket->isOpen()) {
            const int end = m_buf.indexOf("\r\n\r\n");
            if (end < 0) {
                if (m_buf.size() > ShareServer::kMaxHeaderBytes) { fail(431, true); m_buf.clear(); }
                return;
            }
            if (end > ShareServer::kMaxHeaderBytes) { fail(431, true); m_buf.clear(); return; }
            const QList<QByteArray> lines = m_buf.left(end).split('\n');
            const QList<QByteArray> first = lines.first().trimmed().split(' ');
            QHash<QByteArray, QByteArray> headers;
            for (int i = 1; i < lines.size(); ++i) {
                const int c = lines[i].indexOf(':');
                if (c > 0) headers.insert(lines[i].left(c).trimmed().toLower(), lines[i].mid(c + 1).trimmed());
            }
            bool ok = true;
            const qint64 len = headers.value("content-length", "0").toLongLong(&ok);
            if (first.size() != 3 || !ok || len < 0) { fail(400, true); m_buf.clear(); return; }
            if (len > ShareServer::kMaxBodyBytes) { fail(413, true); m_buf.clear(); return; }
            if (m_buf.size() < end + 4 + len) return;   // the body has not all arrived yet
            const QByteArray body = m_buf.mid(end + 4, int(len));
            m_buf.remove(0, end + 4 + int(len));
            handle(first[0], first[1], headers, body);
        }
    }

    void handle(const QByteArray &method, const QByteArray &target, const QHash<QByteArray, QByteArray> &headers,
                const QByteArray &body)
    {
        const QString path = QString::fromUtf8(target).section(QLatin1Char('?'), 0, 0);
        if (m_guestId > 0 && m_server->m_auth.guestSecret(m_guestId).isEmpty()) { fail(401, true); return; }   // removed
        if (!isGuest()) {   // an invite connection: one thing only, trade the invite for a key
            if (method != "POST" || path != QLatin1String("/v1/redeem")) { fail(403, true); return; }
            const QJsonObject o = QJsonDocument::fromJson(body).object();
            qint64 gid = 0;
            const QString key = m_server->m_auth.redeemInvite(m_label, o.value("name").toString().left(60),
                                                              QDateTime::currentSecsSinceEpoch(), &gid);
            if (key.isEmpty()) { fail(403, true); return; }
            json(200, QJsonObject{{"key", key}, {"guest", double(gid)}}, true);
            emit m_server->inviteRedeemed(gid);
            return;
        }
        const bool head = method == "HEAD";
        if (method != "GET" && !head) { fail(405); return; }
        if (path == QLatin1String("/v1/hello")) {
            json(200, QJsonObject{{"app", "hop-tunes"}, {"protocol", 1}, {"name", m_server->m_lib.hostName()}});
        } else if (path == QLatin1String("/v1/library")) {
            QJsonArray items;
            for (const ShareItem &it : m_server->m_lib.items(m_guestId))
                items.append(QJsonObject{{"id", it.id}, {"title", it.title}, {"kind", it.kind}, {"size", double(it.size)}});
            json(200, QJsonObject{{"items", items}});
        } else if (path.startsWith(QLatin1String("/v1/stream/"))) {
            stream(path.mid(11), headers.value("range"), head);
        } else {
            fail(404);   // there is no download or file-listing route
        }
    }

    void stream(const QString &id, const QByteArray &range, bool head)
    {
        const QString p = m_server->m_lib.pathFor(m_guestId, id);
        if (p.isEmpty()) { fail(404); return; }
        QFile probe(p);
        if (!probe.open(QIODevice::ReadOnly)) { fail(404); return; }
        const qint64 size = probe.size();
        qint64 a = 0, b = size - 1;
        int status = 200;
        QByteArray extra = "Accept-Ranges: bytes\r\nContent-Disposition: inline\r\n";
        if (!range.isEmpty()) {
            static const QByteArray unit = "bytes=";
            if (!range.startsWith(unit) || range.contains(',')) { send(416, "application/json", "{}", "Content-Range: bytes */" + QByteArray::number(size) + "\r\n"); return; }
            const QList<QByteArray> parts = range.mid(unit.size()).trimmed().split('-');
            bool okA = true, okB = true;
            if (parts.size() != 2) okA = okB = false;
            else if (parts[0].isEmpty()) {            // the last N bytes
                const qint64 n = parts[1].toLongLong(&okB);
                okA = okB && n > 0;
                if (okA) a = std::max<qint64>(0, size - n);
            } else {
                a = parts[0].toLongLong(&okA);
                if (!parts[1].isEmpty()) b = std::min<qint64>(parts[1].toLongLong(&okB), size - 1);
            }
            if (!okA || !okB || a < 0 || a >= size || b < a) {
                send(416, "application/json", "{}", "Content-Range: bytes */" + QByteArray::number(size) + "\r\n"); return;
            }
            status = 206;
            extra += "Content-Range: bytes " + QByteArray::number(a) + "-" + QByteArray::number(b) + "/" + QByteArray::number(size) + "\r\n";
        }
        const QByteArray type = QMimeDatabase().mimeTypeForFile(p, QMimeDatabase::MatchExtension).name().toUtf8();
        const qint64 length = size == 0 ? 0 : b - a + 1;
        send(status, type, {}, extra, false, true, length);   // the headers; the bytes follow below
        if (head || length == 0) return;
        m_file.setFileName(p);
        if (!m_file.open(QIODevice::ReadOnly) || !m_file.seek(a)) { m_socket->disconnectFromHost(); return; }
        m_streamPath = p;
        m_pos = a; m_end = b;
        m_streaming = true;
        pump();
    }

    void pump()
    {
        if (!m_streaming) return;
        while (m_socket->isOpen() && m_socket->bytesToWrite() < 256 * 1024 && m_pos <= m_end) {
            // approval is checked again as the stream goes on: withdrawing it (or removing the guest) cuts the stream
            if (!m_server->m_db.isShared(m_guestId, m_streamPath) || m_server->m_auth.guestSecret(m_guestId).isEmpty()) {
                m_streaming = false; m_file.close(); m_socket->abort(); return;
            }
            const QByteArray chunk = m_file.read(std::min<qint64>(64 * 1024, m_end - m_pos + 1));
            if (chunk.isEmpty()) { m_streaming = false; m_file.close(); m_socket->abort(); return; }
            m_socket->write(chunk);
            m_pos += chunk.size();
        }
        if (m_pos > m_end) {
            m_streaming = false;
            m_file.close();
            if (!m_buf.isEmpty()) parse();   // a request that arrived while the stream was going
        }
    }
};

// ---- the server ----------------------------------------------------------------------------------------

ShareServer::ShareServer(ProfileDb &db, ShareAuth &auth, ShareLibrary &library, QObject *parent)
    : QObject(parent), m_db(db), m_auth(auth), m_lib(library) {}

ShareServer::~ShareServer() { stop(); }

bool ShareServer::encryptionAvailable() { return QSslSocket::supportsSsl(); }

bool ShareServer::isLocalAddress(const QHostAddress &addr)
{
    bool mapped = false;
    const quint32 v4 = addr.toIPv4Address(&mapped);   // also unwraps ::ffff:a.b.c.d
    if (mapped) return QHostAddress(v4).isInSubnet(QHostAddress(QStringLiteral("10.0.0.0")), 8)
                    || QHostAddress(v4).isInSubnet(QHostAddress(QStringLiteral("172.16.0.0")), 12)
                    || QHostAddress(v4).isInSubnet(QHostAddress(QStringLiteral("192.168.0.0")), 16)
                    || QHostAddress(v4).isInSubnet(QHostAddress(QStringLiteral("169.254.0.0")), 16)
                    || QHostAddress(v4).isLoopback();
    return addr.isLoopback()
        || addr.isInSubnet(QHostAddress(QStringLiteral("fc00::")), 7)
        || addr.isInSubnet(QHostAddress(QStringLiteral("fe80::")), 10);
}

bool ShareServer::start(quint16 port, bool allowInternet)
{
    stop();
    if (!encryptionAvailable()) return false;
    // TLS 1.2 with a pre-shared key and forward secrecy; no certificates and no plain ciphers.
    QList<QSslCipher> ciphers;
    for (const char *n : {"ECDHE-PSK-CHACHA20-POLY1305", "DHE-PSK-AES256-GCM-SHA384", "DHE-PSK-AES128-GCM-SHA256",
                          "DHE-PSK-CHACHA20-POLY1305"}) {
        const QSslCipher c(QString::fromLatin1(n), QSsl::TlsV1_2);
        if (!c.isNull()) ciphers << c;
    }
    if (ciphers.isEmpty()) return false;
    QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
    conf.setProtocol(QSsl::TlsV1_2);
    conf.setCiphers(ciphers);
    conf.setPeerVerifyMode(QSslSocket::VerifyNone);   // authentication is the shared key, not a certificate
    m_server = new QSslServer(this);
    m_server->setSslConfiguration(conf);
    m_server->setHandshakeTimeout(5000);
    m_allowInternet = allowInternet;
    connect(m_server, &QSslServer::preSharedKeyAuthenticationRequired, this,
            [this](QSslSocket *s, QSslPreSharedKeyAuthenticator *a) { onPsk(s, a); });
    connect(m_server, &QSslServer::errorOccurred, this, [this](QSslSocket *s, QAbstractSocket::SocketError) { onSocketError(s); });
    connect(m_server, &QSslServer::pendingConnectionAvailable, this, [this] {
        while (QTcpSocket *t = m_server->nextPendingConnection()) onPeer(qobject_cast<QSslSocket *>(t));
    });
    if (!m_server->listen(QHostAddress::Any, port)) { delete m_server; m_server = nullptr; return false; }
    return true;
}

void ShareServer::stop()
{
    if (m_server) { m_server->close(); delete m_server; m_server = nullptr; }
    const QList<ShareConnection *> conns = m_conns;
    for (ShareConnection *c : conns) { c->socket()->abort(); }
    qDeleteAll(m_conns);
    m_conns.clear();
    m_identities.clear();
    emit connectedGuestsChanged();
}

bool ShareServer::isListening() const { return m_server && m_server->isListening(); }
quint16 ShareServer::port() const { return m_server ? m_server->serverPort() : 0; }

QList<qint64> ShareServer::connectedGuests() const
{
    QSet<qint64> ids;
    for (const ShareConnection *c : m_conns) if (c->isGuest()) ids.insert(c->guestId());
    QList<qint64> out(ids.begin(), ids.end());
    std::sort(out.begin(), out.end());
    return out;
}

void ShareServer::kick(qint64 guestId)
{
    for (ShareConnection *c : m_conns) if (c->guestId() == guestId) c->socket()->abort();
}

void ShareServer::onPsk(QSslSocket *socket, QSslPreSharedKeyAuthenticator *auth)
{
    const QString id = QString::fromUtf8(auth->identity());
    m_identities.insert(socket, id);
    QByteArray key;
    if (id.startsWith(QLatin1String("guest:"))) key = m_auth.guestSecret(id.mid(6).toLongLong());
    else if (id.startsWith(QLatin1String("invite:"))) key = m_auth.inviteSecret(id.mid(7), QDateTime::currentSecsSinceEpoch());
    if (key.isEmpty()) {   // unknown: a random key, so the handshake fails the same way as for a wrong one
        key.resize(32);
        for (int i = 0; i < key.size(); ++i) key[i] = char(QRandomGenerator::system()->bounded(256));
    }
    auth->setPreSharedKey(key);
}

void ShareServer::onSocketError(QSslSocket *socket)
{
    if (!socket || socket->isEncrypted()) return;
    const QString id = m_identities.take(socket);
    if (id.startsWith(QLatin1String("invite:"))) m_auth.noteInviteMiss(id.mid(7));
}

void ShareServer::onPeer(QSslSocket *socket)
{
    if (!socket) return;
    const QString identity = m_identities.take(socket);
    if (m_conns.size() >= kMaxConnections || (!m_allowInternet && !isLocalAddress(socket->peerAddress()))
        || !(identity.startsWith(QLatin1String("guest:")) || identity.startsWith(QLatin1String("invite:")))) {
        socket->abort();
        socket->deleteLater();
        return;
    }
    auto *c = new ShareConnection(this, socket, identity);
    m_conns.append(c);
    if (c->isGuest()) emit connectedGuestsChanged();
}

void ShareServer::connectionGone(ShareConnection *c)
{
    if (!m_conns.removeOne(c)) return;
    const bool guest = c->isGuest();
    c->deleteLater();
    if (guest) emit connectedGuestsChanged();
}

} // namespace hop
