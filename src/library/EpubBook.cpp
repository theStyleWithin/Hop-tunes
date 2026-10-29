// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/EpubBook.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>
#include <QXmlStreamReader>

#include <zlib.h>

namespace hop {

namespace {

quint16 u16(const QByteArray &b, qsizetype o) { return quint16(uchar(b[o])) | quint16(uchar(b[o + 1])) << 8; }
quint32 u32(const QByteArray &b, qsizetype o) { return quint32(u16(b, o)) | quint32(u16(b, o + 2)) << 16; }

QByteArray inflateRaw(const QByteArray &in, quint32 expected)
{
    z_stream zs{};
    if (inflateInit2(&zs, -15) != Z_OK) return {};
    QByteArray out;
    out.resize(qsizetype(expected));
    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    zs.avail_in = uInt(in.size());
    zs.next_out = reinterpret_cast<Bytef *>(out.data());
    zs.avail_out = uInt(out.size());
    const int rc = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (rc != Z_STREAM_END || zs.total_out != expected) return {};
    return out;
}

QString plainText(QString html)
{
    html.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    html.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    html.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    html.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    html.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    return html.simplified();
}

} // namespace

void EpubBook::close()
{
    m_zip.clear();
    m_entries.clear();
    m_spine.clear();
    m_titles.clear();
    m_title.clear();
    m_author.clear();
    m_error.clear();
}

bool EpubBook::open(const QString &path, const QString &imageCacheDir)
{
    close();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { m_error = QStringLiteral("cannot open the file"); return false; }
    m_zip = f.readAll();
    m_cacheDir = imageCacheDir + QLatin1Char('/')
        + QString::fromLatin1(QCryptographicHash::hash((path + QLatin1Char('|') + QString::number(QFileInfo(path).lastModified().toSecsSinceEpoch())).toUtf8(),
                                                       QCryptographicHash::Sha1).toHex());
    if (!readDirectory() || !readPackage()) { const QString e = m_error; close(); m_error = e; return false; }
    if (m_spine.isEmpty()) { m_error = QStringLiteral("this book has no readable chapters"); return false; }
    m_titles = QStringList(m_spine.size(), QString());
    return true;
}

bool EpubBook::readDirectory()
{
    // End-of-central-directory record: the last 22+ bytes, signature 0x06054b50.
    const qsizetype n = m_zip.size();
    qsizetype eocd = -1;
    for (qsizetype i = n - 22; i >= qMax<qsizetype>(0, n - 22 - 65535); --i)
        if (u32(m_zip, i) == 0x06054b50) { eocd = i; break; }
    if (eocd < 0) { m_error = QStringLiteral("not an EPUB (not a zip file)"); return false; }
    const int count = u16(m_zip, eocd + 10);
    qsizetype o = u32(m_zip, eocd + 16);
    for (int i = 0; i < count; ++i) {
        if (o + 46 > n || u32(m_zip, o) != 0x02014b50) { m_error = QStringLiteral("damaged zip directory"); return false; }
        Entry e;
        e.method = u16(m_zip, o + 10);
        e.compSize = u32(m_zip, o + 20);
        e.size = u32(m_zip, o + 24);
        e.localOffset = u32(m_zip, o + 42);
        const int nameLen = u16(m_zip, o + 28), extraLen = u16(m_zip, o + 30), commentLen = u16(m_zip, o + 32);
        if (o + 46 + nameLen > n) { m_error = QStringLiteral("damaged zip directory"); return false; }
        const QString name = QString::fromUtf8(m_zip.constData() + o + 46, nameLen);
        if (!name.endsWith(QLatin1Char('/'))) m_entries.insert(name, e);
        o += 46 + nameLen + extraLen + commentLen;
    }
    return true;
}

QByteArray EpubBook::readEntry(const QString &name) const
{
    const auto it = m_entries.constFind(name);
    if (it == m_entries.constEnd()) return {};
    const Entry &e = it.value();
    const qsizetype o = e.localOffset;
    if (o + 30 > m_zip.size() || u32(m_zip, o) != 0x04034b50) return {};
    const qsizetype data = o + 30 + u16(m_zip, o + 26) + u16(m_zip, o + 28);
    if (data + qsizetype(e.compSize) > m_zip.size()) return {};
    const QByteArray raw = m_zip.mid(data, e.compSize);
    if (e.method == 0) return raw;
    if (e.method == 8) return inflateRaw(raw, e.size);
    return {};
}

QString EpubBook::resolve(const QString &baseDir, const QString &href) const
{
    QString h = QUrl::fromPercentEncoding(href.toUtf8());
    const int hash = h.indexOf(QLatin1Char('#'));
    if (hash >= 0) h.truncate(hash);
    return QDir::cleanPath(baseDir.isEmpty() ? h : baseDir + QLatin1Char('/') + h);
}

bool EpubBook::readPackage()
{
    // META-INF/container.xml names the package (.opf) file.
    QString opfPath;
    {
        QXmlStreamReader x(readEntry(QStringLiteral("META-INF/container.xml")));
        while (!x.atEnd()) {
            x.readNext();
            if (x.isStartElement() && x.name() == QLatin1String("rootfile")) {
                opfPath = x.attributes().value(QLatin1String("full-path")).toString();
                break;
            }
        }
    }
    if (opfPath.isEmpty()) { m_error = QStringLiteral("not an EPUB (no package file)"); return false; }
    const QString baseDir = QFileInfo(opfPath).path() == QLatin1String(".") ? QString() : QFileInfo(opfPath).path();

    QHash<QString, QString> manifest;      // id -> href
    QHash<QString, QString> mediaTypes;    // id -> media-type
    QStringList idrefs;
    QXmlStreamReader x(readEntry(opfPath));
    while (!x.atEnd()) {
        x.readNext();
        if (!x.isStartElement()) continue;
        const QStringView name = x.name();
        if (name == QLatin1String("title") && m_title.isEmpty()) m_title = x.readElementText().trimmed();
        else if (name == QLatin1String("creator") && m_author.isEmpty()) m_author = x.readElementText().trimmed();
        else if (name == QLatin1String("item")) {
            const QString id = x.attributes().value(QLatin1String("id")).toString();
            manifest.insert(id, x.attributes().value(QLatin1String("href")).toString());
            mediaTypes.insert(id, x.attributes().value(QLatin1String("media-type")).toString());
        } else if (name == QLatin1String("itemref")) {
            if (x.attributes().value(QLatin1String("linear")) != QLatin1String("no"))
                idrefs << x.attributes().value(QLatin1String("idref")).toString();
        }
    }
    if (x.hasError() && manifest.isEmpty()) { m_error = QStringLiteral("damaged package file"); return false; }
    for (const QString &id : idrefs) {
        const QString type = mediaTypes.value(id);
        if (!type.contains(QLatin1String("html")) && !type.contains(QLatin1String("xml"))) continue;
        const QString p = resolve(baseDir, manifest.value(id));
        if (m_entries.contains(p)) m_spine << p;
    }
    if (m_title.isEmpty()) m_title = QStringLiteral("untitled");
    return true;
}

QString EpubBook::extractImage(const QString &chapterPath, const QString &src)
{
    if (src.startsWith(QLatin1String("data:")) || src.contains(QLatin1String("://"))) return {};
    const QString base = QFileInfo(chapterPath).path() == QLatin1String(".") ? QString() : QFileInfo(chapterPath).path();
    const QString entry = resolve(base, src);
    if (!m_entries.contains(entry)) return {};
    const QString out = m_cacheDir + QLatin1Char('/')
        + QString::fromLatin1(QCryptographicHash::hash(entry.toUtf8(), QCryptographicHash::Sha1).toHex().left(16))
        + QLatin1Char('_') + QFileInfo(entry).fileName();
    if (!QFileInfo::exists(out)) {
        const QByteArray data = readEntry(entry);
        if (data.isEmpty()) return {};
        QDir().mkpath(m_cacheDir);
        QFile f(out);
        if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size()) return {};
    }
    return QUrl::fromLocalFile(out).toString();
}

