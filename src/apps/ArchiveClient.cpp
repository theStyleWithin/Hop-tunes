// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps/ArchiveClient.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

#include <algorithm>

namespace hop {

namespace {
QString firstString(const QJsonValue &v)
{
    if (v.isString()) return v.toString();
    if (v.isArray() && !v.toArray().isEmpty()) return v.toArray().first().toString();
    if (v.isDouble()) return QString::number(v.toInt());
    return {};
}
QString plain(const QString &html)
{
    static const QRegularExpression tags(QStringLiteral("<[^>]*>"));
    QString s = html;
    s.replace(QStringLiteral("<br"), QStringLiteral("\n<br")).replace(QStringLiteral("</p>"), QStringLiteral("\n"));
    s.remove(tags);
    s.replace(QStringLiteral("&nbsp;"), QStringLiteral(" ")).replace(QStringLiteral("&lt;"), QStringLiteral("<")).replace(QStringLiteral("&gt;"), QStringLiteral(">"))
     .replace(QStringLiteral("&quot;"), QStringLiteral("\"")).replace(QStringLiteral("&#39;"), QStringLiteral("'")).replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    static const QRegularExpression blank(QStringLiteral("\\n{3,}"));
    s.replace(blank, QStringLiteral("\n\n"));
    return s.trimmed().left(1500);
}
QString licenceFor(const QString &url)
{
    static const QRegularExpression re(QStringLiteral("^https?://creativecommons\\.org/(licenses|publicdomain)/([a-z0-9-]+)/"));
    const auto m = re.match(url);
    if (!m.hasMatch()) return {};
    return m.captured(1) == QLatin1String("publicdomain") ? QStringLiteral("public domain") : QStringLiteral("CC ") + m.captured(2).toUpper();
}
bool wantedKind(const QString &k) { return k == QLatin1String("audio") || k == QLatin1String("movies") || k == QLatin1String("texts"); }
}

bool ArchiveClient::validId(const QString &id)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,120}$"));
    return re.match(id).hasMatch();
}

QString ArchiveClient::clean(const QString &s, int max) { return s.simplified().left(max); }

QString ArchiveClient::downloadUrl(const QString &id, const QString &file)
{
    return QStringLiteral("https://archive.org/download/") + id + QLatin1Char('/') + QString::fromLatin1(QUrl::toPercentEncoding(file, "/"));
}

QString ArchiveClient::searchUrl(const QString &text, const QString &kind, int page, bool curatorOnly)
{
    const QString types = kind == QLatin1String("audio") || kind == QLatin1String("movies") || kind == QLatin1String("texts")
        ? QStringLiteral("mediatype:") + kind : QStringLiteral("mediatype:(audio OR movies OR texts)");
    QString t = text;
    t.remove(QLatin1Char('"')).remove(QLatin1Char('\\'));
    t = clean(t, 100);
    QString q = t.isEmpty() ? types : QStringLiteral("(") + t + QStringLiteral(") AND ") + types;
    if (curatorOnly) q = QStringLiteral("collection:") + curatorCollection() + QStringLiteral(" AND ") + q;
    auto enc = [](const QString &x) { return QString::fromLatin1(QUrl::toPercentEncoding(x)); };
    return QStringLiteral("https://archive.org/advancedsearch.php?q=") + enc(q)
         + QStringLiteral("&fl%5B%5D=identifier&fl%5B%5D=title&fl%5B%5D=creator&fl%5B%5D=mediatype&fl%5B%5D=year&fl%5B%5D=downloads&sort%5B%5D=")
         + enc(curatorOnly ? QStringLiteral("addeddate desc") : QStringLiteral("downloads desc"))
         + QStringLiteral("&rows=") + QString::number(curatorOnly ? kCuratorRows : kPage) + QStringLiteral("&page=") + QString::number(qMax(1, page)) + QStringLiteral("&output=json");
}

