// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps/OdyseeClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRegularExpression>
#include <QUrl>

namespace hop {

namespace {
bool containsNoCase(const QStringList &l, const QString &s)
{
    for (const QString &x : l) if (x.compare(s, Qt::CaseInsensitive) == 0) return true;
    return false;
}
bool hex(const QString &s, int min, int max)
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-f]+$"));
    return s.size() >= min && s.size() <= max && re.match(s).hasMatch();
}
}

QString OdyseeClient::clean(const QString &s, int max) { return s.simplified().left(max); }

bool OdyseeClient::validChannel(const QString &c)
{
    static const QRegularExpression re(QStringLiteral("^@[^\\s/:$#\"\\\\<>?&=%]{1,60}$"));
    return re.match(c).hasMatch();
}

QString OdyseeClient::playUrl(const QString &name, const QString &claimId, const QString &sdHash, const QString &mediaType)
{
    QString ext;
    if (mediaType == QLatin1String("video/mp4")) ext = QStringLiteral("mp4");
    else if (mediaType == QLatin1String("video/webm")) ext = QStringLiteral("webm");
    else if (mediaType == QLatin1String("video/quicktime")) ext = QStringLiteral("mov");
    if (ext.isEmpty() || !hex(claimId, 40, 40) || !hex(sdHash, 6, 96) || name.isEmpty() || name.size() > 200) return {};
    return QStringLiteral("https://player.odycdn.com/api/v3/streams/free/") + QString::fromLatin1(QUrl::toPercentEncoding(name)) + QLatin1Char('/') + claimId + QLatin1Char('/') + sdHash.left(6) + QLatin1Char('.') + ext;
}

QString OdyseeClient::pageUrl(const QString &canonical)
{
    QString c = canonical;
    if (!c.startsWith(QLatin1String("lbry://"))) return {};
    c = c.mid(7);
    c.replace(QLatin1Char('#'), QLatin1Char(':'));
    return QStringLiteral("https://odysee.com/") + QString::fromLatin1(QUrl::toPercentEncoding(c, "/@:!"));
}

QByteArray OdyseeClient::request(const QString &text, const QString &channel, const QStringList &channelIds, int page, bool mature)
{
    QJsonObject p{{QStringLiteral("page"), qBound(1, page, 50)}, {QStringLiteral("page_size"), kPage}, {QStringLiteral("no_totals"), true},
                  {QStringLiteral("claim_type"), QJsonArray{QStringLiteral("stream")}}, {QStringLiteral("stream_types"), QJsonArray{QStringLiteral("video")}}};
    const QString t = clean(text, 100);
    const bool following = !channelIds.isEmpty();
    if (!mature) p.insert(QStringLiteral("not_tags"), QJsonArray{QStringLiteral("mature")});
    if (!t.isEmpty()) p.insert(QStringLiteral("text"), t);
    if (!channel.isEmpty() && validChannel(channel)) p.insert(QStringLiteral("channel"), channel);
    if (following) {
        QJsonArray ids;
        for (const QString &i : channelIds) if (hex(i, 40, 40) && ids.size() < kMaxFollows) ids.append(i);
        p.insert(QStringLiteral("channel_ids"), ids);
    }
    if (t.isEmpty() && channel.isEmpty() && !following) p.insert(QStringLiteral("order_by"), QJsonArray{QStringLiteral("trending_group"), QStringLiteral("trending_mixed")});
    else p.insert(QStringLiteral("order_by"), QJsonArray{QStringLiteral("release_time")});
    return QJsonDocument(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("method"), QStringLiteral("claim_search")},
                                     {QStringLiteral("params"), p}, {QStringLiteral("id"), 1}}).toJson(QJsonDocument::Compact);
}

