// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps/KickClient.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QSet>
#include <QTimer>
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

bool KickClient::parseSearchCategories(const QByteArray &json, QVariantList *out)
{
    bool ok = false;
    const QJsonObject root = object(json, &ok);
    if (!ok || !root.contains(QStringLiteral("categories"))) return false;
    for (const QJsonValue &v : root.value(QStringLiteral("categories")).toArray()) {
        const QJsonObject o = v.toObject();
        const int id = whole(o.value(QStringLiteral("id")));
        const QString name = text(o.value(QStringLiteral("name")));
        if (id <= 0 || name.isEmpty()) continue;
        out->append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), name.left(60)}, {QStringLiteral("mature"), o.value(QStringLiteral("is_mature")).toBool(false)}});
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
    if (m_health == QLatin1String("down")) {
        setError(QStringLiteral("kick looks down right now. nothing new is loaded until it answers again; your downloads still play"));
        return false;
    }
    return true;
}

// ---- the health light ------------------------------------------------------------------------------------
// While the app is open it asks two tiny questions of Kick's own servers (the list and a channel page, never anything of yours) every
// minute, and more often while something looks wrong. The first list the app loads counts too, so the light is there at once.

QString KickClient::healthText() const
{
    if (m_health == QLatin1String("good")) return QStringLiteral("kick is up");
    if (m_health == QLatin1String("slow")) return QStringLiteral("kick is slow or partly down");
    if (m_health == QLatin1String("down")) return QStringLiteral("kick looks down");
    return QStringLiteral("checking kick");
}

int KickClient::classifyHealth(const NetReply &r, qint64 ms)
{
    if (r.status == 0 || r.status >= 500) return 2;                 // nothing came back, or kick itself failed
    if (r.status == 429 || r.status == 403 || ms > 5000) return 1;  // refusing for now, or very slow
    return 0;                                                       // (a 404 means kick answered)
}

QString KickClient::healthFrom(const QList<int> &last)
{
    if (last.isEmpty()) return QString();
    if (last.size() >= 3 && std::all_of(last.begin(), last.end(), [](int o) { return o == 2; })) return QStringLiteral("down");
    return std::any_of(last.begin(), last.end(), [](int o) { return o != 0; }) ? QStringLiteral("slow") : QStringLiteral("good");
}

void KickClient::healthNoted(int outcome)
{
    if (!m_open) return;
    m_healthLast.append(outcome);
    while (m_healthLast.size() > 3) m_healthLast.removeFirst();
    const QString now = healthFrom(m_healthLast), was = m_health;
    if (now != was) {
        m_health = now;
        emit healthChanged();
        if (was == QLatin1String("down") && ready()) {            // kick is back: pick up where we were, with fresh results
            setError(QString());
            refresh();
            if (!m_channel.isEmpty() && !m_channel.value(QStringLiteral("slug")).toString().isEmpty()) openChannel(m_channel.value(QStringLiteral("slug")).toString());
        } else if (now == QLatin1String("down")) {
            ++m_listGen; ++m_channelGen;                             // whatever is still on its way is not going to be used
            m_pending = 0; emit loadingChanged();
            setError(QStringLiteral("kick looks down right now. nothing new is loaded until it answers again; your downloads still play"));
        }
    }
    healthSchedule();
}

void KickClient::healthSchedule()
{
    if (!m_open) return;
    if (!m_healthTimer) {
        m_healthTimer = new QTimer(this);
        m_healthTimer->setSingleShot(true);
        connect(m_healthTimer, &QTimer::timeout, this, &KickClient::healthProbe);
    }
    const int div = m_health == QLatin1String("down") ? 4 : (m_health == QLatin1String("slow") ? 2 : 1);
    m_healthTimer->start(qMax(25, m_healthMs / div));
}

void KickClient::healthProbe()
{
    if (!m_open || !m_net->allowed(kApp)) return;
    const quint64 gen = m_healthGen;
    struct Tally { int outcome = 0; int failed = 0; int pending = 2; };
    auto t = std::make_shared<Tally>();
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();
    NetLimits lim; lim.maxBody = 512 * 1024; lim.timeoutMs = 8000;
    auto done = [this, gen, t, timer](NetReply r) {
        if (gen != m_healthGen) return;
        const int o = classifyHealth(r, timer->elapsed());
        t->outcome = qMax(t->outcome, o);
        if (o == 2) ++t->failed;
        if (--t->pending > 0) return;
        healthNoted(t->failed >= 2 ? 2 : (t->failed == 1 ? 1 : t->outcome));
    };
    QUrl list(QStringLiteral("https://web.kick.com/api/v1/livestreams"));
    QUrlQuery q; q.addQueryItem(QStringLiteral("limit"), QStringLiteral("1")); list.setQuery(q);
    m_net->get(kApp, list, done, lim);
    m_net->get(kApp, QUrl(QStringLiteral("https://kick.com/api/v2/channels/") + QLatin1String(kDeveloperSlug)), done, lim);
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
    ++m_healthGen;
    m_healthLast.clear(); m_health.clear();
    emit openChanged(); emit healthChanged();
    healthSchedule();
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
    ++m_listGen; ++m_channelGen; ++m_healthGen;
    if (m_healthTimer) m_healthTimer->stop();
    m_healthLast.clear(); m_health.clear();
    m_net->close(kApp);
    m_pending = 0;
    m_followChecks = 0;
    m_streams.clear(); m_raw.clear(); m_next.clear(); m_channel.clear(); m_status.clear(); m_searchChannels.clear();
    m_query.clear();
    emit loadingChanged(); emit streamsChanged(); emit channelChanged(); emit listsChanged(); emit filtersChanged(); emit openChanged(); emit healthChanged();
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
    if (!language().isEmpty()) q.addQueryItem(QStringLiteral("language"), language());
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
        if (m_healthLast.isEmpty()) healthNoted(r.status == 0 || r.status >= 500 ? 2 : 0);   // the first list tells us how Kick is, at once
        if (!r.error.isEmpty()) { setError(r.error); return; }
        if (!parseLivestreams(r.body, &got, &next)) { setError(QStringLiteral("kick answered, but not in a form this version understands")); return; }
        m_next = next;
        m_raw.append(got);
        remember(got);
        applyFilters();
        // a whole page hidden by "blocked" or "18+" would look like a dead end: ask for the next one (at most 3 times in a row)
        if (m_streams.isEmpty() && !m_next.isEmpty() && m_emptyPages < 3) { ++m_emptyPages; requestList(true); }
        else m_emptyPages = 0;
        Q_UNUSED(more)
    });
}

