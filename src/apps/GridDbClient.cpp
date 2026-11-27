// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps/GridDbClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>
#include <QUrl>

namespace hop {

namespace {
const char *const kApiBase = "https://www.steamgriddb.com/api/v2";

bool urlOk(const QString &u) { return AppNet::urlAllowed(QStringLiteral("steamgriddb"), QUrl(u)); }
}

GridDbClient::GridDbClient(AppNet *net, QObject *parent) : QObject(parent), m_net(net) {}

bool GridDbClient::validKey(const QString &key)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_-]{8,128}$"));
    return re.match(key).hasMatch();
}

void GridDbClient::setKey(const QString &key)
{
    const QString k = validKey(key.trimmed()) ? key.trimmed() : QString();
    if (k == m_key) return;
    m_key = k;
    emit changed();
}

void GridDbClient::setMessage(const QString &m)
{
    if (m == m_message) return;
    m_message = m;
    emit changed();
}

void GridDbClient::open()
{
    if (m_open) return;
    if (m_key.isEmpty()) { setMessage(QStringLiteral("paste your SteamGridDB key first")); return; }
    if (!m_net->allowed(QLatin1String(kApp))) { setMessage(QStringLiteral("SteamGridDB is not allowed to use the internet yet")); return; }
    m_open = true;
    m_net->open(QLatin1String(kApp));
    setMessage(QString());
    emit changed();
}

void GridDbClient::close()
{
    ++m_epoch;
    m_busy = 0;
    const bool was = m_open;
    m_open = false;
    m_games.clear();
    m_grids.clear();
    m_message.clear();
    if (was) m_net->close(QLatin1String(kApp));
    emit gamesChanged();
    emit gridsChanged();
    emit changed();
}

QString GridDbClient::searchUrl(const QString &term)
{
    const QString t = term.simplified().left(100);
    return QString::fromLatin1(kApiBase) + QStringLiteral("/search/autocomplete/") + QString::fromLatin1(QUrl::toPercentEncoding(t));
}

static QString gridQuery()
{
    // vertical covers (600x900 is the Steam library size; the others are its close relatives), still pictures, nothing adult, no flashing ones
    return QStringLiteral("?dimensions=600x900,342x482,660x930&types=static&nsfw=false&humor=false&epilepsy=false");
}

QString GridDbClient::gridsUrl(int gameId) { return QString::fromLatin1(kApiBase) + QStringLiteral("/grids/game/") + QString::number(qMax(0, gameId)) + gridQuery(); }

QString GridDbClient::gridsSteamUrl(const QString &appId)
{
    static const QRegularExpression digits(QStringLiteral("^[0-9]{1,12}$"));
    return QString::fromLatin1(kApiBase) + QStringLiteral("/grids/steam/") + (digits.match(appId).hasMatch() ? appId : QStringLiteral("0")) + gridQuery();
}

QString GridDbClient::explain(int status, const QString &error)
{
    if (status == 401 || status == 403) return QStringLiteral("SteamGridDB did not accept that key: check it, or make a new one in your SteamGridDB preferences");
    if (status == 404) return QStringLiteral("SteamGridDB found nothing for that");
    if (status == 429) return QStringLiteral("SteamGridDB asked to slow down: try again in a minute");
    if (status >= 500) return QStringLiteral("SteamGridDB is having trouble right now: try again later");
    return error.isEmpty() ? QStringLiteral("SteamGridDB could not be reached") : error;
}

QVariantList GridDbClient::parseGames(const QByteArray &json)
{
    QVariantList out;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject() || !doc.object().value(QLatin1String("success")).toBool(false)) return out;
    for (const QJsonValue &v : doc.object().value(QLatin1String("data")).toArray()) {
        if (out.size() >= kMaxGames) break;
        const QJsonObject o = v.toObject();
        const int id = o.value(QLatin1String("id")).toInt(-1);
        const QString name = o.value(QLatin1String("name")).toString().simplified().left(200);
        if (id <= 0 || name.isEmpty()) continue;
        out.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("verified"), o.value(QLatin1String("verified")).toBool()}});
    }
    return out;
}

