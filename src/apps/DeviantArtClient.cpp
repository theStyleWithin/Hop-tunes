// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps/DeviantArtClient.h"

#include <QJsonArray>
#include <QPointer>
#include <QRegularExpression>
#include <QUrl>
#include <QXmlStreamReader>

namespace hop {

namespace {
constexpr const char *kFeed = "https://backend.deviantart.com/rss.xml";

bool deviantArtPage(const QUrl &u)
{
    const QString h = u.host().toLower();
    return u.scheme() == QLatin1String("https") && u.userInfo().isEmpty() && (u.port() == -1 || u.port() == 443)
        && (h == QLatin1String("deviantart.com") || h.endsWith(QLatin1String(".deviantart.com")));
}

QString lower(const QString &s) { return s.trimmed().toLower(); }
bool containsNoCase(const QStringList &l, const QString &s) { return l.contains(s, Qt::CaseInsensitive); }
}

QString DeviantArtClient::clean(const QString &s) { return s.simplified().left(300); }

QString DeviantArtClient::licenceName(const QString &url)
{
    static const QRegularExpression re(QStringLiteral("^https?://creativecommons\\.org/(licenses|publicdomain)/([a-z-]+)/"));
    const auto m = re.match(url);
    if (!m.hasMatch()) return QString();
    return m.captured(1) == QLatin1String("publicdomain") ? QStringLiteral("public domain") : QStringLiteral("CC ") + m.captured(2).toUpper();
}

QVector<DeviantArtClient::Work> DeviantArtClient::parseRss(const QByteArray &xml, QString *next)
{
    QVector<Work> out;
    if (next) next->clear();
    QXmlStreamReader r(xml);
    bool inItem = false;
    Work w;
    int bestThumbW = 0;
    bool haveArtist = false;
    static const QRegularExpression tags(QStringLiteral("<[^>]*>"));
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            const QString n = r.qualifiedName().toString();
            if (!inItem) {
                if (n == QLatin1String("atom:link") && r.attributes().value(QStringLiteral("rel")) == QLatin1String("next") && next) {
                    const QUrl u(r.attributes().value(QStringLiteral("href")).toString());
                    if (u.scheme() == QLatin1String("https") && u.host().toLower() == QLatin1String("backend.deviantart.com") && u.path() == QLatin1String("/rss.xml"))
                        *next = u.toString(QUrl::FullyEncoded);
                } else if (n == QLatin1String("item")) { inItem = true; w = Work(); bestThumbW = 0; haveArtist = false; }
                continue;
            }
            if (n == QLatin1String("title")) w.title = clean(r.readElementText());
            else if (n == QLatin1String("link")) w.page = r.readElementText().trimmed();
            else if (n == QLatin1String("guid")) w.id = clean(r.readElementText());
            else if (n == QLatin1String("pubDate")) w.published = clean(r.readElementText());
            else if (n == QLatin1String("media:rating")) w.rating = lower(r.readElementText());
            else if (n == QLatin1String("media:copyright")) w.copyright = clean(r.readElementText());
            else if (n == QLatin1String("creativeCommons:license")) w.license = licenceName(r.readElementText().trimmed());
            else if (n == QLatin1String("media:description")) w.description = r.readElementText().remove(tags).simplified().left(600);
            else if (n == QLatin1String("media:credit")) {
                const QString who = clean(r.readElementText());
                if (!haveArtist && !who.isEmpty() && !who.startsWith(QLatin1String("http"))) { w.artist = who; haveArtist = true; }
            } else if (n == QLatin1String("media:content")) {
                const QUrl u(r.attributes().value(QStringLiteral("url")).toString());
                if (AppNet::urlAllowed(QLatin1String(kApp), u)) w.image = u.toString(QUrl::FullyEncoded);
            } else if (n == QLatin1String("media:thumbnail")) {
                const QUrl u(r.attributes().value(QStringLiteral("url")).toString());
                const int wd = r.attributes().value(QStringLiteral("width")).toInt();
                // the thumbnail nearest 300 pixels wide, preferring the larger when two are equally near
                if (AppNet::urlAllowed(QLatin1String(kApp), u) && (w.thumb.isEmpty() || qAbs(wd - 300) < qAbs(bestThumbW - 300) || (qAbs(wd - 300) == qAbs(bestThumbW - 300) && wd > bestThumbW))) {
                    w.thumb = u.toString(QUrl::FullyEncoded); bestThumbW = wd;
                }
            }
        } else if (r.isEndElement() && inItem && r.qualifiedName() == QLatin1String("item")) {
            inItem = false;
            if (w.id.isEmpty()) w.id = w.page;
            if (w.thumb.isEmpty()) w.thumb = w.image;
            if (w.image.isEmpty()) w.image = w.thumb;
            w.mature = w.rating != QLatin1String("nonadult");       // anything the feed does not call nonadult is treated as 18+
            // only works that have a picture and a page on deviantart.com itself (the page is what "open on deviantart.com" opens)
            if (!w.image.isEmpty() && !w.id.isEmpty() && deviantArtPage(QUrl(w.page))) out.append(w);
            if (out.size() >= 200) break;
        }
    }
    return out;
}

