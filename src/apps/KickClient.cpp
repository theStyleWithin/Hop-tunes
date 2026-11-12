// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps/KickClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>

#include <functional>
#include <memory>

#include <algorithm>

namespace hop {

namespace {
QString text(const QJsonValue &v) { return v.isString() ? v.toString() : QString(); }
int whole(const QJsonValue &v) { return v.isDouble() ? v.toInt() : v.toString().toInt(); }
QJsonObject object(const QByteArray &json, bool *ok)
{
    QJsonParseError e;
    const QJsonDocument d = QJsonDocument::fromJson(json, &e);
    *ok = e.error == QJsonParseError::NoError && d.isObject();
    return d.object();
}
}

KickClient::KickClient(AppNet *net, QObject *parent) : QObject(parent), m_net(net)
{
    connect(m_net, &AppNet::contactedChanged, this, [this](const QString &app) { if (app == QLatin1String(kApp)) emit contactedChanged(); });
}

// ---- reading the replies ----------------------------------------------------------------------------

QString KickClient::normalize(const QString &in)
{
    QString s = in.trimmed();
    static const QRegularExpression link(QStringLiteral("^(?:https?://)?(?:www\\.)?kick\\.com/([A-Za-z0-9_-]{1,40})(?:[/?#].*)?$"));
    const auto m = link.match(s);
    if (m.hasMatch()) s = m.captured(1);
    if (s.startsWith(QLatin1Char('@'))) s.remove(0, 1);
    static const QRegularExpression name(QStringLiteral("^[A-Za-z0-9_-]{1,40}$"));
    return name.match(s).hasMatch() ? s.toLower() : QString();
}

QString KickClient::viewersText(int n)
{
    if (n >= 1000000) return QStringLiteral("%1m").arg(n / 1000000.0, 0, 'f', n >= 10000000 ? 0 : 1);
    if (n >= 10000) return QStringLiteral("%1k").arg(n / 1000.0, 0, 'f', 1);
    if (n >= 1000) return QStringLiteral("%1k").arg(n / 1000.0, 0, 'f', 1);
    return QString::number(qMax(0, n));
}

bool KickClient::parseLivestreams(const QByteArray &json, QVariantList *out, QString *nextCursor)
{
    bool ok = false;
    const QJsonObject root = object(json, &ok);
    if (!ok) return false;
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QJsonArray list = data.value(QStringLiteral("livestreams")).toArray();
    if (!data.contains(QStringLiteral("livestreams"))) return false;
    for (const QJsonValue &v : list) {
        const QJsonObject o = v.toObject();
        const QJsonObject ch = o.value(QStringLiteral("channel")).toObject();
        const QString slug = normalize(text(ch.value(QStringLiteral("slug"))));
        if (slug.isEmpty()) continue;
        const QJsonObject cat = o.value(QStringLiteral("category")).toObject();
        const int viewers = whole(o.value(QStringLiteral("viewer_count")));
        QVariantMap m;
        m[QStringLiteral("slug")] = slug;
        m[QStringLiteral("name")] = text(ch.value(QStringLiteral("username"))).isEmpty() ? slug : text(ch.value(QStringLiteral("username")));
        m[QStringLiteral("title")] = text(o.value(QStringLiteral("title")));
        m[QStringLiteral("viewers")] = viewers;
        m[QStringLiteral("viewersText")] = viewersText(viewers);
        m[QStringLiteral("thumb")] = text(o.value(QStringLiteral("thumbnail")).toObject().value(QStringLiteral("src")));
        m[QStringLiteral("avatar")] = text(ch.value(QStringLiteral("profile_pic")));
        m[QStringLiteral("category")] = text(cat.value(QStringLiteral("name")));
        m[QStringLiteral("categoryId")] = whole(cat.value(QStringLiteral("id")));
        m[QStringLiteral("language")] = text(o.value(QStringLiteral("language")));
        m[QStringLiteral("mature")] = o.value(QStringLiteral("is_mature")).toBool(false);
        out->append(m);
    }
    if (nextCursor) *nextCursor = text(data.value(QStringLiteral("pagination")).toObject().value(QStringLiteral("next_cursor")));
    return true;
}

bool KickClient::parseCategories(const QByteArray &json, QVariantList *out)
{
    bool ok = false;
    const QJsonObject root = object(json, &ok);
    if (!ok || !root.contains(QStringLiteral("data"))) return false;
    for (const QJsonValue &v : root.value(QStringLiteral("data")).toArray()) {
        const QJsonObject o = v.toObject();
        const int id = whole(o.value(QStringLiteral("id")));
        const QString name = text(o.value(QStringLiteral("name")));
        if (id <= 0 || name.isEmpty()) continue;
        out->append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("slug"), text(o.value(QStringLiteral("slug")))}});
    }
    return true;
}