QVector<OdyseeClient::Video> OdyseeClient::parse(const QByteArray &json)
{
    QVector<Video> out;
    const QJsonArray items = QJsonDocument::fromJson(json).object().value(QStringLiteral("result")).toObject().value(QStringLiteral("items")).toArray();
    for (const QJsonValue &iv : items) {
        const QJsonObject i = iv.toObject();
        const QString id = i.value(QStringLiteral("claim_id")).toString();
        const QJsonObject v = i.value(QStringLiteral("value")).toObject();
        if (!hex(id, 40, 40) || i.value(QStringLiteral("value_type")).toString() == QLatin1String("channel") || v.isEmpty()) continue;
        if (v.value(QStringLiteral("stream_type")).toString() != QLatin1String("video")) continue;
        Video x;
        x.id = id; x.name = clean(i.value(QStringLiteral("name")).toString());
        x.title = clean(v.value(QStringLiteral("title")).toString().isEmpty() ? x.name : v.value(QStringLiteral("title")).toString());
        const QJsonObject ch = i.value(QStringLiteral("signing_channel")).toObject();
        x.channel = clean(ch.value(QStringLiteral("name")).toString(), 80);
        x.channelId = ch.value(QStringLiteral("claim_id")).toString();
        if (!hex(x.channelId, 40, 40)) x.channelId.clear();
        const QString th = v.value(QStringLiteral("thumbnail")).toObject().value(QStringLiteral("url")).toString();
        if (AppNet::urlAllowed(QLatin1String(kApp), QUrl(th))) x.thumb = th;
        const QJsonObject src = v.value(QStringLiteral("source")).toObject();
        x.url = playUrl(x.name, id, src.value(QStringLiteral("sd_hash")).toString(), src.value(QStringLiteral("media_type")).toString());
        x.page = pageUrl(i.value(QStringLiteral("canonical_url")).toString());
        x.duration = qBound(0, v.value(QStringLiteral("video")).toObject().value(QStringLiteral("duration")).toInt(), 24 * 3600 * 7);
        x.released = v.value(QStringLiteral("release_time")).toString().toLongLong();
        for (const QJsonValue &t : v.value(QStringLiteral("tags")).toArray()) if (t.toString().compare(QStringLiteral("mature"), Qt::CaseInsensitive) == 0) x.mature = true;
        if (x.page.isEmpty()) continue;
        out.append(x);
    }
    return out;
}

OdyseeClient::OdyseeClient(AppNet *net, QObject *parent) : QObject(parent), m_net(net) {}

void OdyseeClient::setMessage(const QString &m)
{
    if (m == m_message) return;
    m_message = m;
    emit changed();
}

void OdyseeClient::open()
{
    if (m_open) return;
    m_open = true;
    m_net->open(kApp);
    emit openChanged();
    if (!m_net->allowed(kApp)) { setMessage(QStringLiteral("this app is not allowed to use the internet (settings > apps)")); return; }
    search(QString());
}

void OdyseeClient::close()
{
    if (!m_open) return;
    m_open = false;
    ++m_gen;
    m_net->close(kApp);
    m_loading = false; m_more = false; m_all.clear(); m_visible.clear(); m_hidden = 0; m_query.clear(); m_message.clear(); m_channel.clear(); m_following = false;
    emit openChanged(); emit changed(); emit videosChanged();
}

bool OdyseeClient::streamAllowed(const QString &url) const { return AppNet::streamUrlAllowed(QLatin1String(kApp), QUrl(url)); }

QStringList OdyseeClient::followIds() const
{
    QStringList l;
    for (const QVariant &v : m_follows) { const QString id = v.toMap().value(QStringLiteral("id")).toString(); if (!id.isEmpty()) l << id; }
    return l;
}

void OdyseeClient::search(const QString &text)
{
    if (!m_open) return;
    const QString t = clean(text, 100);
    m_following = false; m_channel.clear();
    if (t.startsWith(QLatin1Char('@'))) {
        if (!validChannel(t)) { setMessage(QStringLiteral("that is not a channel name")); return; }
        m_channel = t;
        if (isBlockedChannel(t)) { m_query = t; m_all.clear(); m_more = false; rebuild(); setMessage(QStringLiteral("you blocked this channel (unblock it in your lists to see its videos)")); return; }
    } else {
        for (const QString &w : t.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts))
            if (containsNoCase(m_blockedWords, w)) { setMessage(QStringLiteral("\"%1\" is one of your blocked words, so it is not searched").arg(w)); return; }
    }
    m_query = t;
    m_page = 1; m_all.clear(); m_more = false;
    rebuild();
    run(false);
}

void OdyseeClient::showFollowing()
{
    if (!m_open) return;
    m_query = QStringLiteral("following"); m_following = true; m_channel.clear(); m_page = 1; m_all.clear(); m_more = false;
    rebuild();
    if (m_follows.isEmpty()) { setMessage(QStringLiteral("you follow no channels yet: open a video and follow its channel")); return; }
    run(false);
}

void OdyseeClient::loadMore()
{
    if (!m_open || m_loading || !m_more) return;
    ++m_page;
    run(true);
}