namespace {
// a stream whose language Kick does not say is kept (nothing to go on); one that says something other than English is not
bool isEnglish(const QString &lang) { return lang.isEmpty() || lang.compare(QLatin1String("en"), Qt::CaseInsensitive) == 0 || lang.compare(QLatin1String("english"), Qt::CaseInsensitive) == 0; }
} // namespace

void KickClient::applyFilters()
{
    QVariantList shown;
    // a search has two kinds of answer: live streams playing a matching game or category, and channels with a similar name
    // (the scope chooses one or both; "everything" lists the streams first and the channels after, nobody twice)
    QVariantList merged;
    if (!m_query.isEmpty()) {
        QSet<QString> seen;
        if (m_searchScope != QLatin1String("channels"))
            for (const QVariant &v : m_searchStreams) { merged.append(v); seen.insert(v.toMap().value(QStringLiteral("slug")).toString()); }
        if (m_searchScope != QLatin1String("streams"))
            for (const QVariant &v : m_searchChannels) if (!seen.contains(v.toMap().value(QStringLiteral("slug")).toString())) merged.append(v);
    }
    const int gameCount = m_searchScope == QLatin1String("channels") ? 0 : int(m_searchStreams.size());
    const QVariantList &src = m_query.isEmpty() ? m_raw : merged;
    for (const QVariant &v : src) {
        const QVariantMap m = v.toMap();
        if (m_blocked.contains(m.value(QStringLiteral("slug")).toString())) continue;
        if (!m_showMature && m.value(QStringLiteral("mature")).toBool()) continue;
        if (m_englishOnly && !isEnglish(m.value(QStringLiteral("language")).toString())) continue;
        shown.append(m);
    }
    if (m_query.isEmpty() && pinApplies() && !m_dev.isEmpty()) {
        const QString slug = m_dev.value(QStringLiteral("slug")).toString();
        const bool hidden = m_blocked.contains(slug) || (!m_showMature && m_dev.value(QStringLiteral("mature")).toBool())
                            || (m_englishOnly && !isEnglish(m_dev.value(QStringLiteral("language")).toString()));
        if (!hidden) {
            QVariantList withPin{m_dev};
            for (const QVariant &v : shown) if (v.toMap().value(QStringLiteral("slug")).toString() != slug) withPin.append(v);
            shown = withPin;
        }
    }
    m_streams = shown;
    m_gameShown.clear(); m_channelShown.clear();
    if (!m_query.isEmpty()) {
        QSet<QString> gameSlugs;
        if (gameCount > 0) for (const QVariant &v : m_searchStreams) gameSlugs.insert(v.toMap().value(QStringLiteral("slug")).toString());
        for (const QVariant &v : shown) (gameSlugs.contains(v.toMap().value(QStringLiteral("slug")).toString()) ? m_gameShown : m_channelShown).append(v);
    }
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
        remember(ch);
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

void KickClient::remember(const QVariantMap &ch)
{
    const QString slug = ch.value(QStringLiteral("slug")).toString();
    if (slug.isEmpty()) return;
    QVariantMap &k = m_known[slug];
    for (const char *key : {"thumb", "avatar", "name"}) {
        const QString v = ch.value(QString::fromLatin1(key)).toString();
        if (!v.isEmpty()) k[QString::fromLatin1(key)] = v;
    }
    if (m_known.size() > 2000) m_known.clear();                               // a long session never grows without bound
}

void KickClient::remember(const QVariantList &channels)
{
    for (const QVariant &v : channels) remember(v.toMap());
}

// live search results have no picture of their own: ask for the channels that are live (a few at a time), and show them as they come
void KickClient::lookupThumbs(const QStringList &slugs, quint64 gen)
{
    if (slugs.isEmpty()) return;
    auto next = std::make_shared<int>(0);
    auto step = std::make_shared<std::function<void()>>();
    *step = [this, slugs, gen, next, step] {
        if (gen != m_listGen || *next >= slugs.size() || !m_open) return;
        const QString slug = slugs.at((*next)++);
        m_net->get(kApp, QUrl(QStringLiteral("https://kick.com/api/v2/channels/") + slug), [this, gen, slug, step](NetReply r) {
            if (gen != m_listGen) return;
            QVariantMap ch;
            if (r.error.isEmpty() && parseChannel(r.body, &ch)) {
                remember(ch);
                const QString thumb = ch.value(QStringLiteral("live")).toBool() ? ch.value(QStringLiteral("thumb")).toString() : QString();
                if (!thumb.isEmpty()) {
                    for (QVariant &v : m_searchChannels) {
                        QVariantMap m = v.toMap();
                        if (m.value(QStringLiteral("slug")).toString() != slug) continue;
                        m[QStringLiteral("thumb")] = thumb;
                        m[QStringLiteral("viewers")] = ch.value(QStringLiteral("viewers"));
                        m[QStringLiteral("viewersText")] = ch.value(QStringLiteral("viewersText"));
                        if (m.value(QStringLiteral("title")).toString().isEmpty()) m[QStringLiteral("title")] = ch.value(QStringLiteral("title"));
                        if (m.value(QStringLiteral("category")).toString().isEmpty()) m[QStringLiteral("category")] = ch.value(QStringLiteral("category"));
                        v = m;
                    }
                    applyFilters();
                }
            }
            (*step)();
        });
    };
    for (int i = 0; i < 3; ++i) (*step)();
}

void KickClient::requestCategoryStreams(const QVariantList &categories, quint64 gen)
{
    QStringList names;
    for (const QVariant &v : categories) names << v.toMap().value(QStringLiteral("name")).toString();
    for (const QVariant &cv : categories) {
        const int id = cv.toMap().value(QStringLiteral("id")).toInt();
        QUrl url(QStringLiteral("https://web.kick.com/api/v1/livestreams"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("limit"), QString::number(kPageSize));
        q.addQueryItem(QStringLiteral("sort"), QStringLiteral("viewer_count_desc"));
        if (!language().isEmpty()) q.addQueryItem(QStringLiteral("language"), language());
        q.addQueryItem(QStringLiteral("category_id"), QString::number(id));
        url.setQuery(q);
        begin();
        m_net->get(kApp, url, [this, gen, names](NetReply r) {
            end();
            if (gen != m_listGen) return;
            QVariantList got;
            QString next;
            if (!r.error.isEmpty() || !parseLivestreams(r.body, &got, &next) || got.isEmpty()) return;     // nothing playing it: just the channels
            remember(got);
            QSet<QString> seen;
            for (const QVariant &v : m_searchStreams) seen.insert(v.toMap().value(QStringLiteral("slug")).toString());
            for (const QVariant &v : got) if (!seen.contains(v.toMap().value(QStringLiteral("slug")).toString())) m_searchStreams.append(v);
            m_searchCategory = names.join(QStringLiteral(" / "));
            applyFilters();
        });
    }
}

namespace {
// lower case, letters and digits only: "Lumielle VT_uber" -> "lumiellevtuber"
QString plain(const QString &s)
{
    QString out;
    for (QChar c : s.toLower()) if (c.isLetterOrNumber()) out += c;
    return out;
}
int editDistance(const QString &a, const QString &b)
{
    QVector<int> prev(b.size() + 1), cur(b.size() + 1);
    for (int j = 0; j <= b.size(); ++j) prev[j] = j;
    for (int i = 1; i <= a.size(); ++i) {
        cur[0] = i;
        for (int j = 1; j <= b.size(); ++j)
            cur[j] = qMin(qMin(prev[j] + 1, cur[j - 1] + 1), prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1));
        std::swap(prev, cur);
    }
    return prev[b.size()];
}
} // namespace

int KickClient::nameScore(const QString &typedIn, const QString &candidateIn)
{
    const QString q = plain(typedIn), c = plain(candidateIn);
    if (q.isEmpty() || c.isEmpty()) return 0;
    if (q == c) return 1000;
    if (c.startsWith(q)) return 900 - qMin(99, c.size() - q.size());                 // the name begins with what was typed: shorter (closer) first
    if (q.startsWith(c) && c.size() >= 3) return 800 + qMin(99, c.size());           // what was typed is the name plus more ("... vtuber")
    const int at = c.indexOf(q);
    if (at > 0) return 700 - qMin(99, at);                                           // somewhere inside the name
    const int d = editDistance(q, c.left(qMax(q.size(), 1) + 2));                    // otherwise the nearest spelling
    const double sim = 1.0 - double(d) / double(qMax(q.size(), 1));
    return sim > 0 ? int(600 * sim) : 0;
}

void KickClient::rankByName(QVariantList *channels, const QString &typed)
{
    std::stable_sort(channels->begin(), channels->end(), [&typed](const QVariant &x, const QVariant &y) {
        const QVariantMap a = x.toMap(), b = y.toMap();
        const int sa = qMax(nameScore(typed, a.value(QStringLiteral("slug")).toString()), nameScore(typed, a.value(QStringLiteral("name")).toString()));
        const int sb = qMax(nameScore(typed, b.value(QStringLiteral("slug")).toString()), nameScore(typed, b.value(QStringLiteral("name")).toString()));
        return sa > sb;
    });
}

void KickClient::search(const QString &text)
{
    m_query = text.trimmed();
    emit filtersChanged();
    if (!ready()) return;
    m_searchStreams.clear(); m_searchCategory.clear();
    if (m_query.isEmpty()) { ++m_listGen; m_searchChannels.clear(); m_searchScope = QStringLiteral("all"); m_gameShown.clear(); m_channelShown.clear(); emit filtersChanged(); emit streamsChanged(); refresh(); return; }
    const quint64 gen = ++m_listGen;
    setError(QString());
    m_next.clear();
    requestSearch(m_query, gen, m_query.contains(QLatin1Char(' ')));
}

// Kick finds nothing for a phrase like "name vtuber": then it is asked again with the longest single word, and the answers are ranked by
// how close each name is to everything that was typed
void KickClient::requestSearch(const QString &word, quint64 gen, bool mayRetry)
{
    QUrl url(QStringLiteral("https://kick.com/api/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("searched_word"), word);
    url.setQuery(q);
    begin();
    m_net->get(kApp, url, [this, gen, mayRetry](NetReply r) {
        end();
        if (gen != m_listGen) return;
        if (!r.error.isEmpty()) { setError(r.error); return; }
        QVariantList ch;
        if (!parseSearch(r.body, &ch)) { setError(QStringLiteral("kick answered, but not in a form this version understands")); return; }
        if (ch.isEmpty() && mayRetry) {
            QString longest;
            for (const QString &w : m_query.split(QLatin1Char(' '), Qt::SkipEmptyParts)) if (plain(w).size() > plain(longest).size()) longest = w;
            if (plain(longest).size() >= 2) { requestSearch(longest, gen, false); return; }
        }
        rankByName(&ch, m_query);
        // a live result gets its stream picture from what was seen before, or by asking (a few at a time); the rest show their avatar
        QStringList need;
        for (QVariant &v : ch) {
            QVariantMap m = v.toMap();
            const QString slug = m.value(QStringLiteral("slug")).toString();
            const QVariantMap k = m_known.value(slug);
            if (m.value(QStringLiteral("avatar")).toString().isEmpty() && !k.value(QStringLiteral("avatar")).toString().isEmpty()) m[QStringLiteral("avatar")] = k.value(QStringLiteral("avatar"));
            if (m.value(QStringLiteral("live")).toBool() && m.value(QStringLiteral("thumb")).toString().isEmpty()) {
                if (!k.value(QStringLiteral("thumb")).toString().isEmpty()) m[QStringLiteral("thumb")] = k.value(QStringLiteral("thumb"));
                else if (need.size() < 12) need << slug;
            }
            v = m;
        }
        m_searchChannels = ch;
        applyFilters();
        lookupThumbs(need, gen);
        // a typed game or category ("Palworld") finds streams PLAYING it, not just channels with a similar name
        QVariantList cats, chosen;
        if (parseSearchCategories(r.body, &cats)) {
            for (const QVariant &v : cats) {
                const QVariantMap m = v.toMap();
                if (chosen.size() < 2 && nameScore(m_query, m.value(QStringLiteral("name")).toString()) >= 700) chosen.append(m);
            }
        }
        if (!chosen.isEmpty()) requestCategoryStreams(chosen, gen);
    });
}

void KickClient::setSearchScope(const QString &s)
{
    const QString v = s == QLatin1String("streams") || s == QLatin1String("channels") ? s : QStringLiteral("all");
    if (v == m_searchScope) return;
    m_searchScope = v;
    emit filtersChanged();
    if (!m_query.isEmpty()) applyFilters();                    // no new request: the same answers, looked at another way
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
void KickClient::setEnglishOnly(bool on)
{
    if (on == m_englishOnly) return;
    m_englishOnly = on;
    emit filtersChanged();
    emit listsEdited();                                       // a setting worth saving with the profile
    if (m_open) refresh();
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
    ++m_libraryGen;
    m_past.clear(); m_clips.clear(); m_libraryState.clear();
    emit libraryChanged();
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
        remember(ch);
        emit channelChanged();
    });
}

void KickClient::closeChannel()
{
    ++m_channelGen;
    ++m_libraryGen;
    m_past.clear(); m_clips.clear(); m_libraryState.clear();
    emit libraryChanged();
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
    QString m = mode == QLatin1String("pretend") ? mode : QStringLiteral("off");
    if (m == m_chatMode) return;
    m_chatMode = m;
    emit chatModeChanged();
    emit listsEdited();                       // worth saving with the profile
}

QJsonObject KickClient::listsJson() const
{
    QJsonObject out{{QStringLiteral("following"), QJsonArray::fromStringList(m_following)}, {QStringLiteral("blocked"), QJsonArray::fromStringList(m_blocked)},
                       {QStringLiteral("chatMode"), m_chatMode}, {QStringLiteral("playlists"), playlistsJson()},
                       {QStringLiteral("englishOnly"), m_englishOnly}, {QStringLiteral("askBeforeOpening"), m_askBeforeOpening}, {QStringLiteral("pinnedCategories"), pinnedJson()}};
    return out;
}

QJsonObject KickClient::pinnedJson() const
{
    QJsonArray a;
    for (const QVariant &v : m_pinned) { const QVariantMap m = v.toMap(); a.append(QJsonObject{{QStringLiteral("id"), m.value(QStringLiteral("id")).toInt()}, {QStringLiteral("name"), m.value(QStringLiteral("name")).toString()}}); }
    return QJsonObject{{QStringLiteral("custom"), m_customCategories}, {QStringLiteral("items"), a}};
}

QJsonArray KickClient::playlistsJson() const
{
    QJsonArray arr;
    for (const Playlist &p : m_playlists) {
        QJsonArray items;
        for (const QVariant &v : p.items) items.append(QJsonValue::fromVariant(v));
        arr.append(QJsonObject{{QStringLiteral("id"), p.id}, {QStringLiteral("name"), p.name}, {QStringLiteral("kind"), p.kind}, {QStringLiteral("items"), items}});
    }
    return arr;
}

void KickClient::setListsJson(const QJsonObject &o)
{
    m_following.clear(); m_blocked.clear(); m_status.clear();
    for (const QJsonValue &v : o.value(QStringLiteral("following")).toArray()) { const QString s = normalize(v.toString()); if (!s.isEmpty() && !m_following.contains(s)) m_following.append(s); }
    for (const QJsonValue &v : o.value(QStringLiteral("blocked")).toArray()) { const QString s = normalize(v.toString()); if (!s.isEmpty() && !m_blocked.contains(s)) m_blocked.append(s); }
    for (const QString &s : m_blocked) m_following.removeAll(s);
    // playlists: anything that is not well formed is simply left out
    m_playlists.clear();
    for (const QJsonValue &pv : o.value(QStringLiteral("playlists")).toArray()) {
        if (m_playlists.size() >= kMaxPlaylists) break;
        const QJsonObject po = pv.toObject();
        Playlist p;
        p.id = po.value(QStringLiteral("id")).toString().left(60);
        p.name = po.value(QStringLiteral("name")).toString().trimmed().left(kMaxNameLength);
        p.kind = po.value(QStringLiteral("kind")).toString();
        if (p.id.isEmpty() || p.name.isEmpty() || (p.kind != QLatin1String("channels") && p.kind != QLatin1String("clips") && p.kind != QLatin1String("streams"))) continue;
        bool dup = false;
        for (const Playlist &q : m_playlists) if (q.id == p.id) dup = true;
        if (dup) continue;
        m_playlists.append(p);
        const QString id = p.id;
        for (const QJsonValue &iv : po.value(QStringLiteral("items")).toArray()) {
            if (m_playlists.last().items.size() >= kMaxItems) break;
            if (p.kind == QLatin1String("channels")) {
                const QString slug = normalize(iv.toString());
                if (!slug.isEmpty() && !isInPlaylist(id, slug)) m_playlists.last().items.append(slug);
            } else if (iv.isObject()) {
                QVariantMap m = iv.toObject().toVariantMap();
                const QString itemId = m.value(QStringLiteral("id")).toString();
                QString why;
                if (!itemId.isEmpty() && !isInPlaylist(id, itemId) && AppNet::urlAllowed(kApp, QUrl(m.value(QStringLiteral("url")).toString()), &why)) m_playlists.last().items.append(m);
            }
        }
    }
    m_importMessage.clear();
    emit importChanged();
    if (o.contains(QStringLiteral("account")))   // an older version kept sign-in tokens here: they are dropped, and the next save leaves them out
        QTimer::singleShot(0, this, [this] { emit listsEdited(); });
    {   // your category chips; anything not well formed is left out
        m_pinned.clear();
        const QJsonObject pc = o.value(QStringLiteral("pinnedCategories")).toObject();
        m_customCategories = pc.value(QStringLiteral("custom")).toBool(false);
        for (const QJsonValue &v : pc.value(QStringLiteral("items")).toArray()) {
            if (m_pinned.size() >= kMaxPinned) break;
            const QJsonObject co = v.toObject();
            const int id = whole(co.value(QStringLiteral("id")));
            const QString name = co.value(QStringLiteral("name")).toString().trimmed().left(60);
            bool dup = false;
            for (const QVariant &p : m_pinned) if (p.toMap().value(QStringLiteral("id")).toInt() == id) dup = true;
            if (id > 0 && !name.isEmpty() && !dup) m_pinned.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("slug"), QString()}});
        }
        m_categoryResults.clear();
        emit categoriesChanged();
        emit categoryResultsChanged();
    }
    m_englishOnly = o.value(QStringLiteral("englishOnly")).toBool(false);
    m_askBeforeOpening = o.value(QStringLiteral("askBeforeOpening")).toBool(true);
    emit filtersChanged();
    const QString cm = o.value(QStringLiteral("chatMode")).toString();
    m_chatMode = cm == QLatin1String("pretend") ? cm : QStringLiteral("off");
    emit chatModeChanged();
    emit listsChanged();
    if (!m_raw.isEmpty() || !m_searchChannels.isEmpty()) applyFilters();
}