bool KickClient::parseSearch(const QByteArray &json, QVariantList *channels)
{
    bool ok = false;
    const QJsonObject root = object(json, &ok);
    if (!ok || !root.contains(QStringLiteral("channels"))) return false;
    for (const QJsonValue &v : root.value(QStringLiteral("channels")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString slug = normalize(text(o.value(QStringLiteral("slug"))));
        if (slug.isEmpty()) continue;
        const QJsonObject user = o.value(QStringLiteral("user")).toObject();
        QVariantMap m;
        m[QStringLiteral("slug")] = slug;
        m[QStringLiteral("name")] = text(user.value(QStringLiteral("username"))).isEmpty() ? slug : text(user.value(QStringLiteral("username")));
        m[QStringLiteral("title")] = QString();
        m[QStringLiteral("viewers")] = 0;
        m[QStringLiteral("viewersText")] = QString();
        m[QStringLiteral("thumb")] = QString();
        m[QStringLiteral("avatar")] = text(user.value(QStringLiteral("profilePic")));
        m[QStringLiteral("category")] = QString();
        m[QStringLiteral("categoryId")] = 0;
        m[QStringLiteral("language")] = QString();
        m[QStringLiteral("mature")] = false;
        m[QStringLiteral("live")] = o.value(QStringLiteral("isLive")).toBool(false);
        m[QStringLiteral("followers")] = whole(o.value(QStringLiteral("followers_count")));
        channels->append(m);
    }
    return true;
}

bool KickClient::parseChannel(const QByteArray &json, QVariantMap *out)
{
    bool ok = false;
    const QJsonObject o = object(json, &ok);
    if (!ok) return false;
    const QString slug = normalize(text(o.value(QStringLiteral("slug"))));
    if (slug.isEmpty()) return false;
    const QJsonObject user = o.value(QStringLiteral("user")).toObject();
    const QJsonValue live = o.value(QStringLiteral("livestream"));
    const QJsonObject ls = live.toObject();
    const QJsonArray cats = ls.value(QStringLiteral("categories")).toArray();
    const int viewers = whole(ls.value(QStringLiteral("viewer_count")));
    QVariantMap m;
    m[QStringLiteral("slug")] = slug;
    m[QStringLiteral("name")] = text(user.value(QStringLiteral("username"))).isEmpty() ? slug : text(user.value(QStringLiteral("username")));
    m[QStringLiteral("avatar")] = !text(user.value(QStringLiteral("profile_pic"))).isEmpty() ? text(user.value(QStringLiteral("profile_pic"))) : text(user.value(QStringLiteral("profilePic")));
    m[QStringLiteral("live")] = live.isObject();
    m[QStringLiteral("title")] = text(ls.value(QStringLiteral("session_title")));
    m[QStringLiteral("viewers")] = viewers;
    m[QStringLiteral("viewersText")] = viewersText(viewers);
    m[QStringLiteral("category")] = cats.isEmpty() ? QString() : text(cats.first().toObject().value(QStringLiteral("name")));
    m[QStringLiteral("playbackUrl")] = live.isObject() ? text(o.value(QStringLiteral("playback_url"))) : QString();
    m[QStringLiteral("chatroomId")] = whole(o.value(QStringLiteral("chatroom")).toObject().value(QStringLiteral("id")));
    m[QStringLiteral("followers")] = whole(o.value(QStringLiteral("followers_count")));
    m[QStringLiteral("thumb")] = text(ls.value(QStringLiteral("thumbnail")).toObject().value(QStringLiteral("url")));
    m[QStringLiteral("mature")] = ls.value(QStringLiteral("is_mature")).toBool(false);
    m[QStringLiteral("language")] = text(ls.value(QStringLiteral("language")));
    *out = m;
    return true;
}

// ---- what the person asks for ---------------------------------------------------------------------------

bool KickClient::ready()
{
    if (!m_open) return false;
    if (!m_net->allowed(kApp)) { setError(QStringLiteral("this app is not allowed to use the internet (settings > apps)")); return false; }
    return true;
}

void KickClient::setError(const QString &e)
{
    if (e == m_error) return;
    m_error = e;
    emit errorChanged();
}

void KickClient::open()
{
    if (m_open) return;
    m_open = true;
    m_net->open(kApp);
    emit openChanged();
    setError(QString());
    if (!ready()) return;
    requestCategories();
    refresh();
    refreshFollowing();
}

void KickClient::close()
{
    if (!m_open) return;
    m_open = false;
    ++m_listGen; ++m_channelGen;
    m_net->close(kApp);
    m_pending = 0;
    m_followChecks = 0;
    m_streams.clear(); m_raw.clear(); m_next.clear(); m_channel.clear(); m_status.clear(); m_searchChannels.clear();
    m_query.clear();
    emit loadingChanged(); emit streamsChanged(); emit channelChanged(); emit listsChanged(); emit filtersChanged(); emit openChanged();
}

void KickClient::refresh()
{
    if (!ready()) return;
    if (!m_query.isEmpty()) { search(m_query); return; }
    m_raw.clear(); m_streams.clear(); m_next.clear(); m_emptyPages = 0;
    emit streamsChanged();
    requestList(false);
    requestDeveloper();
}

void KickClient::loadMore()
{
    if (!ready() || m_next.isEmpty() || m_pending > 0 || !m_query.isEmpty()) return;
    requestList(true);
}

void KickClient::requestList(bool more)
{
    QUrl url(QStringLiteral("https://web.kick.com/api/v1/livestreams"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("limit"), QString::number(kPageSize));
    q.addQueryItem(QStringLiteral("sort"), m_sort == QLatin1String("featured") ? QStringLiteral("featured") : QStringLiteral("viewer_count_desc"));
    if (!m_language.isEmpty()) q.addQueryItem(QStringLiteral("language"), m_language);
    if (m_categoryId > 0) q.addQueryItem(QStringLiteral("category_id"), QString::number(m_categoryId));
    if (more && !m_next.isEmpty()) q.addQueryItem(QStringLiteral("after"), m_next);
    url.setQuery(q);
    const quint64 gen = ++m_listGen;
    setError(QString());
    begin();
    m_net->get(kApp, url, [this, gen, more](NetReply r) {
        if (gen != m_listGen) { end(); return; }                   // a newer list was asked for meanwhile
        end();
        QVariantList got;
        QString next;
        if (!r.error.isEmpty()) { setError(r.error); return; }
        if (!parseLivestreams(r.body, &got, &next)) { setError(QStringLiteral("kick answered, but not in a form this version understands")); return; }
        m_next = next;
        m_raw.append(got);
        applyFilters();
        // a whole page hidden by "blocked" or "18+" would look like a dead end: ask for the next one (at most 3 times in a row)
        if (m_streams.isEmpty() && !m_next.isEmpty() && m_emptyPages < 3) { ++m_emptyPages; requestList(true); }
        else m_emptyPages = 0;
        Q_UNUSED(more)
    });
}

void KickClient::applyFilters()
{
    QVariantList shown;
    const QVariantList &src = m_query.isEmpty() ? m_raw : m_searchChannels;
    for (const QVariant &v : src) {
        const QVariantMap m = v.toMap();
        if (m_blocked.contains(m.value(QStringLiteral("slug")).toString())) continue;
        if (!m_showMature && m.value(QStringLiteral("mature")).toBool()) continue;
        shown.append(m);
    }
    if (m_query.isEmpty() && pinApplies() && !m_dev.isEmpty()) {
        const QString slug = m_dev.value(QStringLiteral("slug")).toString();
        const bool hidden = m_blocked.contains(slug) || (!m_showMature && m_dev.value(QStringLiteral("mature")).toBool());
        if (!hidden) {
            QVariantList withPin{m_dev};
            for (const QVariant &v : shown) if (v.toMap().value(QStringLiteral("slug")).toString() != slug) withPin.append(v);
            shown = withPin;
        }
    }
    m_streams = shown;
    emit streamsChanged();
}

void KickClient::requestDeveloper()
{
    m_dev.clear();
    if (!pinApplies()) return;
    const quint64 gen = m_listGen;
    begin();
    m_net->get(kApp, QUrl(QStringLiteral("https://kick.com/api/v2/channels/") + QLatin1String(kDeveloperSlug)), [this, gen](NetReply r) {
        end();
        if (gen != m_listGen || !r.error.isEmpty()) return;        // not asked for any more, or not reachable: simply no pinned card
        QVariantMap ch;
        if (!parseChannel(r.body, &ch) || !ch.value(QStringLiteral("live")).toBool()) return;
        if (ch.value(QStringLiteral("slug")).toString() != QLatin1String(kDeveloperSlug)) return;      // only that channel can take the card
        ch[QStringLiteral("developer")] = true;
        m_dev = ch;
        applyFilters();
    });
}

void KickClient::requestCategories()
{
    if (!m_categories.isEmpty()) return;
    const quint64 gen = m_channelGen;
    begin();
    m_net->get(kApp, QUrl(QStringLiteral("https://kick.com/api/v1/subcategories?page=1")), [this, gen](NetReply r) {
        end();
        if (gen != m_channelGen || !r.error.isEmpty()) return;      // categories are optional: the grid works without them
        QVariantList cats;
        if (parseCategories(r.body, &cats)) { m_categories = cats; emit categoriesChanged(); }
    });
}

void KickClient::search(const QString &text)
{
    m_query = text.trimmed();
    emit filtersChanged();
    if (!ready()) return;
    if (m_query.isEmpty()) { m_searchChannels.clear(); refresh(); return; }
    QUrl url(QStringLiteral("https://kick.com/api/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("searched_word"), m_query);
    url.setQuery(q);
    const quint64 gen = ++m_listGen;
    setError(QString());
    m_next.clear();
    begin();
    m_net->get(kApp, url, [this, gen](NetReply r) {
        end();
        if (gen != m_listGen) return;
        if (!r.error.isEmpty()) { setError(r.error); return; }
        QVariantList ch;
        if (!parseSearch(r.body, &ch)) { setError(QStringLiteral("kick answered, but not in a form this version understands")); return; }
        m_searchChannels = ch;
        applyFilters();
    });
}

void KickClient::setSort(const QString &s)
{
    const QString v = s == QLatin1String("featured") ? s : QStringLiteral("viewers");
    if (v == m_sort) return;
    m_sort = v; emit filtersChanged(); refresh();
}
void KickClient::setLanguage(const QString &l)
{
    const QString v = l.trimmed().left(5).toLower();
    if (v == m_language) return;
    m_language = v; emit filtersChanged(); refresh();
}
void KickClient::setCategoryId(int id)
{
    id = qMax(0, id);
    if (id == m_categoryId) return;
    m_categoryId = id; emit filtersChanged(); if (m_query.isEmpty()) refresh(); else applyFilters();
}
void KickClient::setShowMature(bool on)
{
    if (on == m_showMature) return;
    m_showMature = on; emit filtersChanged(); applyFilters();
}

void KickClient::openChannel(const QString &slugOrLink)
{
    const QString slug = normalize(slugOrLink);
    if (slug.isEmpty()) { setError(QStringLiteral("that is not a kick channel name or link")); return; }
    if (!ready()) return;
    const quint64 gen = ++m_channelGen;
    m_channel = QVariantMap{{QStringLiteral("slug"), slug}, {QStringLiteral("name"), slug}, {QStringLiteral("loading"), true}};
    emit channelChanged();
    setError(QString());
    begin();
    m_net->get(kApp, QUrl(QStringLiteral("https://kick.com/api/v2/channels/") + slug), [this, gen, slug](NetReply r) {
        end();
        if (gen != m_channelGen) return;
        QVariantMap ch;
        if (r.status == 404) { m_channel = QVariantMap{{QStringLiteral("slug"), slug}, {QStringLiteral("name"), slug}, {QStringLiteral("notFound"), true}}; emit channelChanged(); return; }
        if (!r.error.isEmpty()) { m_channel = QVariantMap{{QStringLiteral("slug"), slug}, {QStringLiteral("name"), slug}, {QStringLiteral("error"), r.error}}; emit channelChanged(); setError(r.error); return; }
        if (!parseChannel(r.body, &ch)) {
            const QString e = QStringLiteral("kick answered, but not in a form this version understands");
            m_channel = QVariantMap{{QStringLiteral("slug"), slug}, {QStringLiteral("name"), slug}, {QStringLiteral("error"), e}};
            emit channelChanged(); setError(e); return;
        }
        m_channel = ch;
        emit channelChanged();
    });
}

void KickClient::closeChannel()
{
    ++m_channelGen;
    m_channel.clear();
    emit channelChanged();
}

// ---- the lists kept on this computer ---------------------------------------------------------------------

QVariantList KickClient::following() const
{
    QVariantList out;
    for (const QString &slug : m_following) {
        QVariantMap m = m_status.value(slug);
        if (m.isEmpty()) m = QVariantMap{{QStringLiteral("live"), false}, {QStringLiteral("viewers"), 0}, {QStringLiteral("checked"), false}};
        m[QStringLiteral("slug")] = slug;
        if (!m.contains(QStringLiteral("name"))) m[QStringLiteral("name")] = slug;
        out.append(m);
    }
    return out;
}

bool KickClient::follow(const QString &s)
{
    const QString slug = normalize(s);
    if (slug.isEmpty() || m_following.contains(slug) || m_following.size() >= 200) return false;
    m_blocked.removeAll(slug);                 // following someone you had blocked lifts the block
    m_following.append(slug);
    emit listsChanged(); emit listsEdited();
    return true;
}

void KickClient::unfollow(const QString &s)
{
    if (m_following.removeAll(normalize(s)) > 0) { m_status.remove(normalize(s)); emit listsChanged(); emit listsEdited(); }
}

bool KickClient::block(const QString &s)
{
    const QString slug = normalize(s);
    if (slug.isEmpty() || m_blocked.contains(slug) || m_blocked.size() >= 1000) return false;
    m_blocked.append(slug);
    m_following.removeAll(slug);               // a blocked channel is not followed
    m_status.remove(slug);
    emit listsChanged(); emit listsEdited();
    applyFilters();
    return true;
}

void KickClient::unblock(const QString &s)
{
    if (m_blocked.removeAll(normalize(s)) > 0) { emit listsChanged(); emit listsEdited(); applyFilters(); }
}

void KickClient::clearBlocked()
{
    if (m_blocked.isEmpty()) return;
    m_blocked.clear();
    emit listsChanged(); emit listsEdited();
    applyFilters();
}

void KickClient::setChatMode(const QString &mode)
{
    QString m = mode == QLatin1String("pretend") ? mode : (mode == QLatin1String("kick") && m_signedIn ? mode : QStringLiteral("off"));
    if (m == m_chatMode) return;
    m_chatMode = m;
    emit chatModeChanged();
    emit listsEdited();                       // worth saving with the profile
}

QJsonObject KickClient::listsJson() const
{
    return QJsonObject{{QStringLiteral("following"), QJsonArray::fromStringList(m_following)}, {QStringLiteral("blocked"), QJsonArray::fromStringList(m_blocked)},
                       {QStringLiteral("chatMode"), m_chatMode}};
}

void KickClient::setListsJson(const QJsonObject &o)
{
    m_following.clear(); m_blocked.clear(); m_status.clear();
    for (const QJsonValue &v : o.value(QStringLiteral("following")).toArray()) { const QString s = normalize(v.toString()); if (!s.isEmpty() && !m_following.contains(s)) m_following.append(s); }
    for (const QJsonValue &v : o.value(QStringLiteral("blocked")).toArray()) { const QString s = normalize(v.toString()); if (!s.isEmpty() && !m_blocked.contains(s)) m_blocked.append(s); }
    for (const QString &s : m_blocked) m_following.removeAll(s);
    const QString cm = o.value(QStringLiteral("chatMode")).toString();
    m_chatMode = cm == QLatin1String("pretend") || (cm == QLatin1String("kick") && m_signedIn) ? cm : QStringLiteral("off");
    emit chatModeChanged();
    emit listsChanged();
    if (!m_raw.isEmpty() || !m_searchChannels.isEmpty()) applyFilters();
}

// one lookup per followed channel, a few at a time, only when asked
void KickClient::refreshFollowing()
{
    if (!ready() || m_following.isEmpty()) return;
    const quint64 gen = m_channelGen;
    const QStringList todo = m_following.mid(0, 40);
    auto next = std::make_shared<int>(0);
    auto step = std::make_shared<std::function<void()>>();
    *step = [this, todo, gen, next, step] {
        if (gen != m_channelGen || *next >= todo.size() || !m_open) return;
        const QString slug = todo.at((*next)++);
        begin();
        m_net->get(kApp, QUrl(QStringLiteral("https://kick.com/api/v2/channels/") + slug), [this, gen, slug, step](NetReply r) {
            end();
            if (gen != m_channelGen) return;
            QVariantMap ch;
            if (r.error.isEmpty() && parseChannel(r.body, &ch) && m_following.contains(slug)) {
                m_status[slug] = QVariantMap{{QStringLiteral("live"), ch.value(QStringLiteral("live"))}, {QStringLiteral("viewers"), ch.value(QStringLiteral("viewers"))},
                                             {QStringLiteral("name"), ch.value(QStringLiteral("name"))}, {QStringLiteral("avatar"), ch.value(QStringLiteral("avatar"))},
                                             {QStringLiteral("checked"), true}};
                emit listsChanged();
            }
            (*step)();
        });
    };
    for (int i = 0; i < 3; ++i) (*step)();       // three at a time
}

} // namespace hop

bool hop::KickClient::streamAllowed(const QString &url) const
{
    return hop::AppNet::streamUrlAllowed(QStringLiteral("kick"), QUrl(url));
}