void OdyseeClient::run(bool append)
{
    const quint64 gen = ++m_gen;
    m_loading = true;
    setMessage(QString());
    emit changed();
    const QString text = m_following || m_query.startsWith(QLatin1Char('@')) ? QString() : m_query;
    const QByteArray body = request(text, m_channel, m_following ? followIds() : QStringList(), m_page, m_showMature);
    QPointer<OdyseeClient> self(this);
    NetLimits lim;
    lim.maxBody = 4 * 1024 * 1024;
    m_net->postJson(kApp, AppNet::postEndpointFor(QLatin1String(kApp)), body, [self, gen, append](NetReply r) {
        if (!self || self->m_gen != gen || !self->m_open) return;
        self->m_loading = false;
        if (!r.error.isEmpty()) { self->setMessage(QStringLiteral("could not load: ") + r.error); if (append) --self->m_page; emit self->changed(); emit self->videosChanged(); return; }
        const QVector<Video> got = parse(r.body);
        const int rawCount = QJsonDocument::fromJson(r.body).object().value(QStringLiteral("result")).toObject().value(QStringLiteral("items")).toArray().size();
        for (const Video &v : got) {
            bool dup = false;
            for (const Video &o : std::as_const(self->m_all)) if (o.id == v.id) { dup = true; break; }
            if (!dup && self->m_all.size() < kMaxKept) self->m_all.append(v);
        }
        self->m_more = rawCount >= kPage && self->m_all.size() < kMaxKept;
        self->rebuild();
        if (self->m_all.isEmpty()) self->setMessage(QStringLiteral("nothing found"));
        emit self->changed();
    }, lim);
}