// one lookup per followed channel, a few at a time, only when asked
void KickClient::refreshFollowing()
{
    if (!ready() || m_following.isEmpty()) return;
    refreshChannels(m_following.mid(0, 40));
}

void KickClient::refreshChannels(const QStringList &todo)
{
    if (!ready() || todo.isEmpty()) return;
    const quint64 gen = m_channelGen;
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
            if (r.error.isEmpty() && parseChannel(r.body, &ch)) remember(ch);
            if (r.error.isEmpty() && !ch.isEmpty() && (m_following.contains(slug) || channelInPlaylists(slug))) {
                m_status[slug] = QVariantMap{{QStringLiteral("live"), ch.value(QStringLiteral("live"))}, {QStringLiteral("viewers"), ch.value(QStringLiteral("viewers"))},
                                             {QStringLiteral("name"), ch.value(QStringLiteral("name"))}, {QStringLiteral("avatar"), ch.value(QStringLiteral("avatar"))},
                                             {QStringLiteral("thumb"), ch.value(QStringLiteral("live")).toBool() ? ch.value(QStringLiteral("thumb")) : QVariant(QString())},
                                             {QStringLiteral("checked"), true}};
                emit listsChanged();
            }
            (*step)();
        });
    };
    for (int i = 0; i < 3; ++i) (*step)();       // three at a time
}

