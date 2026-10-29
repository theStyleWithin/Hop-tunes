// SPDX-License-Identifier: GPL-3.0-or-later
// EpubBook: unzipping (stored and deflated entries), reading order, chapter HTML, images.
#include "library/EpubBook.h"

#include <QFile>
#include <QImage>
#include <QBuffer>
#include <QTemporaryDir>
#include <QtTest>

#include <zlib.h>

using namespace hop;

namespace {

void put16(QByteArray &b, quint16 v) { b.append(char(v & 0xff)); b.append(char(v >> 8)); }
void put32(QByteArray &b, quint32 v) { put16(b, quint16(v & 0xffff)); put16(b, quint16(v >> 16)); }

QByteArray deflateRaw(const QByteArray &in)
{
    z_stream zs{};
    deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    QByteArray out(int(deflateBound(&zs, uLong(in.size()))), 0);
    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    zs.avail_in = uInt(in.size());
    zs.next_out = reinterpret_cast<Bytef *>(out.data());
    zs.avail_out = uInt(out.size());
    deflate(&zs, Z_FINISH);
    out.resize(int(zs.total_out));
    deflateEnd(&zs);
    return out;
}

struct ZipEntry { QString name; QByteArray data; bool deflate; };

// A minimal but valid zip writer, enough for an EPUB.
QByteArray makeZip(const QList<ZipEntry> &entries)
{
    QByteArray out, central;
    for (const ZipEntry &e : entries) {
        const QByteArray name = e.name.toUtf8();
        const QByteArray body = e.deflate ? deflateRaw(e.data) : e.data;
        const quint32 crc = quint32(crc32(0, reinterpret_cast<const Bytef *>(e.data.constData()), uInt(e.data.size())));
        const quint32 offset = quint32(out.size());
        put32(out, 0x04034b50); put16(out, 20); put16(out, 0); put16(out, e.deflate ? 8 : 0);
        put16(out, 0); put16(out, 0); put32(out, crc); put32(out, quint32(body.size()));
        put32(out, quint32(e.data.size())); put16(out, quint16(name.size())); put16(out, 0);
        out += name; out += body;
        put32(central, 0x02014b50); put16(central, 20); put16(central, 20); put16(central, 0);
        put16(central, e.deflate ? 8 : 0); put16(central, 0); put16(central, 0); put32(central, crc);
        put32(central, quint32(body.size())); put32(central, quint32(e.data.size()));
        put16(central, quint16(name.size())); put16(central, 0); put16(central, 0); put16(central, 0);
        put16(central, 0); put32(central, 0); put32(central, offset);
        central += name;
    }
    const quint32 cdOffset = quint32(out.size());
    out += central;
    put32(out, 0x06054b50); put16(out, 0); put16(out, 0);
    put16(out, quint16(entries.size())); put16(out, quint16(entries.size()));
    put32(out, quint32(central.size())); put32(out, cdOffset); put16(out, 0);
    return out;
}

QByteArray png()
{
    QImage img(4, 4, QImage::Format_RGB32);
    img.fill(Qt::green);
    QByteArray b;
    QBuffer buf(&b);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return b;
}

QByteArray sampleEpub()
{
    const QByteArray container =
        "<?xml version=\"1.0\"?><container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
        "<rootfiles><rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles></container>";
    const QByteArray opf =
        "<?xml version=\"1.0\"?><package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\">"
        "<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:title>Test Book</dc:title><dc:creator>An Author</dc:creator></metadata>"
        "<manifest>"
        "<item id=\"c2\" href=\"text/two.xhtml\" media-type=\"application/xhtml+xml\"/>"
        "<item id=\"c1\" href=\"text/one.xhtml\" media-type=\"application/xhtml+xml\"/>"
        "<item id=\"pic\" href=\"images/pic%201.png\" media-type=\"image/png\"/>"
        "</manifest><spine><itemref idref=\"c1\"/><itemref idref=\"c2\"/></spine></package>";
    const QByteArray one =
        "<?xml version=\"1.0\"?><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>x</title><style>p{color:red}</style></head>"
        "<body class=\"main\"><h1 class=\"t\">The Beginning</h1><p>Hello <b>world</b>.</p>"
        "<script>alert(1)</script><img src=\"../images/pic%201.png\" alt=\"pic\"/></body></html>";
    const QByteArray two = "<html><body><p>No heading here.</p></body></html>";
    return makeZip({
        {"mimetype", "application/epub+zip", false},
        {"META-INF/container.xml", container, true},
        {"OEBPS/content.opf", opf, true},
        {"OEBPS/text/one.xhtml", one, true},
        {"OEBPS/text/two.xhtml", two, false},
        {"OEBPS/images/pic 1.png", png(), false},
    });
}

} // namespace

class TestEpub : public QObject {
    Q_OBJECT
private slots:
    void readsStoredAndDeflatedEntries()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("t.epub"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(sampleEpub());
        f.close();
        EpubBook b;
        QVERIFY2(b.open(path, dir.filePath(QStringLiteral("cache"))), qPrintable(b.lastError()));
        QCOMPARE(b.readEntry(QStringLiteral("mimetype")), QByteArray("application/epub+zip"));   // stored
        QVERIFY(b.readEntry(QStringLiteral("META-INF/container.xml")).contains("rootfile"));      // deflated
        QVERIFY(b.readEntry(QStringLiteral("nope")).isEmpty());
    }

    void readingOrderTitlesAndHtml()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("t.epub"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(sampleEpub());
        f.close();
        EpubBook b;
        QVERIFY(b.open(path, dir.filePath(QStringLiteral("cache"))));
        QCOMPARE(b.title(), QStringLiteral("Test Book"));
        QCOMPARE(b.author(), QStringLiteral("An Author"));
        QCOMPARE(b.chapterCount(), 2);                                   // spine order, not manifest order
        QCOMPARE(b.chapterTitle(0), QStringLiteral("The Beginning"));    // first heading
        QCOMPARE(b.chapterTitle(1), QStringLiteral("chapter 2"));        // no heading: numbered
        QVERIFY(b.chapterTitle(9).isEmpty());

        const QString html = b.chapterHtml(0);
        QVERIFY(html.contains(QLatin1String("Hello <b>world</b>.")));
        QVERIFY(!html.contains(QLatin1String("alert(1)")));              // scripts dropped
        QVERIFY(!html.contains(QLatin1String("color:red")));             // publisher styles dropped
        QVERIFY(!html.contains(QLatin1String("class=")));
        // The image (percent-encoded name, relative to the chapter) was extracted and linked.
        QVERIFY2(html.contains(QLatin1String("file://")), qPrintable(html));
        QVERIFY(!html.contains(QLatin1String("../images")));
        QVERIFY(b.chapterHtml(5).isEmpty());
    }

    void rejectsNonEpubs()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("bad.epub"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not a zip file at all");
        f.close();
        EpubBook b;
        QVERIFY(!b.open(path, dir.filePath(QStringLiteral("cache"))));
        QVERIFY(!b.lastError().isEmpty());
        QVERIFY(!b.isOpen());
        QVERIFY(!b.open(dir.filePath(QStringLiteral("missing.epub")), dir.path()));
    }
};

QTEST_MAIN(TestEpub)
#include "test_epub.moc"