bool OdyseeClient::hides(const Video &v, QString *why) const
{
    auto no = [&](const QString &m) { if (why) *why = m; return true; };
    if (v.mature && !m_showMature) return no(QStringLiteral("18+"));
    if (containsNoCase(m_blockedChannels, v.channel)) return no(QStringLiteral("blocked channel"));
    for (const QString &word : m_blockedWords) {
        const QRegularExpression re(QStringLiteral("(?<![\\p{L}\\p{N}])") + QRegularExpression::escape(word) + QStringLiteral("(?![\\p{L}\\p{N}])"), QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
        if (re.match(v.title).hasMatch()) return no(QStringLiteral("blocked word"));
    }
    return false;
}

void OdyseeClient::rebuild()
{
    m_visible.clear(); m_hidden = 0;
    for (const Video &v : std::as_const(m_all)) {
        if (hides(v)) { ++m_hidden; continue; }
        m_visible.append(QVariantMap{{QStringLiteral("id"), v.id}, {QStringLiteral("title"), v.title}, {QStringLiteral("channel"), v.channel}, {QStringLiteral("channelId"), v.channelId},
                                     {QStringLiteral("thumb"), v.thumb}, {QStringLiteral("url"), v.url}, {QStringLiteral("page"), v.page}, {QStringLiteral("duration"), v.duration},
                                     {QStringLiteral("released"), v.released}, {QStringLiteral("mature"), v.mature}});
    }
    emit videosChanged();
}

void OdyseeClient::setShowMature(bool on)
{
    if (on == m_showMature) return;
    m_showMature = on;
    emit filtersChanged(); emit listsEdited();
    if (m_open && m_net->allowed(kApp)) { m_page = 1; m_all.clear(); m_more = false; rebuild(); run(false); } else rebuild();   // (the search itself leaves 18+ out, so it asks again)
}

void OdyseeClient::blockChannel(const QString &name)
{
    const QString n = clean(name, 80);
    if (!validChannel(n) || containsNoCase(m_blockedChannels, n) || m_blockedChannels.size() >= kMaxBlocked) return;
    m_blockedChannels.append(n);
    rebuild();
    emit filtersChanged(); emit listsEdited();
}

void OdyseeClient::unblockChannel(const QString &name)
{
    for (int i = 0; i < m_blockedChannels.size(); ++i)
        if (m_blockedChannels.at(i).compare(name.trimmed(), Qt::CaseInsensitive) == 0) { m_blockedChannels.removeAt(i); rebuild(); emit filtersChanged(); emit listsEdited(); return; }
}

bool OdyseeClient::isBlockedChannel(const QString &name) const { return containsNoCase(m_blockedChannels, name.trimmed()); }

bool OdyseeClient::blockWord(const QString &word)
{
    const QString w = clean(word, 40).toLower();
    if (w.isEmpty() || containsNoCase(m_blockedWords, w) || m_blockedWords.size() >= kMaxBlocked) return false;
    m_blockedWords.append(w);
    rebuild();
    emit filtersChanged(); emit listsEdited();
    return true;
}

void OdyseeClient::unblockWord(const QString &word)
{
    for (int i = 0; i < m_blockedWords.size(); ++i)
        if (m_blockedWords.at(i).compare(word.trimmed(), Qt::CaseInsensitive) == 0) { m_blockedWords.removeAt(i); rebuild(); emit filtersChanged(); emit listsEdited(); return; }
}

void OdyseeClient::toggleFollow(const QString &name, const QString &channelId)
{
    const QString n = clean(name, 80);
    if (!validChannel(n)) return;
    for (int i = 0; i < m_follows.size(); ++i)
        if (m_follows.at(i).toMap().value(QStringLiteral("name")).toString().compare(n, Qt::CaseInsensitive) == 0) { m_follows.removeAt(i); emit listsChanged(); emit listsEdited(); return; }
    if (m_follows.size() >= kMaxFollows || !hex(channelId, 40, 40)) return;
    m_follows.append(QVariantMap{{QStringLiteral("name"), n}, {QStringLiteral("id"), channelId}});
    emit listsChanged(); emit listsEdited();
}

bool OdyseeClient::isFollowing(const QString &name) const
{
    for (const QVariant &v : m_follows) if (v.toMap().value(QStringLiteral("name")).toString().compare(name.trimmed(), Qt::CaseInsensitive) == 0) return true;
    return false;
}

// what a favourite or playlist entry keeps: a title and the addresses, never the video; every address is checked again
QVariantMap OdyseeClient::keep(const QVariantMap &v)
{
    const QString id = v.value(QStringLiteral("id")).toString();
    const QString page = v.value(QStringLiteral("page")).toString();
    if (!hex(id, 40, 40) || !page.startsWith(QLatin1String("https://odysee.com/"))) return {};
    const QString thumb = v.value(QStringLiteral("thumb")).toString(), url = v.value(QStringLiteral("url")).toString();
    return QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("title"), clean(v.value(QStringLiteral("title")).toString())}, {QStringLiteral("channel"), clean(v.value(QStringLiteral("channel")).toString(), 80)},
                       {QStringLiteral("channelId"), hex(v.value(QStringLiteral("channelId")).toString(), 40, 40) ? v.value(QStringLiteral("channelId")).toString() : QString()},
                       {QStringLiteral("thumb"), AppNet::urlAllowed(QLatin1String(kApp), QUrl(thumb)) ? thumb : QString()},
                       {QStringLiteral("url"), AppNet::streamUrlAllowed(QLatin1String(kApp), QUrl(url)) ? url : QString()}, {QStringLiteral("page"), page},
                       {QStringLiteral("duration"), qBound(0, v.value(QStringLiteral("duration")).toInt(), 24 * 3600 * 7)}};
}

void OdyseeClient::toggleFavourite(const QVariantMap &video)
{
    const QString id = video.value(QStringLiteral("id")).toString();
    for (int i = 0; i < m_favs.size(); ++i)
        if (m_favs.at(i).toMap().value(QStringLiteral("id")).toString() == id) { m_favs.removeAt(i); emit listsChanged(); emit listsEdited(); return; }
    const QVariantMap k = keep(video);
    if (k.isEmpty() || m_favs.size() >= kMaxFavourites) return;
    m_favs.append(k);
    emit listsChanged(); emit listsEdited();
}

bool OdyseeClient::isFavourite(const QString &id) const
{
    for (const QVariant &v : m_favs) if (v.toMap().value(QStringLiteral("id")).toString() == id) return true;
    return false;
}

QVariantList OdyseeClient::playlists() const
{
    QVariantList l;
    for (const auto &p : m_playlists) l.append(QVariantMap{{QStringLiteral("name"), p.first}, {QStringLiteral("videos"), p.second}});
    return l;
}

bool OdyseeClient::createPlaylist(const QString &name)
{
    const QString n = clean(name, 60);
    if (n.isEmpty() || m_playlists.size() >= kMaxPlaylists) return false;
    for (const auto &p : m_playlists) if (p.first.compare(n, Qt::CaseInsensitive) == 0) return false;
    m_playlists.append({n, {}});
    emit listsChanged(); emit listsEdited();
    return true;
}