QVariantList ArchiveClient::parseSearch(const QByteArray &json, int *total)
{
    QVariantList out;
    if (total) *total = 0;
    const QJsonObject resp = QJsonDocument::fromJson(json).object().value(QStringLiteral("response")).toObject();
    if (total) *total = resp.value(QStringLiteral("numFound")).toInt();
    QSet<QString> seen;
    for (const QJsonValue &v : resp.value(QStringLiteral("docs")).toArray()) {
        const QJsonObject d = v.toObject();
        const QString id = d.value(QStringLiteral("identifier")).toString();
        const QString kind = d.value(QStringLiteral("mediatype")).toString();
        if (!validId(id) || !wantedKind(kind) || seen.contains(id)) continue;   // only music, videos and books: never software or anything else
        seen.insert(id);
        out.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("title"), clean(firstString(d.value(QStringLiteral("title"))).isEmpty() ? id : firstString(d.value(QStringLiteral("title"))))},
                               {QStringLiteral("creator"), clean(firstString(d.value(QStringLiteral("creator"))))}, {QStringLiteral("kind"), kind},
                               {QStringLiteral("year"), clean(firstString(d.value(QStringLiteral("year"))), 10)}, {QStringLiteral("downloads"), d.value(QStringLiteral("downloads")).toInt()},
                               {QStringLiteral("thumb"), QStringLiteral("https://archive.org/services/img/") + id}, {QStringLiteral("featured"), false}});
    }
    return out;
}

QVariantMap ArchiveClient::parseDetail(const QByteArray &json, const QString &id)
{
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    const QJsonObject m = root.value(QStringLiteral("metadata")).toObject();
    const QString kind = m.value(QStringLiteral("mediatype")).toString();
    if (!validId(id) || m.isEmpty() || !wantedKind(kind)) return {};
    struct F { QString name, format, title, length; int track = 0; };
    QVector<F> files;
    for (const QJsonValue &v : root.value(QStringLiteral("files")).toArray()) {
        const QJsonObject o = v.toObject();
        F f; f.name = o.value(QStringLiteral("name")).toString(); f.format = o.value(QStringLiteral("format")).toString();
        f.title = clean(o.value(QStringLiteral("title")).toString()); f.length = clean(firstString(o.value(QStringLiteral("length"))), 20);
        f.track = firstString(o.value(QStringLiteral("track"))).section(QLatin1Char('/'), 0, 0).toInt();
        if (f.name.isEmpty() || f.name.startsWith(QLatin1Char('.')) || f.name.contains(QLatin1String(".."))) continue;
        files.append(f);
    }
    QVariantMap out{{QStringLiteral("id"), id}, {QStringLiteral("kind"), kind}, {QStringLiteral("title"), clean(firstString(m.value(QStringLiteral("title"))).isEmpty() ? id : firstString(m.value(QStringLiteral("title"))))},
                    {QStringLiteral("creator"), clean(firstString(m.value(QStringLiteral("creator"))))}, {QStringLiteral("year"), clean(firstString(m.value(QStringLiteral("year"))).isEmpty() ? firstString(m.value(QStringLiteral("date"))).left(4) : firstString(m.value(QStringLiteral("year"))), 10)},
                    {QStringLiteral("description"), plain(firstString(m.value(QStringLiteral("description"))))},
                    {QStringLiteral("license"), licenceFor(m.value(QStringLiteral("licenseurl")).toString())}, {QStringLiteral("rights"), clean(firstString(m.value(QStringLiteral("rights"))), 300)},
                    {QStringLiteral("page"), QStringLiteral("https://archive.org/details/") + id}, {QStringLiteral("thumb"), QStringLiteral("https://archive.org/services/img/") + id}};
    QVariantList tracks, videos;
    QString epub, text, pdf;
    if (kind == QLatin1String("audio")) {
        auto pick = [&](const QStringList &formats) {
            QVector<F> sel;
            for (const F &f : std::as_const(files)) for (const QString &fm : formats) if (f.format == fm) { sel.append(f); break; }
            return sel;
        };
        QVector<F> sel = pick({QStringLiteral("VBR MP3"), QStringLiteral("MP3"), QStringLiteral("128Kbps MP3"), QStringLiteral("64Kbps MP3")});
        if (sel.isEmpty()) sel = pick({QStringLiteral("Ogg Vorbis")});
        if (sel.isEmpty()) sel = pick({QStringLiteral("Flac"), QStringLiteral("24bit Flac")});
        std::stable_sort(sel.begin(), sel.end(), [](const F &a, const F &b) { if (a.track != b.track) return a.track < b.track; return a.name.compare(b.name, Qt::CaseInsensitive) < 0; });
        for (const F &f : std::as_const(sel))
            tracks.append(QVariantMap{{QStringLiteral("name"), f.name}, {QStringLiteral("title"), f.title.isEmpty() ? f.name : f.title}, {QStringLiteral("length"), f.length}, {QStringLiteral("url"), downloadUrl(id, f.name)}});
    } else if (kind == QLatin1String("movies")) {
        const QStringList order = {QStringLiteral("h.264"), QStringLiteral("MPEG4"), QStringLiteral("512Kb MPEG4"), QStringLiteral("h.264 IA"), QStringLiteral("Ogg Video"), QStringLiteral("WebM"), QStringLiteral("Matroska")};
        for (const QString &fm : order) {
            QVector<F> sel;
            for (const F &f : std::as_const(files)) if (f.format == fm) sel.append(f);
            if (sel.isEmpty()) continue;
            std::stable_sort(sel.begin(), sel.end(), [](const F &a, const F &b) { return a.name.compare(b.name, Qt::CaseInsensitive) < 0; });
            for (const F &f : std::as_const(sel)) videos.append(QVariantMap{{QStringLiteral("name"), f.name}, {QStringLiteral("title"), f.title.isEmpty() ? f.name : f.title}, {QStringLiteral("url"), downloadUrl(id, f.name)}});
            break;
        }
    } else {
        for (const F &f : std::as_const(files)) {
            if (epub.isEmpty() && f.format == QLatin1String("EPUB")) epub = downloadUrl(id, f.name);
            else if (text.isEmpty() && (f.format == QLatin1String("DjVuTXT") || f.name.endsWith(QLatin1String("_djvu.txt")))) text = downloadUrl(id, f.name);
            else if (pdf.isEmpty() && f.format.contains(QLatin1String("PDF"))) pdf = downloadUrl(id, f.name);
        }
    }
    out[QStringLiteral("tracks")] = tracks;
    out[QStringLiteral("videos")] = videos;
    out[QStringLiteral("epubUrl")] = epub;
    out[QStringLiteral("textUrl")] = text;
    out[QStringLiteral("pdfUrl")] = pdf;
    return out;
}

