// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps/KickAuth.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QUrlQuery>

namespace hop {

namespace {
constexpr const char *kApp = "kick";
const QString kIdHost = QStringLiteral("https://id.kick.com");

QString base64Url(const QByteArray &b) { return QString::fromLatin1(b.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)); }
}

KickAuth::KickAuth(AppNet *net, QObject *parent) : QObject(parent), m_net(net)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(5 * 60 * 1000);                          // a sign-in that is not finished in five minutes is dropped
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        stopListening();
        setState(QStringLiteral("error"), QStringLiteral("the sign-in was not finished in time: start again"));
    });
}

// ---- the pieces ------------------------------------------------------------------------------------------------

QString KickAuth::pkceChallenge(const QString &verifier)
{
    return base64Url(QCryptographicHash::hash(verifier.toLatin1(), QCryptographicHash::Sha256));
}

QString KickAuth::randomToken(int length, bool urlSafe)
{
    static const char unreserved[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";   // RFC 7636's characters
    static const char hex[] = "0123456789abcdef";
    QString out;
    for (int i = 0; i < length; ++i) {
        if (urlSafe) out += QLatin1Char(unreserved[QRandomGenerator::system()->bounded(int(sizeof(unreserved) - 1))]);
        else out += QLatin1Char(hex[QRandomGenerator::system()->bounded(16)]);
    }
    return out;
}

QByteArray KickAuth::form(const QList<QPair<QString, QString>> &fields)
{
    QUrlQuery q;
    for (const auto &f : fields) q.addQueryItem(f.first, f.second);
    return q.toString(QUrl::FullyEncoded).toUtf8();
}

QString KickAuth::redirectUri() const { return QStringLiteral("http://localhost:%1/callback").arg(port()); }

void KickAuth::setCredentials(const QString &clientId, const QString &clientSecret)
{
    const QString id = clientId.trimmed().left(200);
    const QString secret = clientSecret.trimmed().isEmpty() ? m_clientSecret : clientSecret.trimmed().left(200);      // an empty secret box keeps the saved one
    if (id == m_clientId && secret == m_clientSecret) return;
    const bool wasSignedIn = signedIn();
    m_clientId = id; m_clientSecret = secret;
    if (wasSignedIn) { m_access.clear(); m_refresh.clear(); m_name.clear(); m_expiresAt = 0; }   // tokens belong to the app that made them
    cancel();
    emit saveRequested();
    emit changed();
}

void KickAuth::setState(const QString &state, const QString &message)
{
    m_state = state; m_message = message;
    emit changed();
}

// ---- signing in ------------------------------------------------------------------------------------------------

void KickAuth::signIn()
{
    if (signedIn() || m_state == QLatin1String("waiting") || m_state == QLatin1String("working")) return;
    if (!configured()) { setState(QStringLiteral("error"), QStringLiteral("fill in your Kick app's client ID and secret first")); return; }
    stopListening();
    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::LocalHost, quint16(m_port))) {
        const QString why = m_server->errorString();
        m_server->deleteLater(); m_server = nullptr;
        setState(QStringLiteral("error"), QStringLiteral("could not wait for Kick on this computer (port %1): %2").arg(m_port).arg(why));
        return;
    }
    connect(m_server, &QTcpServer::newConnection, this, &KickAuth::onConnection);
    m_verifier = randomToken(64, true);
    m_expectedState = randomToken(32, false);
    QUrl url(kIdHost + QStringLiteral("/oauth/authorize"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    q.addQueryItem(QStringLiteral("client_id"), m_clientId);
    m_redirect = redirectUri();
    q.addQueryItem(QStringLiteral("redirect_uri"), m_redirect);
    q.addQueryItem(QStringLiteral("state"), m_expectedState);
    q.addQueryItem(QStringLiteral("scope"), QString::fromLatin1(kScope));
    q.addQueryItem(QStringLiteral("code_challenge"), pkceChallenge(m_verifier));
    q.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    url.setQuery(q);
    m_authorizeUrl = url;
    m_timeout.start();
    setState(QStringLiteral("waiting"), QStringLiteral("finish signing in on kick.com, in your browser…"));
}

void KickAuth::stopListening()
{
    m_timeout.stop();
    if (m_server) { m_server->close(); m_server->deleteLater(); m_server = nullptr; }
}

void KickAuth::cancel()
{
    ++m_gen;
    stopListening();
    m_authorizeUrl.clear(); m_verifier.clear(); m_expectedState.clear();
    if (m_state == QLatin1String("waiting") || m_state == QLatin1String("working") || m_state == QLatin1String("error")) setState(QString(), QString());
}

void KickAuth::onConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket *sock = m_server->nextPendingConnection();
        sock->setParent(this);                                      // (the listener closes right after the callback: the answer must still go out)
        connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
        QPointer<QTcpSocket> guard(sock);
        auto tryRead = [this, guard] {
            if (!guard) return;
            const QByteArray data = guard->peek(8192);
            const int eol = data.indexOf("\r\n");
            if (eol < 0) { if (data.size() >= 8192) guard->close(); return; }
            const QByteArray line = data.left(eol);
            guard->readAll();
            handleRequestLine(guard, line);
        };
        connect(sock, &QTcpSocket::readyRead, this, tryRead);
        if (sock->bytesAvailable() > 0) tryRead();
    }
}