// ---- playlists ---------------------------------------------------------------------------------------------

bool KickClient::channelInPlaylists(const QString &slug) const
{
    for (const Playlist &p : m_playlists)
        if (p.kind == QLatin1String("channels"))
            for (const QVariant &v : p.items) if (v.toString() == slug) return true;
    return false;
}

QString KickClient::createPlaylist(const QString &nameIn, const QString &kind)
{
    const QString name = nameIn.trimmed().left(kMaxNameLength);
    if (name.isEmpty() || m_playlists.size() >= kMaxPlaylists) return {};
    if (kind != QLatin1String("channels") && kind != QLatin1String("clips") && kind != QLatin1String("streams")) return {};
    Playlist p;
    p.id = QStringLiteral("pl") + QString::number(QDateTime::currentMSecsSinceEpoch(), 36);
    for (bool clash = true; clash;) {                         // (two made in the same millisecond, or after a delete: never the same id)
        clash = false;
        for (const Playlist &q : m_playlists) if (q.id == p.id) { p.id += QLatin1Char('x'); clash = true; break; }
    }
    p.name = name;
    p.kind = kind;
    m_playlists.append(p);
    emit listsChanged();
    emit listsEdited();
    return p.id;
}

bool KickClient::renamePlaylist(const QString &id, const QString &nameIn)
{
    const QString name = nameIn.trimmed().left(kMaxNameLength);
    if (name.isEmpty()) return false;
    for (Playlist &p : m_playlists)
        if (p.id == id) { p.name = name; emit listsChanged(); emit listsEdited(); return true; }
    return false;
}