ArchiveClient::ArchiveClient(AppNet *net, QObject *parent) : QObject(parent), m_net(net) {}

void ArchiveClient::setMessage(const QString &m)
{
    if (m == m_message) return;
    m_message = m;
    emit changed();
}

void ArchiveClient::setKind(const QString &k)
{
    const QString v = wantedKind(k) ? k : QStringLiteral("all");
    if (v == m_kind) return;
    m_kind = v;
    emit changed();
    if (m_open) { if (m_curatorView) showCurator(); else search(m_query); }
}

void ArchiveClient::open()
{
    if (m_open) return;
    m_open = true;
    m_net->open(kApp);
    emit openChanged();
    if (!m_net->allowed(kApp)) { setMessage(QStringLiteral("this app is not allowed to use the internet (settings > apps)")); return; }
    search(QString());
}

void ArchiveClient::close()
{
    if (!m_open) return;
    m_open = false;
    m_curatorView = false;
    ++m_gen; ++m_detailGen; ++m_bookGen;
    m_net->close(kApp);
    m_loading = false; m_detailLoading = false; m_pending = 0;
    m_items.clear(); m_curatorBatch.clear(); m_generalBatch.clear(); m_detail.clear();
    m_featured = 0; m_total = 0; m_loaded = 0; m_page = 1; m_query.clear(); m_message.clear();
    closeBook();
    emit openChanged(); emit changed(); emit itemsChanged(); emit detailChanged();
}

void ArchiveClient::showCurator()
{
    if (!m_open) return;
    m_curatorView = true;
    m_query.clear();
    m_page = 1; m_total = 0; m_loaded = 0; m_featured = 0;
    m_items.clear(); m_curatorBatch.clear(); m_generalBatch.clear();
    emit itemsChanged();
    runSearch(false);
}

