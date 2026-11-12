// SPDX-License-Identifier: GPL-3.0-or-later
#include "net/AppNet.h"

#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>

namespace hop {

namespace {

// A cookie jar that keeps nothing: nothing a site sets is remembered, and nothing is sent back.
class NoCookies : public QNetworkCookieJar {
public:
    using QNetworkCookieJar::QNetworkCookieJar;
    bool insertCookie(const QNetworkCookie &) override { return false; }
    bool updateCookie(const QNetworkCookie &) override { return false; }
    bool setCookiesFromUrl(const QList<QNetworkCookie> &, const QUrl &) override { return false; }
    QList<QNetworkCookie> cookiesForUrl(const QUrl &) const override { return {}; }
};

class QtTransport : public NetTransport {
public:
    QtTransport()
    {
        m_nam.setCookieJar(new NoCookies);
        m_nam.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);   // AppNet decides where redirects may go
    }
    void get(const QUrl &url, const QByteArray &ua, const QByteArray &accept, NetLimits limits, Done done) override
    {
        QNetworkRequest req(url);
        req.setRawHeader("User-Agent", ua);
        req.setRawHeader("Accept", accept);
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
        req.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
        req.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
        req.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
        req.setTransferTimeout(limits.timeoutMs);
        QNetworkReply *rep = m_nam.get(req);
        m_live.insert(rep);
        QObject::connect(rep, &QNetworkReply::readyRead, rep, [rep, limits] {
            if (rep->bytesAvailable() > limits.maxBody) rep->abort();           // too much: stop
        });
        QObject::connect(rep, &QNetworkReply::finished, rep, [this, rep, done, limits] {
            m_live.remove(rep);
            NetReply r;
            r.status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            r.location = rep->header(QNetworkRequest::LocationHeader).toUrl();
            if (rep->error() == QNetworkReply::OperationCanceledError) r.error = QStringLiteral("stopped");
            else if (rep->error() == QNetworkReply::TimeoutError) r.error = QStringLiteral("kick did not answer in time");
            else if (rep->error() != QNetworkReply::NoError && r.status == 0) r.error = QStringLiteral("could not connect: ") + rep->errorString();
            else r.body = rep->readAll().left(limits.maxBody);
            if (r.error.isEmpty() && (r.status < 200 || r.status >= 400)) r.error = QStringLiteral("the site answered %1").arg(r.status);
            rep->deleteLater();
            done(std::move(r));
        });
    }
    void abortAll() override
    {
        const auto live = m_live.values();
        m_live.clear();
        for (QNetworkReply *r : live) { r->disconnect(); r->abort(); r->deleteLater(); }
    }
private:
    QNetworkAccessManager m_nam;
    QSet<QNetworkReply *> m_live;
};

} // namespace

AppNet::AppNet(QObject *parent) : QObject(parent) {}
AppNet::~AppNet() = default;

QStringList AppNet::hostsFor(const QString &app)
{
    if (app == QLatin1String("kick"))
        return {QStringLiteral("kick.com"), QStringLiteral("web.kick.com"), QStringLiteral("images.kick.com"), QStringLiteral("stream.kick.com"), QStringLiteral("files.kick.com"), QStringLiteral("clips.kick.com")};
    return {};
}

QStringList AppNet::streamSuffixesFor(const QString &app)
{
    return app == QLatin1String("kick") ? QStringList{QStringLiteral("live-video.net")} : QStringList{};
}

bool AppNet::streamUrlAllowed(const QString &app, const QUrl &url, QString *why)
{
    auto no = [&](const QString &m) { if (why) *why = m; return false; };
    if (!url.isValid() || url.scheme() != QLatin1String("https")) return no(QStringLiteral("only secure (https) streams are allowed"));
    if (!url.userInfo().isEmpty() || (url.port() != -1 && url.port() != 443)) return no(QStringLiteral("that stream address is not allowed"));
    const QString host = url.host().toLower();
    for (const QString &suffix : streamSuffixesFor(app))
        if (host == suffix || host.endsWith(QLatin1Char('.') + suffix)) return true;
    return no(QStringLiteral("%1 is not a video site this app uses").arg(host));
}

bool AppNet::urlAllowed(const QString &app, const QUrl &url, QString *why)
{
    auto no = [&](const QString &m) { if (why) *why = m; return false; };
    if (!url.isValid() || url.scheme() != QLatin1String("https")) return no(QStringLiteral("only secure (https) addresses are allowed"));
    if (!url.userInfo().isEmpty()) return no(QStringLiteral("addresses with a user name are not allowed"));
    if (url.port() != -1 && url.port() != 443) return no(QStringLiteral("only the standard port is allowed"));
    if (!hostsFor(app).contains(url.host().toLower())) return no(QStringLiteral("%1 is not one of the sites this app uses").arg(url.host()));
    return true;
}

void AppNet::setAllowed(const QString &app, bool on)
{
    if (on) m_allowed.insert(app); else { m_allowed.remove(app); close(app); }
}

void AppNet::open(const QString &app)
{
    m_open.insert(app);
    m_contacted[app].clear();
    emit contactedChanged(app);
    emit opened(app);
}

void AppNet::close(const QString &app)
{
    ++m_epoch[app];                       // answers that are still on the way are dropped
    m_open.remove(app);
    m_contacted[app].clear();
    if (m_transport) m_transport->abortAll();
    emit contactedChanged(app);
    emit closed(app);
}

void AppNet::setTransport(std::unique_ptr<NetTransport> t) { m_transport = std::move(t); }

void AppNet::get(const QString &app, const QUrl &url, std::function<void(NetReply)> done, NetLimits limits)
{
    QString why;
    if (!m_allowed.contains(app)) { NetReply r; r.error = QStringLiteral("this app is not allowed to use the internet (settings > apps)"); done(r); return; }
    if (!m_open.contains(app)) { NetReply r; r.error = QStringLiteral("the app is not open"); done(r); return; }
    if (!urlAllowed(app, url, &why)) { NetReply r; r.error = why; done(r); return; }
    if (!m_transport) m_transport = std::make_unique<QtTransport>();
    fetch(app, url, 0, m_epoch.value(app), limits, std::move(done));
}

void AppNet::fetch(const QString &app, const QUrl &url, int hops, quint64 epoch, NetLimits limits, std::function<void(NetReply)> done)
{
    m_contacted[app].insert(url.host().toLower());
    emit contactedChanged(app);
    QPointer<AppNet> self(this);
    m_transport->get(url, userAgent(), "application/json, image/*;q=0.8, */*;q=0.5", limits,
        [self, app, url, hops, epoch, limits, done = std::move(done)](NetReply r) mutable {
            if (!self || self->m_epoch.value(app) != epoch) return;                 // the app was closed meanwhile: nothing is delivered
            if (r.status >= 300 && r.status < 400 && r.location.isValid()) {
                const QUrl next = url.resolved(r.location);
                QString why;
                if (hops >= kMaxRedirects) { NetReply e; e.error = QStringLiteral("too many redirects"); done(e); return; }
                if (!urlAllowed(app, next, &why)) { NetReply e; e.error = QStringLiteral("redirect refused: ") + why; done(e); return; }
                self->fetch(app, next, hops + 1, epoch, limits, std::move(done));
                return;
            }
            done(std::move(r));
        });
}

} // namespace hop