void KickAuth::handleRequestLine(QTcpSocket *sock, const QByteArray &line)
{
    auto reply = [sock](int code, const QString &title, const QString &text) {
        const QByteArray body = QStringLiteral("<!doctype html><meta charset=utf-8><title>%1</title><body style=\"font-family:sans-serif;background:#101014;color:#eee;padding:2em\"><h2>%1</h2><p>%2</p>")
                                    .arg(title.toHtmlEscaped(), text.toHtmlEscaped()).toUtf8();
        sock->write("HTTP/1.1 " + QByteArray::number(code) + (code == 200 ? " OK" : " Not Found") + "\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nReferrer-Policy: no-referrer\r\nConnection: close\r\nContent-Length: "
                    + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        sock->disconnectFromHost();
    };
    // "GET /callback?code=…&state=… HTTP/1.1"
    const QList<QByteArray> parts = line.split(' ');
    if (parts.size() < 2 || parts.at(0) != "GET") { reply(404, QStringLiteral("not found"), QStringLiteral("nothing here")); return; }
    const QUrl url = QUrl::fromEncoded("http://localhost" + parts.at(1));
    if (url.path() != QLatin1String("/callback")) { reply(404, QStringLiteral("not found"), QStringLiteral("nothing here")); return; }   // (a browser's favicon request, for one)
    const QUrlQuery q(url);
    const QString code = q.queryItemValue(QStringLiteral("code")), st = q.queryItemValue(QStringLiteral("state")), err = q.queryItemValue(QStringLiteral("error"));
    if (st.isEmpty() || st != m_expectedState) {                                // not ours (or a replay): refuse and keep waiting
        reply(404, QStringLiteral("not found"), QStringLiteral("this sign-in did not start in Hop-tunes"));
        return;
    }
    stopListening();
    if (!err.isEmpty() || code.isEmpty()) {
        reply(200, QStringLiteral("not signed in"), QStringLiteral("Kick did not give Hop-tunes access. You can close this tab."));
        setState(QStringLiteral("error"), err == QLatin1String("access_denied") ? QStringLiteral("you did not allow Hop-tunes on kick.com") : QStringLiteral("Kick did not finish the sign-in"));
        return;
    }
    reply(200, QStringLiteral("signed in"), QStringLiteral("You can close this tab and go back to Hop-tunes."));
    setState(QStringLiteral("working"), QStringLiteral("signing in…"));
    exchangeCode(code);
}

void KickAuth::exchangeCode(const QString &code)
{
    const quint64 gen = m_gen;
    NetLimits lim; lim.post = true; lim.maxBody = 64 * 1024;
    lim.formBody = form({{QStringLiteral("grant_type"), QStringLiteral("authorization_code")}, {QStringLiteral("client_id"), m_clientId}, {QStringLiteral("client_secret"), m_clientSecret},
                         {QStringLiteral("redirect_uri"), m_redirect}, {QStringLiteral("code_verifier"), m_verifier}, {QStringLiteral("code"), code}});
    m_verifier.clear(); m_expectedState.clear(); m_authorizeUrl.clear();
    m_net->get(kApp, QUrl(kIdHost + QStringLiteral("/oauth/token")), [this, gen](NetReply r) {
        if (gen != m_gen) return;
        const QJsonObject o = QJsonDocument::fromJson(r.body).object();
        if (!r.error.isEmpty() || o.value(QStringLiteral("access_token")).toString().isEmpty()) {
            setState(QStringLiteral("error"), r.error.isEmpty() ? QStringLiteral("Kick answered, but not with a sign-in this version understands")
                                                              : QStringLiteral("Kick did not accept the sign-in (%1): check the client ID, the secret and the redirect address").arg(r.error));
            return;
        }
        applyTokens(o, false);
        setState(QString(), QString());
        fetchMe(false);
    }, lim);
}

void KickAuth::applyTokens(const QJsonObject &o, bool fromRefresh)
{
    const QString access = o.value(QStringLiteral("access_token")).toString();
    if (access.isEmpty() || access.size() > 4000) return;
    m_access = access;
    const QString refresh = o.value(QStringLiteral("refresh_token")).toString();
    if (!refresh.isEmpty() && refresh.size() <= 4000) m_refresh = refresh;                 // (a refresh may not hand out a new one)
    const QJsonValue ex = o.value(QStringLiteral("expires_in"));
    const qint64 secs = ex.isString() ? ex.toString().toLongLong() : qint64(ex.toDouble());
    m_expiresAt = QDateTime::currentSecsSinceEpoch() + (secs > 0 ? secs : 3600);
    if (!fromRefresh && m_name.isEmpty()) m_name = QStringLiteral("your Kick account");
    emit saveRequested();
    emit changed();
}

void KickAuth::fetchMe(bool mayRefresh)
{
    if (!signedIn()) return;
    const quint64 gen = m_gen;
    NetLimits lim; lim.bearer = m_access.toUtf8(); lim.maxBody = 64 * 1024;
    m_net->get(kApp, QUrl(QStringLiteral("https://api.kick.com/public/v1/users")), [this, gen, mayRefresh](NetReply r) {
        if (gen != m_gen || !signedIn()) return;
        if (r.status == 401 || r.status == 403) {
            if (mayRefresh && !m_refresh.isEmpty()) {                                    // the token ran out: one refresh, one more try
                refreshToken([this](bool ok) {
                    if (ok) fetchMe(false);
                    else { clearAll(); setState(QStringLiteral("error"), QStringLiteral("your sign-in has expired: sign in again")); emit saveRequested(); }
                });
                return;
            }
            clearAll();
            setState(QStringLiteral("error"), QStringLiteral("your sign-in has expired: sign in again"));
            emit saveRequested();
            return;
        }
        if (!r.error.isEmpty()) return;                                                  // (offline or Kick is down: the name stays as it was)
        const QJsonArray a = QJsonDocument::fromJson(r.body).object().value(QStringLiteral("data")).toArray();
        const QString name = a.isEmpty() ? QString() : a.first().toObject().value(QStringLiteral("name")).toString();
        if (!name.isEmpty()) { m_name = name.left(60); emit saveRequested(); emit changed(); }
    }, lim);
}

void KickAuth::refreshToken(std::function<void(bool)> done)
{
    const quint64 gen = m_gen;
    NetLimits lim; lim.post = true; lim.maxBody = 64 * 1024;
    lim.formBody = form({{QStringLiteral("grant_type"), QStringLiteral("refresh_token")}, {QStringLiteral("client_id"), m_clientId},
                         {QStringLiteral("client_secret"), m_clientSecret}, {QStringLiteral("refresh_token"), m_refresh}});
    m_net->get(kApp, QUrl(kIdHost + QStringLiteral("/oauth/token")), [this, gen, done = std::move(done)](NetReply r) {
        if (gen != m_gen) return;
        const QJsonObject o = QJsonDocument::fromJson(r.body).object();
        if (!r.error.isEmpty() || o.value(QStringLiteral("access_token")).toString().isEmpty()) { done(false); return; }
        applyTokens(o, true);
        done(true);
    }, lim);
}

void KickAuth::refreshAndVerify()
{
    if (!signedIn() || !configured()) return;
    if (!m_refresh.isEmpty() && m_expiresAt - QDateTime::currentSecsSinceEpoch() < 120) {
        refreshToken([this](bool ok) {
            if (ok) fetchMe(false);
            else { clearAll(); setState(QStringLiteral("error"), QStringLiteral("your sign-in has expired: sign in again")); emit saveRequested(); }
        });
        return;
    }
    fetchMe(true);
}

// ---- signing out -------------------------------------------------------------------------------------------------

void KickAuth::revoke(const QString &token, const char *hint)
{
    if (token.isEmpty()) return;
    QUrl url(kIdHost + QStringLiteral("/oauth/revoke"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("token"), token);
    q.addQueryItem(QStringLiteral("token_hint_type"), QString::fromLatin1(hint));
    url.setQuery(q);
    NetLimits lim; lim.post = true; lim.maxBody = 16 * 1024;
    m_net->get(kApp, url, [](NetReply) {}, lim);                                     // (best effort: the tokens are forgotten here either way)
}

void KickAuth::signOut()
{
    const QString access = m_access, refresh = m_refresh;
    ++m_gen;
    stopListening();
    m_access.clear(); m_refresh.clear(); m_name.clear(); m_expiresAt = 0; m_authorizeUrl.clear(); m_verifier.clear(); m_expectedState.clear();
    setState(QString(), QString());
    emit saveRequested();
    if (m_net->isOpen(kApp)) { revoke(access, "access_token"); revoke(refresh, "refresh_token"); }
}

void KickAuth::clearAll()
{
    ++m_gen;
    stopListening();
    m_access.clear(); m_refresh.clear(); m_name.clear(); m_expiresAt = 0; m_clientId.clear(); m_clientSecret.clear();
    m_authorizeUrl.clear(); m_verifier.clear(); m_expectedState.clear();
    m_state.clear(); m_message.clear();
    emit changed();
}

// ---- what the profile keeps ----------------------------------------------------------------------------------------

QJsonObject KickAuth::toJson() const
{
    if (m_clientId.isEmpty() && m_clientSecret.isEmpty() && m_access.isEmpty()) return {};
    QJsonObject o{{QStringLiteral("clientId"), m_clientId}, {QStringLiteral("clientSecret"), m_clientSecret}};
    if (!m_access.isEmpty()) {
        o.insert(QStringLiteral("access"), m_access); o.insert(QStringLiteral("refresh"), m_refresh);
        o.insert(QStringLiteral("expiresAt"), double(m_expiresAt)); o.insert(QStringLiteral("name"), m_name);
    }
    return o;
}

void KickAuth::fromJson(const QJsonObject &o)
{
    auto str = [&](const char *k, int max) { const QString v = o.value(QLatin1String(k)).toString(); return v.size() <= max ? v : QString(); };
    ++m_gen;
    stopListening();
    m_clientId = str("clientId", 200); m_clientSecret = str("clientSecret", 200);
    m_access = str("access", 4000); m_refresh = str("refresh", 4000);
    m_name = o.value(QStringLiteral("name")).toString().left(60);
    m_expiresAt = qint64(o.value(QStringLiteral("expiresAt")).toDouble());
    if (!configured()) { m_access.clear(); m_refresh.clear(); m_name.clear(); m_expiresAt = 0; }     // tokens without the app that made them are no use
    m_authorizeUrl.clear(); m_state.clear(); m_message.clear();
    emit changed();
}

} // namespace hop
