// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "net/AppNet.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>

namespace hop {

// Kick's official sign-in (OAuth 2.1 with PKCE, https://docs.kick.com/getting-started/generating-tokens-oauth2-flow), used only to show who is
// signed in (scope user:read). Each person registers their own Kick app and gives Hop-tunes its client ID and secret: nothing secret is shipped.
//  * the person's browser opens the real id.kick.com page; Kick sends the browser back to http://localhost:<port>/callback, where a one-shot
//    listener on this computer (127.0.0.1 only) takes the code, checks `state`, answers a small "go back to Hop-tunes" page and closes
//  * the code is exchanged at id.kick.com/oauth/token; the tokens are kept in the profile (see toJson / fromJson)
//  * every request goes through AppNet, which lets only three exact sign-in requests carry a body or a token (AppNet::authUrlAllowed)
class KickAuth : public QObject {
    Q_OBJECT
public:
    explicit KickAuth(AppNet *net, QObject *parent = nullptr);

    static constexpr int kDefaultPort = 38573;
    static constexpr const char *kScope = "user:read";

    // ---- what the person sets up ----
    void setCredentials(const QString &clientId, const QString &clientSecret);   // an empty secret keeps the saved one
    QString clientId() const { return m_clientId; }
    bool configured() const { return !m_clientId.isEmpty() && !m_clientSecret.isEmpty(); }
    void setPort(int port) { m_port = port; }                       // tests use 0 (any free port)
    int port() const { return m_server && m_server->isListening() ? m_server->serverPort() : m_port; }
    QString redirectUri() const;                                    // what the person registers in their Kick app

    // ---- state ----
    bool signedIn() const { return !m_access.isEmpty(); }
    QString accountName() const { return m_name; }
    QString state() const { return m_state; }                      // "" | "waiting" | "working" | "error"
    QString message() const { return m_message; }
    QUrl authorizeUrl() const { return m_authorizeUrl; }

    void signIn();                                                  // opens the listener and builds the address (the page opens it in the browser)
    void cancel();
    void signOut();                                                 // revokes the tokens at Kick, forgets everything (the app credentials stay)
    void refreshAndVerify();                                        // after the app opens: a fresh token if needed, and who it is

    QJsonObject toJson() const;                                     // client id and secret, tokens, name: the profile keeps them on this device
    void fromJson(const QJsonObject &o);
    void clearAll();                                                // a profile with no sign-in (switching profile, guests)

    // the pieces, public so the tests can hold them against known values (RFC 7636 appendix B)
    static QString pkceChallenge(const QString &verifier);
    static QString randomToken(int length, bool urlSafe);
    static QByteArray form(const QList<QPair<QString, QString>> &fields);

signals:
    void changed();                 // anything the screen shows changed
    void saveRequested();           // the tokens or the name changed: worth saving with the profile

private:
    void setState(const QString &state, const QString &message);
    void stopListening();
    void onConnection();
    void handleRequestLine(QTcpSocket *sock, const QByteArray &line);
    void exchangeCode(const QString &code);
    void applyTokens(const QJsonObject &o, bool fromRefresh);
    void fetchMe(bool mayRefresh);
    void refreshToken(std::function<void(bool)> done);
    void revoke(const QString &token, const char *hint);

    AppNet *m_net;
    QString m_clientId, m_clientSecret;
    int m_port = kDefaultPort;
    QTcpServer *m_server = nullptr;
    QTimer m_timeout;
    QString m_verifier, m_expectedState, m_redirect;     // (the redirect address of the sign-in in progress: the token request must repeat it exactly)
    QUrl m_authorizeUrl;
    QString m_state, m_message;
    QString m_access, m_refresh, m_name;
    qint64 m_expiresAt = 0;          // seconds since the epoch
    quint64 m_gen = 0;
};

} // namespace hop