DeviantArtClient::DeviantArtClient(AppNet *net, QObject *parent) : QObject(parent), m_net(net) {}

QString DeviantArtClient::feedUrl(const QString &q)
{
    return QString::fromLatin1(kFeed) + QStringLiteral("?type=deviation&q=") + QString::fromLatin1(QUrl::toPercentEncoding(q));
}

void DeviantArtClient::setMessage(const QString &m)
{
    if (m == m_message) return;
    m_message = m;
    emit changed();
}

void DeviantArtClient::open()
{
    if (m_open) return;
    m_open = true;
    m_net->open(kApp);
    emit openChanged();
    if (!m_net->allowed(kApp)) { setMessage(QStringLiteral("this app is not allowed to use the internet (settings > apps)")); return; }
    search(QString());
}

void DeviantArtClient::close()
{
    if (!m_open) return;
    m_open = false;
    ++m_gen;
    m_net->close(kApp);
    m_loading = false; m_next.clear(); m_all.clear(); m_visible.clear(); m_hidden = 0; m_query.clear(); m_message.clear();
    emit openChanged(); emit changed(); emit worksChanged();
}

void DeviantArtClient::search(const QString &text)
{
    if (!m_open) return;
    const QString t = text.simplified().left(80);
    QString q = t;
    if (t.isEmpty()) q = QStringLiteral("boost:popular");
    else if (t.startsWith(QLatin1Char('@'))) {
        const QString who = t.mid(1).trimmed();
        static const QRegularExpression name(QStringLiteral("^[A-Za-z0-9_-]{1,40}$"));      // (an artist's name: nothing that could change the query)
        if (!name.match(who).hasMatch()) { setMessage(QStringLiteral("that is not an artist's name")); return; }
        if (isBlockedArtist(who)) { m_query = t; m_all.clear(); m_next.clear(); rebuild(); setMessage(QStringLiteral("you blocked this artist (unblock them in your blocked list to see their art)")); return; }
        q = QStringLiteral("gallery:") + who;
    } else {
        for (const QString &word : t.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts))
            if (containsNoCase(m_blockedTags, word)) { setMessage(QStringLiteral("\"%1\" is one of your blocked words, so it is not searched").arg(word)); return; }
    }
    m_query = t;
    m_all.clear(); m_next.clear();
    rebuild();
    fetch(feedUrl(q), false);
}

void DeviantArtClient::loadMore()
{
    if (!m_open || m_loading || m_next.isEmpty()) return;
    fetch(m_next, true);
}