void OdyseeClient::deletePlaylist(const QString &name)
{
    for (int i = 0; i < m_playlists.size(); ++i)
        if (m_playlists.at(i).first.compare(name.trimmed(), Qt::CaseInsensitive) == 0) { m_playlists.removeAt(i); emit listsChanged(); emit listsEdited(); return; }
}

void OdyseeClient::togglePlaylistVideo(const QString &playlist, const QVariantMap &video)
{
    for (auto &p : m_playlists) {
        if (p.first.compare(playlist.trimmed(), Qt::CaseInsensitive) != 0) continue;
        const QString id = video.value(QStringLiteral("id")).toString();
        for (int i = 0; i < p.second.size(); ++i)
            if (p.second.at(i).toMap().value(QStringLiteral("id")).toString() == id) { p.second.removeAt(i); emit listsChanged(); emit listsEdited(); return; }
        const QVariantMap k = keep(video);
        if (k.isEmpty() || p.second.size() >= kMaxPlaylistVideos) return;
        p.second.append(k);
        emit listsChanged(); emit listsEdited();
        return;
    }
}

bool OdyseeClient::inPlaylist(const QString &playlist, const QString &id) const
{
    for (const auto &p : m_playlists)
        if (p.first.compare(playlist.trimmed(), Qt::CaseInsensitive) == 0)
            for (const QVariant &v : p.second) if (v.toMap().value(QStringLiteral("id")).toString() == id) return true;
    return false;
}

QJsonObject OdyseeClient::listsJson() const
{
    auto arr = [](const QVariantList &l) { QJsonArray a; for (const QVariant &v : l) a.append(QJsonObject::fromVariantMap(v.toMap())); return a; };
    QJsonArray pls;
    for (const auto &p : m_playlists) pls.append(QJsonObject{{QStringLiteral("name"), p.first}, {QStringLiteral("videos"), arr(p.second)}});
    return QJsonObject{{QStringLiteral("showMature"), m_showMature}, {QStringLiteral("blockedChannels"), QJsonArray::fromStringList(m_blockedChannels)},
                       {QStringLiteral("blockedWords"), QJsonArray::fromStringList(m_blockedWords)}, {QStringLiteral("follows"), arr(m_follows)},
                       {QStringLiteral("favourites"), arr(m_favs)}, {QStringLiteral("playlists"), pls}};
}

void OdyseeClient::setListsJson(const QJsonObject &o)
{
    m_showMature = o.value(QStringLiteral("showMature")).toBool(false);
    auto strings = [](const QJsonValue &v, bool channel) {
        QStringList l;
        for (const QJsonValue &x : v.toArray()) { const QString s = clean(x.toString(), channel ? 80 : 40); if (!s.isEmpty() && (!channel || validChannel(s)) && l.size() < kMaxBlocked && !containsNoCase(l, s)) l << s; }
        return l;
    };
    m_blockedChannels = strings(o.value(QStringLiteral("blockedChannels")), true);
    m_blockedWords = strings(o.value(QStringLiteral("blockedWords")), false);
    m_follows.clear();
    for (const QJsonValue &v : o.value(QStringLiteral("follows")).toArray()) {
        const QJsonObject f = v.toObject();
        const QString n = clean(f.value(QStringLiteral("name")).toString(), 80), id = f.value(QStringLiteral("id")).toString();
        if (validChannel(n) && hex(id, 40, 40) && m_follows.size() < kMaxFollows) m_follows.append(QVariantMap{{QStringLiteral("name"), n}, {QStringLiteral("id"), id}});
    }
    m_favs.clear();
    for (const QJsonValue &v : o.value(QStringLiteral("favourites")).toArray()) { const QVariantMap k = keep(v.toObject().toVariantMap()); if (!k.isEmpty() && m_favs.size() < kMaxFavourites) m_favs.append(k); }
    m_playlists.clear();
    for (const QJsonValue &pv : o.value(QStringLiteral("playlists")).toArray()) {
        const QJsonObject po = pv.toObject();
        const QString n = clean(po.value(QStringLiteral("name")).toString(), 60);
        if (n.isEmpty() || m_playlists.size() >= kMaxPlaylists) continue;
        QVariantList vids;
        for (const QJsonValue &v : po.value(QStringLiteral("videos")).toArray()) { const QVariantMap k = keep(v.toObject().toVariantMap()); if (!k.isEmpty() && vids.size() < kMaxPlaylistVideos) vids.append(k); }
        m_playlists.append({n, vids});
    }
    rebuild();
    emit filtersChanged(); emit listsChanged();
}

} // namespace hop