void ArchiveClient::search(const QString &text)
{
    if (!m_open) return;
    m_curatorView = false;
    m_query = clean(text, 100);
    m_page = 1; m_total = 0; m_loaded = 0; m_featured = 0;
    m_items.clear(); m_curatorBatch.clear(); m_generalBatch.clear();
    emit itemsChanged();
    runSearch(false);
}

void ArchiveClient::loadMore()
{
    if (!m_open || m_loading || m_loaded >= m_total) return;
    ++m_page;
    runSearch(true);
}

void ArchiveClient::runSearch(bool append)
{
    const quint64 gen = ++m_gen;
    m_loading = true;
    m_pending = (append || m_curatorView) ? 1 : 2;      // the curator's matches and the rest: both answers are needed before the list is shown
    setMessage(QString());
    emit changed();
    QPointer<ArchiveClient> self(this);
    NetLimits lim;
    lim.maxBody = 3 * 1024 * 1024;
    if (m_curatorView) {                // his page: only his picks, as many pages as there are
        m_net->get(kApp, QUrl(searchUrl(QString(), m_kind, m_page, true)), [self, gen](NetReply r) {
            if (!self || self->m_gen != gen || !self->m_open) return;
            if (!r.error.isEmpty()) { self->m_curatorBatch = QVariantList(); self->setMessage(QStringLiteral("could not load: ") + r.error); }
            else { int total = 0; self->m_curatorBatch = parseSearch(r.body, &total); self->m_total = total; }
            self->finishSearch();
        }, lim);
        return;
    }
    auto general = [self, gen, append](NetReply r) {
        if (!self || self->m_gen != gen || !self->m_open) return;
        if (!r.error.isEmpty()) { self->m_generalBatch = QVariantList(); self->setMessage(QStringLiteral("could not load: ") + r.error); }
        else {
            int total = 0;
            self->m_generalBatch = parseSearch(r.body, &total);
            self->m_total = total;
        }
        if (--self->m_pending == 0) self->finishSearch();
        (void)append;
    };
    m_net->get(kApp, QUrl(searchUrl(m_query, m_kind, m_page, false)), general, lim);
    if (!append) {
        m_net->get(kApp, QUrl(searchUrl(m_query, m_kind, 1, true)), [self, gen](NetReply r) {
            if (!self || self->m_gen != gen || !self->m_open) return;
            self->m_curatorBatch = r.error.isEmpty() ? parseSearch(r.body) : QVariantList();    // (if only this one fails, the rest still shows)
            if (--self->m_pending == 0) self->finishSearch();
        }, lim);
    }
}

void ArchiveClient::finishSearch()
{
    m_loading = false;
    if (m_curatorView) {                // his page: his picks only, all of them marked as his
        QSet<QString> seen;
        for (const QVariant &v : std::as_const(m_items)) seen.insert(v.toMap().value(QStringLiteral("id")).toString());
        QVariantList added;
        for (const QVariant &v : std::as_const(m_curatorBatch)) {
            QVariantMap m = v.toMap();
            if (seen.contains(m.value(QStringLiteral("id")).toString())) continue;
            seen.insert(m.value(QStringLiteral("id")).toString());
            m[QStringLiteral("featured")] = true;
            added.append(m);
        }
        if (m_page == 1) m_items = added; else m_items += added;
        m_featured = m_items.size();
        m_loaded = m_page * kCuratorRows;
        m_curatorBatch.clear();
        if (m_items.isEmpty() && m_message.isEmpty()) setMessage(QStringLiteral("nothing here"));
        emit changed();
        emit itemsChanged();
        return;
    }
    QSet<QString> have;
    for (const QVariant &v : std::as_const(m_items)) have.insert(v.toMap().value(QStringLiteral("id")).toString());
    QVariantList added;
    for (const QVariant &v : std::as_const(m_curatorBatch)) {
        QVariantMap m = v.toMap();
        if (have.contains(m.value(QStringLiteral("id")).toString())) continue;
        m[QStringLiteral("featured")] = true;
        have.insert(m.value(QStringLiteral("id")).toString());
        added.append(m);
    }
    m_featured += added.size();
    for (const QVariant &v : std::as_const(m_generalBatch)) {
        const QString id = v.toMap().value(QStringLiteral("id")).toString();
        if (have.contains(id)) continue;
        have.insert(id);
        added.append(v);
    }
    // the curator's matches go first; what is already listed (a "more" page) stays where it is
    if (m_page == 1) m_items = added;
    else m_items += added;
    m_loaded = m_page * kPage;
    m_curatorBatch.clear(); m_generalBatch.clear();
    if (m_items.isEmpty() && m_message.isEmpty()) setMessage(QStringLiteral("nothing found for this search"));
    emit changed();
    emit itemsChanged();
}