QVariantList GridDbClient::parseGrids(const QByteArray &json)
{
    QVariantList out;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject() || !doc.object().value(QLatin1String("success")).toBool(false)) return out;
    for (const QJsonValue &v : doc.object().value(QLatin1String("data")).toArray()) {
        if (out.size() >= kMaxGrids) break;
        const QJsonObject o = v.toObject();
        const QString url = o.value(QLatin1String("url")).toString();
        QString thumb = o.value(QLatin1String("thumb")).toString();
        if (!urlOk(url)) continue;                       // only SteamGridDB's own picture servers, over https
        if (!urlOk(thumb)) thumb = url;
        out.append(QVariantMap{{QStringLiteral("id"), o.value(QLatin1String("id")).toInt()}, {QStringLiteral("url"), url}, {QStringLiteral("thumb"), thumb},
                               {QStringLiteral("width"), o.value(QLatin1String("width")).toInt()}, {QStringLiteral("height"), o.value(QLatin1String("height")).toInt()},
                               {QStringLiteral("score"), o.value(QLatin1String("score")).toInt()},
                               {QStringLiteral("author"), o.value(QLatin1String("author")).toObject().value(QLatin1String("name")).toString().simplified().left(80)}});
    }
    return out;
}

void GridDbClient::request(const QString &url, std::function<void(NetReply)> done)
{
    if (!m_open) { setMessage(m_key.isEmpty() ? QStringLiteral("paste your SteamGridDB key first") : QStringLiteral("the cover search is not open")); return; }
    ++m_busy;
    emit changed();
    NetLimits lim;
    lim.authorization = "Bearer " + m_key.toUtf8();
    lim.maxBody = 2 * 1024 * 1024;
    const quint64 epoch = m_epoch;
    QPointer<GridDbClient> self(this);
    m_net->get(QLatin1String(kApp), QUrl(url), [self, epoch, done = std::move(done)](NetReply r) {
        if (!self || self->m_epoch != epoch) return;
        if (self->m_busy > 0) --self->m_busy;
        done(std::move(r));
        emit self->changed();
    }, lim);
}

void GridDbClient::search(const QString &term)
{
    if (term.simplified().isEmpty()) { setMessage(QStringLiteral("type the game's name")); return; }
    m_grids.clear();
    emit gridsChanged();
    request(searchUrl(term), [this](NetReply r) {
        if (!r.error.isEmpty()) { m_games.clear(); emit gamesChanged(); setMessage(explain(r.status, r.error)); return; }
        m_games = parseGames(r.body);
        emit gamesChanged();
        setMessage(m_games.isEmpty() ? QStringLiteral("SteamGridDB found no game with that name") : QString());
    });
}


void GridDbClient::gridsForGame(int gameId)
{
    if (gameId <= 0) return;
    request(gridsUrl(gameId), [this](NetReply r) {
        if (!r.error.isEmpty()) { m_grids.clear(); emit gridsChanged(); setMessage(explain(r.status, r.error)); return; }
        m_grids = parseGrids(r.body);
        emit gridsChanged();
        setMessage(m_grids.isEmpty() ? QStringLiteral("no vertical covers for that game on SteamGridDB yet") : QString());
    });
}

void GridDbClient::gridsForSteam(const QString &appId)
{
    m_games.clear();
    emit gamesChanged();
    request(gridsSteamUrl(appId), [this](NetReply r) {
        if (!r.error.isEmpty()) { m_grids.clear(); emit gridsChanged(); setMessage(explain(r.status, r.error)); return; }
        m_grids = parseGrids(r.body);
        emit gridsChanged();
        setMessage(m_grids.isEmpty() ? QStringLiteral("no vertical covers for that game on SteamGridDB yet") : QString());
    });
}

void GridDbClient::download(const QString &url, std::function<void(QByteArray, QString)> done)
{
    QString why;
    if (!m_open) { done({}, QStringLiteral("the cover search is not open")); return; }
    if (!AppNet::urlAllowed(QLatin1String(kApp), QUrl(url), &why)) { done({}, why); return; }
    ++m_busy;
    emit changed();
    NetLimits lim;
    lim.maxBody = kMaxImageBytes;
    lim.timeoutMs = 30000;
    const quint64 epoch = m_epoch;
    QPointer<GridDbClient> self(this);
    m_net->get(QLatin1String(kApp), QUrl(url), [self, epoch, done = std::move(done)](NetReply r) {
        if (!self || self->m_epoch != epoch) return;
        if (self->m_busy > 0) --self->m_busy;
        emit self->changed();
        if (!r.error.isEmpty()) { done({}, explain(r.status, r.error)); return; }
        done(r.body, QString());
    }, lim);
}

} // namespace hop
