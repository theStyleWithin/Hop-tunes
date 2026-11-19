// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <memory>

namespace hop {

// The only way an app inside Hop-tunes reaches the internet. Nothing else in Hop-tunes opens a network connection, and a test
// checks that (tests/test_net.cpp scans the sources). The gateway enforces, for every request, whatever the app asks for:
//  * the person has allowed that app to use the internet (settings > apps, or the first-open card)
//  * https only, the standard port, no user name in the address, and only the hosts that app has declared
//  * redirects are followed only to another declared host
//  * no cookies, no disk cache, no referrer, a generic user agent, a size limit, a timeout
//  * connections belong to the app being open: closing it drops everything still on the way
// The actual transfer is a NetTransport, so tests can answer from fixtures without a network.

struct NetReply {
    int status = 0;            // HTTP status, 0 when nothing came back
    QByteArray body;
    QString error;             // empty when the reply is usable (2xx); otherwise a short plain sentence
    QUrl location;             // for a redirect
};

// How much one answer may hold and how long it may take. The defaults suit pages and pictures; a download asks for more, for one video
// segment at a time (the person chose to download, and the storage limit still applies).
struct NetLimits {
    qint64 maxBody = 8 * 1024 * 1024;
    int timeoutMs = 15000;
    qint64 rangeStart = -1;      // with rangeLength: ask only for this slice of the file (HTTP Range)
    qint64 rangeLength = -1;
};

class NetTransport : public QObject {
public:
    using Done = std::function<void(NetReply)>;
    explicit NetTransport(QObject *parent = nullptr) : QObject(parent) {}
    virtual void get(const QUrl &url, const QByteArray &userAgent, const QByteArray &accept, NetLimits limits, Done done) = 0;
    // One kind of POST exists, for an app's public search interface (see AppNet::postJson): JSON in, JSON out, nothing else. A transport that cannot
    // do it (the test fakes that only answer GETs) says so.
    virtual void post(const QUrl &, const QByteArray &, const QByteArray &, NetLimits, Done done) { NetReply r; r.error = QStringLiteral("posting is not supported"); done(std::move(r)); }
    virtual void abortAll() = 0;
};

class AppNet : public QObject {
    Q_OBJECT
public:
    explicit AppNet(QObject *parent = nullptr);
    ~AppNet() override;

    static constexpr qint64 kMaxBody = 8 * 1024 * 1024;
    static constexpr int kTimeoutMs = 15000;
    static constexpr int kMaxRedirects = 3;

    // the sites an app may contact, by app id ("kick"); empty for an unknown app
    static QStringList hostsFor(const QString &app);
    // the rules above for one address (without the permission): https, port, user info, host
    static bool urlAllowed(const QString &app, const QUrl &url, QString *why = nullptr);
    // The video itself is fetched by the video engine (libmpv or GStreamer), not by this gateway: it may only be given an address of
    // the video host the app declared (https, standard port, no user name). Inside the engine the host cannot be enforced any further.
    static QStringList streamSuffixesFor(const QString &app);
    static bool streamUrlAllowed(const QString &app, const QUrl &url, QString *why = nullptr);
    static QByteArray userAgent() { return "Mozilla/5.0"; }

    // permission: set from the profile (and per session for guests)
    void setAllowed(const QString &app, bool on);
    bool allowed(const QString &app) const { return m_allowed.contains(app); }

    // the app is open / closed: closing drops everything in flight and forgets who was contacted
    void open(const QString &app);
    void close(const QString &app);
    bool isOpen(const QString &app) const { return m_open.contains(app); }
    QStringList contacted(const QString &app) const { QStringList l = m_contacted.value(app).values(); l.sort(); return l; }

    // The one POST: a few public search interfaces only answer to a JSON request (Odysee's claim_search). It is allowed to exactly the one address an
    // app declares here (https, that host, that path and query, nothing else), for a small JSON object that names the one read-only method
    // that address is for. No cookies, no token, no redirects, the same size and time limits. The person's permission and the app being open apply as for a GET.
    static QUrl postEndpointFor(const QString &app);
    static QString postMethodFor(const QString &app);
    static constexpr int kMaxPostBody = 4096;
    static bool postAllowed(const QString &app, const QUrl &url, const QByteArray &body, QString *why = nullptr);
    void postJson(const QString &app, const QUrl &url, const QByteArray &body, std::function<void(NetReply)> done, NetLimits limits = {});

    // GET one address for an app; `done` is called once, on the main thread, and never after close()
    void get(const QString &app, const QUrl &url, std::function<void(NetReply)> done, NetLimits limits = {});

    // tests: answer from fixtures instead of the network
    void setTransport(std::unique_ptr<NetTransport> t);

signals:
    void contactedChanged(const QString &app);
    void opened(const QString &app);
    void closed(const QString &app);

private:
    void fetch(const QString &app, const QUrl &url, int hops, quint64 epoch, NetLimits limits, std::function<void(NetReply)> done);

    std::unique_ptr<NetTransport> m_transport;
    QSet<QString> m_allowed, m_open;
    QHash<QString, QSet<QString>> m_contacted;
    QHash<QString, quint64> m_epoch;
};

} // namespace hop