bool ArchiveClient::streamAllowed(const QString &url) const { return AppNet::streamUrlAllowed(QLatin1String(kApp), QUrl(url)); }

void ArchiveClient::openItem(const QString &id)
{
    if (!m_open || !validId(id)) return;
    const quint64 gen = ++m_detailGen;
    closeBook();
    m_detail.clear();
    m_detailLoading = true;
    emit detailChanged();
    QPointer<ArchiveClient> self(this);
    NetLimits lim; lim.maxBody = 6 * 1024 * 1024;
    m_net->get(kApp, QUrl(QStringLiteral("https://archive.org/metadata/") + id), [self, gen, id](NetReply r) {
        if (!self || self->m_detailGen != gen || !self->m_open) return;
        self->m_detailLoading = false;
        if (!r.error.isEmpty()) { self->setMessage(QStringLiteral("could not open it: ") + r.error); }
        else {
            self->m_detail = parseDetail(r.body, id);
            if (self->m_detail.isEmpty()) self->setMessage(QStringLiteral("that is not music, a video or a book hop-tunes can open"));
        }
        emit self->detailChanged();
    }, lim);
}

void ArchiveClient::closeItem()
{
    ++m_detailGen;
    closeBook();
    m_detail.clear();
    m_detailLoading = false;
    emit detailChanged();
}

void ArchiveClient::setBookError(const QString &e)
{
    m_bookLoading = false; m_bookReady = false; m_bookError = e;
    emit bookChanged();
}

void ArchiveClient::closeBook()
{
    ++m_bookGen;
    m_epub.close();
    m_epubOpen = false;
    m_pages.clear();
    m_bookDir.reset();                 // the temporary copy goes
    const bool was = m_bookReady || m_bookLoading || !m_bookError.isEmpty();
    m_bookLoading = false; m_bookReady = false; m_bookError.clear(); m_bookTitle.clear();
    if (was) emit bookChanged();
}

void ArchiveClient::bookFromText(const QString &text)
{
    m_pages.clear();
    QString page;
    const QStringList paras = text.split(QStringLiteral("\n\n"), Qt::SkipEmptyParts);
    for (const QString &p : paras) {
        if (!page.isEmpty() && page.size() + p.size() > 6000) { m_pages << page; page.clear(); }
        page += (page.isEmpty() ? QString() : QStringLiteral("\n\n")) + p.trimmed();
    }
    if (!page.trimmed().isEmpty()) m_pages << page;
}