bool KickClient::deletePlaylist(const QString &id)
{
    for (int i = 0; i < m_playlists.size(); ++i)
        if (m_playlists[i].id == id) { m_playlists.removeAt(i); emit listsChanged(); emit listsEdited(); return true; }
    return false;
}

bool KickClient::addToPlaylist(const QString &id, const QVariantMap &item)
{
    for (Playlist &p : m_playlists) {
        if (p.id != id) continue;
        if (p.items.size() >= kMaxItems) return false;
        if (p.kind == QLatin1String("channels")) {
            const QString slug = normalize(item.value(QStringLiteral("slug")).toString());
            if (slug.isEmpty() || isInPlaylist(id, slug)) return false;
            p.items.append(slug);
        } else {
            const QString itemId = item.value(QStringLiteral("id")).toString();
            const QString itemKind = item.value(QStringLiteral("kind")).toString();
            if (itemId.isEmpty() || itemId.size() > 120 || isInPlaylist(id, itemId)) return false;
            if ((p.kind == QLatin1String("clips")) != (itemKind == QLatin1String("clip"))) return false;       // a clip goes in a clips playlist, a stream in a streams one
            if (p.kind == QLatin1String("streams") && itemKind != QLatin1String("stream")) return false;
            QVariantMap m;
            for (const char *k : {"id", "kind", "title", "channel", "url", "thumb"}) m[QString::fromLatin1(k)] = item.value(QString::fromLatin1(k)).toString().left(500);
            m[QStringLiteral("seconds")] = item.value(QStringLiteral("seconds")).toDouble();
            m[QStringLiteral("estimate")] = item.value(QStringLiteral("estimate")).toDouble();
            QString why;
            if (!AppNet::urlAllowed(kApp, QUrl(m.value(QStringLiteral("url")).toString()), &why)) return false;      // only addresses this app may use
            p.items.append(m);
        }
        emit listsChanged();
        emit listsEdited();
        return true;
    }
    return false;
}