void DeviantArtClient::fetch(const QString &url, bool append)
{
    const quint64 gen = ++m_gen;
    m_loading = true;
    setMessage(QString());
    emit changed();
    QPointer<DeviantArtClient> self(this);
    NetLimits lim;
    lim.maxBody = 2 * 1024 * 1024;
    m_net->get(kApp, QUrl(url), [self, gen, append](NetReply r) {
        if (!self || self->m_gen != gen || !self->m_open) return;
        self->m_loading = false;
        if (!r.error.isEmpty()) { self->setMessage(QStringLiteral("could not load: ") + r.error); emit self->changed(); emit self->worksChanged(); return; }
        QString next;
        const QVector<Work> got = parseRss(r.body, &next);
        if (!append) self->m_all.clear();
        for (const Work &w : got) {
            bool dup = false;
            for (const Work &o : std::as_const(self->m_all)) if (o.id == w.id) { dup = true; break; }
            if (!dup && self->m_all.size() < kMaxWorksKept) self->m_all.append(w);
        }
        self->m_next = got.isEmpty() || self->m_all.size() >= kMaxWorksKept ? QString() : next;
        self->rebuild();
        if (self->m_all.isEmpty()) self->setMessage(QStringLiteral("nothing found for this search"));
        emit self->changed();
    }, lim);
}

bool DeviantArtClient::hides(const Work &w, QString *why) const
{
    auto no = [&](const QString &m) { if (why) *why = m; return true; };
    if (w.mature && !m_showMature) return no(QStringLiteral("18+"));
    if (containsNoCase(m_blockedArtists, w.artist)) return no(QStringLiteral("blocked artist"));
    const QString hay = w.title + QLatin1Char(' ') + w.description;
    for (const QString &word : m_blockedTags) {
        const QRegularExpression re(QStringLiteral("(?<![\\p{L}\\p{N}])") + QRegularExpression::escape(word) + QStringLiteral("(?![\\p{L}\\p{N}])"), QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
        if (re.match(hay).hasMatch()) return no(QStringLiteral("blocked word"));
    }
    return false;
}

void DeviantArtClient::rebuild()
{
    m_visible.clear(); m_hidden = 0;
    for (const Work &w : std::as_const(m_all)) {
        if (hides(w)) { ++m_hidden; continue; }
        m_visible.append(QVariantMap{{QStringLiteral("id"), w.id}, {QStringLiteral("title"), w.title}, {QStringLiteral("artist"), w.artist}, {QStringLiteral("page"), w.page},
                                     {QStringLiteral("image"), w.image}, {QStringLiteral("thumb"), w.thumb}, {QStringLiteral("rating"), w.rating}, {QStringLiteral("mature"), w.mature},
                                     {QStringLiteral("license"), w.license}, {QStringLiteral("copyright"), w.copyright}, {QStringLiteral("description"), w.description},
                                     {QStringLiteral("published"), w.published}});
    }
    emit worksChanged();
}

void DeviantArtClient::setShowMature(bool on)
{
    if (on == m_showMature) return;
    m_showMature = on;
    rebuild();
    emit filtersChanged(); emit listsEdited();
}

void DeviantArtClient::blockArtist(const QString &name)
{
    const QString n = clean(name);
    if (n.isEmpty() || containsNoCase(m_blockedArtists, n) || m_blockedArtists.size() >= kMaxBlocked) return;
    m_blockedArtists.append(n);
    rebuild();
    emit filtersChanged(); emit listsEdited();
}

void DeviantArtClient::unblockArtist(const QString &name)
{
    for (int i = 0; i < m_blockedArtists.size(); ++i)
        if (m_blockedArtists.at(i).compare(name.trimmed(), Qt::CaseInsensitive) == 0) { m_blockedArtists.removeAt(i); rebuild(); emit filtersChanged(); emit listsEdited(); return; }
}

bool DeviantArtClient::isBlockedArtist(const QString &name) const { return containsNoCase(m_blockedArtists, name.trimmed()); }

bool DeviantArtClient::blockTag(const QString &word)
{
    const QString w = clean(word).toLower();
    if (w.isEmpty() || w.size() > 40 || containsNoCase(m_blockedTags, w) || m_blockedTags.size() >= kMaxBlocked) return false;
    m_blockedTags.append(w);
    rebuild();
    emit filtersChanged(); emit listsEdited();
    return true;
}

void DeviantArtClient::unblockTag(const QString &word)
{
    for (int i = 0; i < m_blockedTags.size(); ++i)
        if (m_blockedTags.at(i).compare(word.trimmed(), Qt::CaseInsensitive) == 0) { m_blockedTags.removeAt(i); rebuild(); emit filtersChanged(); emit listsEdited(); return; }
}

void DeviantArtClient::toggleFavouriteArtist(const QString &name)
{
    const QString n = clean(name);
    if (n.isEmpty()) return;
    for (int i = 0; i < m_favArtists.size(); ++i)
        if (m_favArtists.at(i).compare(n, Qt::CaseInsensitive) == 0) { m_favArtists.removeAt(i); emit listsChanged(); emit listsEdited(); return; }
    if (m_favArtists.size() >= kMaxBlocked) return;
    m_favArtists.append(n);
    emit listsChanged(); emit listsEdited();
}

bool DeviantArtClient::isFavouriteArtist(const QString &name) const { return containsNoCase(m_favArtists, name.trimmed()); }

void DeviantArtClient::toggleFavouriteWork(const QVariantMap &work)
{
    const QString id = work.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) return;
    for (int i = 0; i < m_favWorks.size(); ++i)
        if (m_favWorks.at(i).toMap().value(QStringLiteral("id")).toString() == id) { m_favWorks.removeAt(i); emit listsChanged(); emit listsEdited(); return; }
    if (m_favWorks.size() >= kMaxFavouriteWorks || !deviantArtPage(QUrl(work.value(QStringLiteral("page")).toString()))) return;
    m_favWorks.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("title"), clean(work.value(QStringLiteral("title")).toString())},
                                  {QStringLiteral("artist"), clean(work.value(QStringLiteral("artist")).toString())}, {QStringLiteral("page"), work.value(QStringLiteral("page")).toString()},
                                  {QStringLiteral("thumb"), work.value(QStringLiteral("thumb")).toString()}});
    emit listsChanged(); emit listsEdited();
}