void ArchiveClient::openBook()
{
    if (!m_open || m_detail.isEmpty() || m_bookLoading) return;
    const QString epub = m_detail.value(QStringLiteral("epubUrl")).toString(), text = m_detail.value(QStringLiteral("textUrl")).toString();
    closeBook();
    if (epub.isEmpty() && text.isEmpty()) { setBookError(QStringLiteral("this book has no EPUB or plain text to read here: open it on archive.org")); return; }
    const quint64 gen = ++m_bookGen;
    m_bookLoading = true; m_bookTitle = m_detail.value(QStringLiteral("title")).toString();
    emit bookChanged();
    QPointer<ArchiveClient> self(this);
    NetLimits lim; lim.maxBody = epub.isEmpty() ? kMaxTextBytes : kMaxBookBytes; lim.timeoutMs = 180000;
    m_net->get(kApp, QUrl(epub.isEmpty() ? text : epub), [self, gen, epubMode = !epub.isEmpty()](NetReply r) {
        if (!self || self->m_bookGen != gen || !self->m_open) return;
        if (!r.error.isEmpty()) { self->setBookError(QStringLiteral("could not load the book: ") + r.error); return; }
        if (epubMode) {
            self->m_bookDir = std::make_unique<QTemporaryDir>();
            if (!self->m_bookDir->isValid()) { self->setBookError(QStringLiteral("could not make room for the book")); return; }
            const QString path = self->m_bookDir->filePath(QStringLiteral("book.epub"));
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly) || f.write(r.body) != r.body.size()) { self->setBookError(QStringLiteral("could not make room for the book")); return; }
            f.close();
            f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            if (!self->m_epub.open(path, self->m_bookDir->filePath(QStringLiteral("img"))) || self->m_epub.chapterCount() == 0) {
                self->m_epub.close();
                self->setBookError(QStringLiteral("this EPUB could not be read"));
                return;
            }
            self->m_epubOpen = true;
            if (!self->m_epub.title().isEmpty()) self->m_bookTitle = self->m_epub.title();
        } else {
            self->bookFromText(QString::fromUtf8(r.body));
            if (self->m_pages.isEmpty()) { self->setBookError(QStringLiteral("the book has no text to read here: open it on archive.org")); return; }
        }
        self->m_bookLoading = false; self->m_bookReady = true;
        emit self->bookChanged();
    }, lim);
}

QString ArchiveClient::chapterTitle(int i)
{
    if (!m_bookReady) return {};
    if (m_epubOpen) return m_epub.chapterTitle(i);
    return i >= 0 && i < m_pages.size() ? QStringLiteral("page %1").arg(i + 1) : QString();
}

QString ArchiveClient::chapterHtml(int i)
{
    if (!m_bookReady) return {};
    if (m_epubOpen) return m_epub.chapterHtml(i);
    if (i < 0 || i >= m_pages.size()) return {};
    QString h = m_pages.at(i).toHtmlEscaped();
    h.replace(QStringLiteral("\n\n"), QStringLiteral("</p><p>")).replace(QLatin1Char('\n'), QStringLiteral(" "));
    return QStringLiteral("<p>") + h + QStringLiteral("</p>");
}

void ArchiveClient::toggleFavourite(const QVariantMap &item)
{
    const QString id = item.value(QStringLiteral("id")).toString();
    if (!validId(id)) return;
    for (int i = 0; i < m_favs.size(); ++i)
        if (m_favs.at(i).toMap().value(QStringLiteral("id")).toString() == id) { m_favs.removeAt(i); emit favouritesChanged(); emit listsEdited(); return; }
    const QString kind = item.value(QStringLiteral("kind")).toString();
    if (m_favs.size() >= kMaxFavourites || !wantedKind(kind)) return;
    m_favs.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("title"), clean(item.value(QStringLiteral("title")).toString())},
                              {QStringLiteral("creator"), clean(item.value(QStringLiteral("creator")).toString())}, {QStringLiteral("kind"), kind}});
    emit favouritesChanged(); emit listsEdited();
}

bool ArchiveClient::isFavourite(const QString &id) const
{
    for (const QVariant &v : m_favs) if (v.toMap().value(QStringLiteral("id")).toString() == id) return true;
    return false;
}

QJsonObject ArchiveClient::listsJson() const
{
    QJsonArray a;
    for (const QVariant &v : m_favs) a.append(QJsonObject::fromVariantMap(v.toMap()));
    return QJsonObject{{QStringLiteral("favourites"), a}};
}

void ArchiveClient::setListsJson(const QJsonObject &o)
{
    m_favs.clear();
    for (const QJsonValue &v : o.value(QStringLiteral("favourites")).toArray()) {
        const QJsonObject f = v.toObject();
        const QString id = f.value(QStringLiteral("id")).toString(), kind = f.value(QStringLiteral("kind")).toString();
        if (m_favs.size() >= kMaxFavourites || !validId(id) || !wantedKind(kind)) continue;
        m_favs.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("title"), clean(f.value(QStringLiteral("title")).toString())},
                                  {QStringLiteral("creator"), clean(f.value(QStringLiteral("creator")).toString())}, {QStringLiteral("kind"), kind}});
    }
    emit favouritesChanged();
}

} // namespace hop