bool KickClient::removeFromPlaylist(const QString &id, const QString &key)
{
    for (Playlist &p : m_playlists) {
        if (p.id != id) continue;
        for (int i = 0; i < p.items.size(); ++i) {
            const QVariant &v = p.items[i];
            const QString k = v.typeId() == QMetaType::QString ? v.toString() : v.toMap().value(QStringLiteral("id")).toString();
            if (k == key) { p.items.removeAt(i); emit listsChanged(); emit listsEdited(); return true; }
        }
    }
    return false;
}

bool KickClient::isInPlaylist(const QString &id, const QString &keyIn) const
{
    for (const Playlist &p : m_playlists) {
        if (p.id != id) continue;
        const QString key = p.kind == QLatin1String("channels") ? normalize(keyIn) : keyIn;
        for (const QVariant &v : p.items) {
            const QString k = v.typeId() == QMetaType::QString ? v.toString() : v.toMap().value(QStringLiteral("id")).toString();
            if (k == key) return true;
        }
    }
    return false;
}

QVariantList KickClient::playlistsHolding(const QString &key) const
{
    QVariantList out;
    for (const Playlist &p : m_playlists) if (isInPlaylist(p.id, key)) out << p.id;
    return out;
}

QVariantList KickClient::playlists() const
{
    QVariantList out;
    for (const Playlist &p : m_playlists) {
        QVariantList items;
        for (const QVariant &v : p.items) {
            if (p.kind == QLatin1String("channels")) {
                const QString slug = v.toString();
                if (m_blocked.contains(slug)) continue;                       // a blocked streamer stays hidden everywhere
                QVariantMap m = m_status.value(slug);
                if (m.isEmpty()) m = QVariantMap{{QStringLiteral("live"), false}, {QStringLiteral("viewers"), 0}, {QStringLiteral("checked"), false}};
                m[QStringLiteral("slug")] = slug;
                if (!m.contains(QStringLiteral("name"))) m[QStringLiteral("name")] = slug;
                items << m;
            } else {
                items << v;
            }
        }
        out << QVariantMap{{QStringLiteral("id"), p.id}, {QStringLiteral("name"), p.name}, {QStringLiteral("kind"), p.kind},
                           {QStringLiteral("count"), items.size()}, {QStringLiteral("items"), items}};
    }
    return out;
}