QString EpubBook::chapterHtml(int i)
{
    if (i < 0 || i >= m_spine.size()) return {};
    QString s = QString::fromUtf8(readEntry(m_spine[i]));
    if (s.isEmpty()) return {};

    const QRegularExpression::PatternOption ci = QRegularExpression::CaseInsensitiveOption;
    const QRegularExpression::PatternOption dot = QRegularExpression::DotMatchesEverythingOption;
    const auto bodyStart = QRegularExpression(QStringLiteral("<body[^>]*>"), ci).match(s);
    if (bodyStart.hasMatch()) s = s.mid(bodyStart.capturedEnd());
    const qsizetype bodyEnd = s.lastIndexOf(QLatin1String("</body>"), -1, Qt::CaseInsensitive);
    if (bodyEnd >= 0) s.truncate(bodyEnd);

    // Publisher scripts, styles and vector art are not something Qt's rich text can use.
    s.remove(QRegularExpression(QStringLiteral("<script.*?</script>"), QRegularExpression::PatternOptions(ci | dot)));
    s.remove(QRegularExpression(QStringLiteral("<style.*?</style>"), QRegularExpression::PatternOptions(ci | dot)));
    s.remove(QRegularExpression(QStringLiteral("<svg.*?</svg>"), QRegularExpression::PatternOptions(ci | dot)));
    s.remove(QRegularExpression(QStringLiteral("<\\?xml[^>]*\\?>"), ci));
    s.replace(QRegularExpression(QStringLiteral("\\s(class|style|epub:type|xml:lang|lang)=\"[^\"]*\""), ci), QString());

    // Images: pull them out of the book and point at the extracted file.
    const QRegularExpression img(QStringLiteral("(<img[^>]*?\\ssrc=\")([^\"]+)(\")"), ci);
    QString out;
    qsizetype last = 0;
    for (auto m = img.globalMatch(s); m.hasNext();) {
        const auto mm = m.next();
        out += s.mid(last, mm.capturedStart(2) - last);
        const QString url = extractImage(m_spine[i], mm.captured(2));
        out += url.isEmpty() ? mm.captured(2) : url;
        last = mm.capturedEnd(2);
    }
    out += s.mid(last);
    return QStringLiteral("<html><body>") + out + QStringLiteral("</body></html>");
}

QString EpubBook::chapterTitle(int i)
{
    if (i < 0 || i >= m_spine.size()) return {};
    if (m_titles[i].isEmpty()) {
        const QString s = QString::fromUtf8(readEntry(m_spine[i]));
        const auto m = QRegularExpression(QStringLiteral("<h[1-3][^>]*>(.*?)</h[1-3]>"),
                                          QRegularExpression::PatternOptions(QRegularExpression::CaseInsensitiveOption
                                                                             | QRegularExpression::DotMatchesEverythingOption)).match(s);
        QString t = m.hasMatch() ? plainText(m.captured(1)) : QString();
        if (t.isEmpty()) t = QStringLiteral("chapter %1").arg(i + 1);
        m_titles[i] = t.left(80);
    }
    return m_titles[i];
}

} // namespace hop