bool DeviantArtClient::isFavouriteWork(const QString &id) const
{
    for (const QVariant &v : m_favWorks) if (v.toMap().value(QStringLiteral("id")).toString() == id) return true;
    return false;
}

QJsonObject DeviantArtClient::listsJson() const
{
    QJsonArray works;
    for (const QVariant &v : m_favWorks) works.append(QJsonObject::fromVariantMap(v.toMap()));
    QJsonObject o{{QStringLiteral("showMature"), m_showMature}, {QStringLiteral("blockedArtists"), QJsonArray::fromStringList(m_blockedArtists)},
                  {QStringLiteral("blockedTags"), QJsonArray::fromStringList(m_blockedTags)}, {QStringLiteral("favouriteArtists"), QJsonArray::fromStringList(m_favArtists)},
                  {QStringLiteral("favouriteWorks"), works}};
    return o;
}

void DeviantArtClient::setListsJson(const QJsonObject &o)
{
    auto strings = [](const QJsonValue &v) { QStringList l; for (const QJsonValue &x : v.toArray()) { const QString s = clean(x.toString()); if (!s.isEmpty() && l.size() < kMaxBlocked && !containsNoCase(l, s)) l << s; } return l; };
    m_showMature = o.value(QStringLiteral("showMature")).toBool(false);
    m_blockedArtists = strings(o.value(QStringLiteral("blockedArtists")));
    m_blockedTags = strings(o.value(QStringLiteral("blockedTags")));
    m_favArtists = strings(o.value(QStringLiteral("favouriteArtists")));
    m_favWorks.clear();
    for (const QJsonValue &v : o.value(QStringLiteral("favouriteWorks")).toArray()) {
        const QJsonObject w = v.toObject();
        if (m_favWorks.size() >= kMaxFavouriteWorks || w.value(QStringLiteral("id")).toString().isEmpty() || !deviantArtPage(QUrl(w.value(QStringLiteral("page")).toString()))) continue;
        m_favWorks.append(QVariantMap{{QStringLiteral("id"), clean(w.value(QStringLiteral("id")).toString())}, {QStringLiteral("title"), clean(w.value(QStringLiteral("title")).toString())},
                                      {QStringLiteral("artist"), clean(w.value(QStringLiteral("artist")).toString())}, {QStringLiteral("page"), w.value(QStringLiteral("page")).toString()},
                                      {QStringLiteral("thumb"), w.value(QStringLiteral("thumb")).toString()}});
    }
    rebuild();
    emit filtersChanged(); emit listsChanged();
}

} // namespace hop