void KickClient::refreshPlaylist(const QString &id)
{
    QStringList slugs;
    for (const Playlist &p : m_playlists)
        if (p.id == id && p.kind == QLatin1String("channels"))
            for (const QVariant &v : p.items) if (!m_blocked.contains(v.toString()) && slugs.size() < 40) slugs << v.toString();
    refreshChannels(slugs);
}

// ---- a channel's finished streams and clips (for downloading) ----------------------------------------------

bool KickClient::parsePastStreams(const QByteArray &json, QVariantList *out)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) return false;
    out->clear();
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("is_live")).toBool()) continue;
        const QString source = o.value(QStringLiteral("source")).toString();
        const QString id = QString::number(o.value(QStringLiteral("id")).toVariant().toLongLong());
        if (id == QLatin1String("0") || source.isEmpty()) continue;
        const double seconds = o.value(QStringLiteral("duration")).toDouble() / 1000.0;        // Kick gives milliseconds here
        if (seconds <= 0) continue;
        QVariantMap m;
        m[QStringLiteral("id")] = QStringLiteral("stream-") + id;
        m[QStringLiteral("kind")] = QStringLiteral("stream");
        m[QStringLiteral("title")] = o.value(QStringLiteral("session_title")).toString();
        m[QStringLiteral("url")] = source;
        m[QStringLiteral("thumb")] = o.value(QStringLiteral("thumbnail")).toObject().value(QStringLiteral("src")).toString();
        m[QStringLiteral("seconds")] = seconds;
        m[QStringLiteral("date")] = o.value(QStringLiteral("start_time")).toString();
        m[QStringLiteral("viewers")] = o.value(QStringLiteral("viewer_count")).toInt();
        m[QStringLiteral("mature")] = o.value(QStringLiteral("is_mature")).toBool();
        m[QStringLiteral("estimate")] = double(seconds * 3484773.0 / 8.0);                       // ~ the 720p60 rate Kick uses: shown as "about"
        out->append(m);
    }
    return true;
}

bool KickClient::parseClips(const QByteArray &json, QVariantList *out)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject() || !doc.object().value(QStringLiteral("clips")).isArray()) return false;
    out->clear();
    for (const QJsonValue &v : doc.object().value(QStringLiteral("clips")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString id = o.value(QStringLiteral("id")).toString();
        const QString url = o.value(QStringLiteral("clip_url")).toString();
        if (id.isEmpty() || url.isEmpty()) continue;
        QVariantMap m;
        m[QStringLiteral("id")] = id;
        m[QStringLiteral("kind")] = QStringLiteral("clip");
        m[QStringLiteral("title")] = o.value(QStringLiteral("title")).toString();
        m[QStringLiteral("url")] = url;
        m[QStringLiteral("thumb")] = o.value(QStringLiteral("thumbnail_url")).toString();
        m[QStringLiteral("seconds")] = o.value(QStringLiteral("duration")).toDouble();
        m[QStringLiteral("date")] = o.value(QStringLiteral("created_at")).toString();
        m[QStringLiteral("viewers")] = o.value(QStringLiteral("views")).toInt();
        m[QStringLiteral("mature")] = o.value(QStringLiteral("is_mature")).toBool();
        m[QStringLiteral("estimate")] = double(o.value(QStringLiteral("duration")).toDouble() * 1.5e6 / 8.0);
        out->append(m);
    }
    return true;
}

void KickClient::loadPastStreams()
{
    const QString slug = m_channel.value(QStringLiteral("slug")).toString();
    if (slug.isEmpty() || !ready()) return;
    const quint64 gen = ++m_libraryGen;
    m_libraryState = QStringLiteral("loading");
    emit libraryChanged();
    begin();
    m_net->get(kApp, QUrl(QStringLiteral("https://kick.com/api/v2/channels/") + slug + QStringLiteral("/videos")), [this, gen](NetReply r) {
        end();
        if (gen != m_libraryGen) return;
        QVariantList l;
        if (!r.error.isEmpty()) { m_libraryState = r.error; }
        else if (!parsePastStreams(r.body, &l)) { m_libraryState = QStringLiteral("kick answered, but not in a form this version understands"); }
        else { m_past = l; m_libraryState = l.isEmpty() ? QStringLiteral("no finished streams are kept for this channel") : QString(); }
        emit libraryChanged();
    });
}

void KickClient::loadClips()
{
    const QString slug = m_channel.value(QStringLiteral("slug")).toString();
    if (slug.isEmpty() || !ready()) return;
    const quint64 gen = ++m_libraryGen;
    m_libraryState = QStringLiteral("loading");
    emit libraryChanged();
    begin();
    m_net->get(kApp, QUrl(QStringLiteral("https://kick.com/api/v2/channels/") + slug + QStringLiteral("/clips")), [this, gen](NetReply r) {
        end();
        if (gen != m_libraryGen) return;
        QVariantList l;
        if (!r.error.isEmpty()) { m_libraryState = r.error; }
        else if (!parseClips(r.body, &l)) { m_libraryState = QStringLiteral("kick answered, but not in a form this version understands"); }
        else { m_clips = l; m_libraryState = l.isEmpty() ? QStringLiteral("this channel has no clips") : QString(); }
        emit libraryChanged();
    });
}

// ---- adding many channels at once: paste names or links -------------------------------------------------------
// No sign-in and no account: the person pastes channel names or kick.com links (one per line, or separated by spaces or commas) and they go
// into the local follow list, or into a channel playlist. Nothing is sent to Kick for this.

