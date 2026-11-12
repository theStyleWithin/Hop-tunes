// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps/KickClient.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QTimer>
#include <QUuid>
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

namespace {
// a stream whose language Kick does not say is kept (nothing to go on); one that says something other than English is not
bool isEnglish(const QString &lang) { return lang.isEmpty() || lang.compare(QLatin1String("en"), Qt::CaseInsensitive) == 0 || lang.compare(QLatin1String("english"), Qt::CaseInsensitive) == 0; }
} // namespace

void KickClient::applyFilters()
{
    QVariantList shown;
    const QVariantList &src = m_query.isEmpty() ? m_raw : m_searchChannels;
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
    if (m_query.isEmpty()) { ++m_listGen; m_searchChannels.clear(); refresh(); return; }
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
    QString m = mode == QLatin1String("pretend") ? mode : (mode == QLatin1String("kick") && m_signedIn ? mode : QStringLiteral("off"));
    if (m == m_chatMode) return;
    m_chatMode = m;
    emit chatModeChanged();
    emit listsEdited();                       // worth saving with the profile
}

QJsonObject KickClient::listsJson() const
{
    return QJsonObject{{QStringLiteral("following"), QJsonArray::fromStringList(m_following)}, {QStringLiteral("blocked"), QJsonArray::fromStringList(m_blocked)},
                       {QStringLiteral("chatMode"), m_chatMode}, {QStringLiteral("playlists"), playlistsJson()},
                       {QStringLiteral("englishOnly"), m_englishOnly}, {QStringLiteral("askBeforeOpening"), m_askBeforeOpening}, {QStringLiteral("pinnedCategories"), pinnedJson()},
                       {QStringLiteral("account"), m_signedIn ? QJsonObject{{QStringLiteral("token"), m_token}, {QStringLiteral("name"), m_accountName}} : QJsonObject{}}};
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
    ++m_signInGen;                                                // a sign-in in progress belongs to the profile it started in
    m_signInState.clear(); m_signInCode.clear(); m_signInUrl.clear(); m_importMessage.clear(); m_importing = false;
    {
        const QJsonObject acc = o.value(QStringLiteral("account")).toObject();
        const QString token = acc.value(QStringLiteral("token")).toString();
        m_signedIn = !token.isEmpty() && token.size() < 4000;
        m_token = m_signedIn ? token : QString();
        m_accountName = m_signedIn ? acc.value(QStringLiteral("name")).toString().left(60) : QString();
        emit signedInChanged();
        emit signInChanged();
    }
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
    m_chatMode = cm == QLatin1String("pretend") || (cm == QLatin1String("kick") && m_signedIn) ? cm : QStringLiteral("off");
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
            if (r.error.isEmpty() && parseChannel(r.body, &ch) && (m_following.contains(slug) || channelInPlaylists(slug))) {
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

// ---- optional sign-in (Kick's TV-link page) and importing the follow list -------------------------------------

namespace {
NetLimits accountLimits(const QByteArray &method, const QByteArray &body, const QString &token = QString())
{
    auto h = [](const char *k, const QByteArray &v) { return qMakePair(QByteArray(k), v); };
    NetLimits l;
    l.method = method;
    l.body = body;
    l.headers.append(h("Accept", "application/json"));
    if (!body.isEmpty()) l.headers.append(h("Content-Type", "application/json"));
    l.headers.append(h("Origin", "https://kick.com"));             // what kick.com's own pages send; only ever with these three addresses
    l.headers.append(h("Referer", "https://kick.com/tv/login"));
    if (!token.isEmpty()) l.headers.append(h("Authorization", "Bearer " + token.toUtf8()));
    return l;
}
} // namespace

void KickClient::setSignInState(const QString &state, const QString &message)
{
    m_signInState = state;
    m_signInMessage = message;
    emit signInChanged();
}

void KickClient::startSignIn()
{
    if (m_signedIn || !ready()) return;
    const quint64 gen = ++m_signInGen;
    m_signInUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_signInCode = QStringLiteral("%1").arg(QRandomGenerator::system()->bounded(1000000), 6, 10, QLatin1Char('0'));
    m_signInUrl = QStringLiteral("https://kick.com/tv/login?uuid=") + m_signInUuid + QStringLiteral("&code=") + m_signInCode;
    m_signInPolls = 0;
    m_signInFailures = 0;
    setSignInState(QStringLiteral("waiting"), QStringLiteral("waiting for you to approve it on kick.com…"));
    QTimer::singleShot(m_signInPollMs, this, [this, gen] { pollSignIn(gen); });
}

void KickClient::cancelSignIn()
{
    ++m_signInGen;
    m_signInCode.clear(); m_signInUrl.clear(); m_signInUuid.clear();
    setSignInState(QString(), QString());
}

void KickClient::pollSignIn(quint64 gen)
{
    if (gen != m_signInGen || !m_open) return;
    if (++m_signInPolls > 200) {                                   // about ten minutes
        m_signInCode.clear(); m_signInUrl.clear();
        setSignInState(QStringLiteral("error"), QStringLiteral("the code was not approved in time: start again"));
        return;
    }
    const QByteArray body = QJsonDocument(QJsonObject{{QStringLiteral("key"), m_signInCode}}).toJson(QJsonDocument::Compact);
    m_net->get(kApp, QUrl(QStringLiteral("https://kick.com/api/tv/link/authenticate/") + m_signInUuid), [this, gen](NetReply r) {
        if (gen != m_signInGen) return;
        if (r.status == 200 && r.error.isEmpty()) {
            const QJsonObject o = QJsonDocument::fromJson(r.body).object();
            QString token = o.value(QStringLiteral("token")).toString();
            if (token.isEmpty()) token = o.value(QStringLiteral("data")).toObject().value(QStringLiteral("token")).toString();
            if (token.isEmpty()) { setSignInState(QStringLiteral("error"), QStringLiteral("kick answered, but not with a sign-in this version understands")); return; }
            finishSignIn(token);
            return;
        }
        // 403 / 404 / 422 = not approved yet; a few network errors in a row are tolerated, then it says so
        const bool waiting = r.status == 403 || r.status == 404 || r.status == 422;
        if (waiting) m_signInFailures = 0;
        else if (++m_signInFailures >= 5) { setSignInState(QStringLiteral("error"), r.error.isEmpty() ? QStringLiteral("kick did not answer as expected") : r.error); return; }
        QTimer::singleShot(m_signInPollMs, this, [this, gen] { pollSignIn(gen); });
    }, accountLimits("POST", body));
}

void KickClient::finishSignIn(const QString &token)
{
    const quint64 gen = m_signInGen;
    m_token = token;
    m_signedIn = true;
    m_accountName = QStringLiteral("your kick account");
    m_signInCode.clear(); m_signInUrl.clear(); m_signInUuid.clear();
    emit signedInChanged();
    setSignInState(QString(), QString());
    emit listsEdited();                                            // the token is saved with the profile
    // who it is, for the screen (not needed for anything else)
    m_net->get(kApp, QUrl(QStringLiteral("https://kick.com/api/v1/user")), [this, gen](NetReply r) {
        if (gen != m_signInGen || !m_signedIn || !r.error.isEmpty()) return;
        const QJsonObject o = QJsonDocument::fromJson(r.body).object();
        QString name = o.value(QStringLiteral("username")).toString();
        if (name.isEmpty()) name = o.value(QStringLiteral("user")).toObject().value(QStringLiteral("username")).toString();
        if (name.isEmpty()) return;
        m_accountName = name.left(60);
        emit signedInChanged();
        emit listsEdited();
    }, accountLimits("GET", QByteArray(), token));
}

void KickClient::signOut()
{
    ++m_signInGen;
    m_token.clear(); m_accountName.clear(); m_signInCode.clear(); m_signInUrl.clear(); m_signInUuid.clear(); m_importMessage.clear();
    m_importing = false;
    const bool was = m_signedIn;
    m_signedIn = false;
    if (m_chatMode == QLatin1String("kick")) { m_chatMode = QStringLiteral("off"); emit chatModeChanged(); }
    setSignInState(QString(), QString());
    if (was) { emit signedInChanged(); emit listsEdited(); }
}

void KickClient::importFollows()
{
    if (!m_signedIn || m_token.isEmpty() || m_importing || !ready()) return;
    m_importing = true;
    m_importMessage = QStringLiteral("reading your follow list from kick.com…");
    emit signInChanged();
    importPage(m_signInGen, QString(), 0, {});
}

void KickClient::importPage(quint64 gen, QString cursor, int page, QStringList slugs)
{
    QUrl url(QStringLiteral("https://kick.com/api/v2/channels/followed"));
    if (!cursor.isEmpty()) url.setQuery(QStringLiteral("cursor=") + QString::fromLatin1(QUrl::toPercentEncoding(cursor)));
    m_net->get(kApp, url, [this, gen, cursor, page, slugs](NetReply r) mutable {
        if (gen != m_signInGen || !m_importing) return;
        auto stop = [this](const QString &msg) { m_importing = false; m_importMessage = msg; emit signInChanged(); };
        if (r.status == 401 || r.status == 403) {
            m_token.clear(); m_signedIn = false; m_accountName.clear();
            emit signedInChanged(); emit listsEdited();
            stop(QStringLiteral("your sign-in has expired: sign in again")); return;
        }
        if (!r.error.isEmpty()) { stop(QStringLiteral("could not read your follow list: ") + r.error); return; }
        const QJsonObject o = QJsonDocument::fromJson(r.body).object();
        QJsonArray arr = o.value(QStringLiteral("channels")).toArray();
        if (arr.isEmpty()) arr = o.value(QStringLiteral("data")).toArray();
        if (arr.isEmpty() && page == 0 && !o.contains(QStringLiteral("channels")) && !o.contains(QStringLiteral("data"))) {
            stop(QStringLiteral("kick answered, but not in a form this version understands")); return;
        }
        for (const QJsonValue &v : arr) {
            const QJsonObject c = v.toObject();
            QString s = c.value(QStringLiteral("channel_slug")).toString();
            if (s.isEmpty()) s = c.value(QStringLiteral("slug")).toString();
            if (s.isEmpty()) s = c.value(QStringLiteral("channel")).toObject().value(QStringLiteral("slug")).toString();
            if (s.isEmpty()) s = c.value(QStringLiteral("user")).toObject().value(QStringLiteral("username")).toString();
            s = normalize(s);
            if (!s.isEmpty() && !slugs.contains(s) && slugs.size() < 500) slugs << s;
        }
        QString next;
        const QJsonValue nc = o.contains(QStringLiteral("nextCursor")) ? o.value(QStringLiteral("nextCursor")) : (o.contains(QStringLiteral("next_cursor")) ? o.value(QStringLiteral("next_cursor")) : o.value(QStringLiteral("cursor")));
        next = nc.isString() ? nc.toString() : (nc.isDouble() ? QString::number(qint64(nc.toDouble())) : QString());
        if (!arr.isEmpty() && !next.isEmpty() && next != cursor && page < 20 && slugs.size() < 500) { importPage(gen, next, page + 1, slugs); return; }
        // done: add them to the local follow list (a blocked streamer stays blocked; ones already followed are not doubled)
        int added = 0, already = 0, blocked = 0;
        for (const QString &s : slugs) {
            if (m_blocked.contains(s)) { ++blocked; continue; }
            if (m_following.contains(s)) { ++already; continue; }
            m_following.append(s); ++added;
        }
        m_importing = false;
        m_importMessage = slugs.isEmpty() ? QStringLiteral("you do not follow anyone on kick yet")
                                          : QStringLiteral("imported %1 channels (%2 were already in your list%3). they are kept on this device now.")
                                                .arg(added).arg(already).arg(blocked > 0 ? QStringLiteral(", %1 skipped because you blocked them").arg(blocked) : QString());
        emit signInChanged();
        if (added > 0) { emit listsChanged(); emit listsEdited(); }
    }, accountLimits("GET", QByteArray(), m_token));
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