QVariantMap KickClient::importNames(const QString &text, const QString &playlistId)
{
    QStringList tokens = text.split(QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts);
    int added = 0, already = 0, blocked = 0, skipped = 0, seen = 0;
    QStringList done;
    const bool toPlaylist = !playlistId.isEmpty();
    int pl = -1;
    if (toPlaylist) for (int i = 0; i < m_playlists.size(); ++i) if (m_playlists.at(i).id == playlistId) pl = i;
    if (toPlaylist && (pl < 0 || m_playlists.at(pl).kind != QLatin1String("channels"))) {
        m_importMessage = QStringLiteral("choose a playlist of channels (or the follow list) to add them to");
        emit importChanged();
        return QVariantMap{{QStringLiteral("added"), 0}, {QStringLiteral("message"), m_importMessage}};
    }
    for (const QString &t : tokens) {
        if (++seen > 500) { ++skipped; continue; }                                  // one paste is at most 500 words
        const QString s = normalize(t);
        const bool looksLikeName = !s.isEmpty() && s.size() <= 25 && QRegularExpression(QStringLiteral("^[a-z0-9_-]+$")).match(s).hasMatch();
        if (!looksLikeName) { ++skipped; continue; }
        if (done.contains(s)) { ++already; continue; }
        done << s;
        if (m_blocked.contains(s)) { ++blocked; continue; }
        if (toPlaylist) {
            if (isInPlaylist(playlistId, s)) { ++already; continue; }
            if (addToPlaylist(playlistId, QVariantMap{{QStringLiteral("slug"), s}})) ++added; else ++skipped;
        } else {
            if (m_following.contains(s)) { ++already; continue; }
            m_following.append(s); ++added;
        }
    }
    m_importMessage = tokens.isEmpty() ? QStringLiteral("paste some channel names or kick.com links first")
        : QStringLiteral("added %1 (%2 were already there%3%4). they are kept on this device.")
              .arg(added).arg(already)
              .arg(blocked > 0 ? QStringLiteral(", %1 skipped because you blocked them").arg(blocked) : QString())
              .arg(skipped > 0 ? QStringLiteral(", %1 words were not channel names").arg(skipped) : QString());
    emit importChanged();
    if (added > 0 && !toPlaylist) { emit listsChanged(); emit listsEdited(); }
    return QVariantMap{{QStringLiteral("added"), added}, {QStringLiteral("already"), already}, {QStringLiteral("blocked"), blocked}, {QStringLiteral("skipped"), skipped}, {QStringLiteral("message"), m_importMessage}};
}

// ---- your own category chips -------------------------------------------------------------------------------

bool KickClient::pinCategory(int id, const QString &nameIn)
{
    const QString name = nameIn.trimmed().left(60);
    if (id <= 0 || name.isEmpty()) return false;
    if (!m_customCategories) { m_pinned = m_categories; m_customCategories = true; }       // the first change starts from what is shown now
    for (const QVariant &v : m_pinned) if (v.toMap().value(QStringLiteral("id")).toInt() == id) return false;
    if (m_pinned.size() >= kMaxPinned) return false;
    m_pinned.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("slug"), QString()}});
    emit categoriesChanged();
    emit listsEdited();
    return true;
}

void KickClient::unpinCategory(int id)
{
    if (!m_customCategories) { m_pinned = m_categories; m_customCategories = true; }
    for (int i = 0; i < m_pinned.size(); ++i) {
        if (m_pinned[i].toMap().value(QStringLiteral("id")).toInt() != id) continue;
        m_pinned.removeAt(i);
        if (m_categoryId == id) setCategoryId(0);                                          // the list was filtered by it: back to everything
        emit categoriesChanged();
        emit listsEdited();
        return;
    }
    emit categoriesChanged();                                                              // (the copy above may have changed what is shown)
    emit listsEdited();
}

void KickClient::movePinnedCategory(int id, int by)
{
    if (by == 0) return;
    if (!m_customCategories) { m_pinned = m_categories; m_customCategories = true; }       // the first change starts from what is shown now
    for (int i = 0; i < m_pinned.size(); ++i) {
        if (m_pinned[i].toMap().value(QStringLiteral("id")).toInt() != id) continue;
        const int to = qBound(0, i + (by < 0 ? -1 : 1), int(m_pinned.size()) - 1);
        if (to == i) return;
        m_pinned.move(i, to);
        emit categoriesChanged();
        emit listsEdited();
        return;
    }
}

void KickClient::resetCategories()
{
    if (!m_customCategories && m_pinned.isEmpty()) return;
    m_customCategories = false;
    m_pinned.clear();
    emit categoriesChanged();
    emit listsEdited();
}

void KickClient::searchCategories(const QString &textIn)
{
    const QString word = textIn.trimmed();
    const quint64 gen = ++m_categorySearchGen;
    if (word.size() < 2 || !ready()) { m_categoryResults.clear(); m_categorySearching = false; emit categoryResultsChanged(); return; }
    m_categorySearching = true;
    emit categoryResultsChanged();
    QUrl url(QStringLiteral("https://kick.com/api/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("searched_word"), word);
    url.setQuery(q);
    m_net->get(kApp, url, [this, gen](NetReply r) {
        if (gen != m_categorySearchGen) return;
        m_categorySearching = false;
        QVariantList cats;
        if (r.error.isEmpty() && parseSearchCategories(r.body, &cats)) m_categoryResults = cats; else m_categoryResults.clear();
        emit categoryResultsChanged();
    });
}

} // namespace hop

bool hop::KickClient::streamAllowed(const QString &url) const
{
    return hop::AppNet::streamUrlAllowed(QStringLiteral("kick"), QUrl(url));
}
