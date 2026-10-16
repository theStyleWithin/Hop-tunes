// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/Smart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>

namespace hop {

// ---- search -----------------------------------------------------------------------

QString foldForSearch(const QString &s)
{
    const QString d = s.normalized(QString::NormalizationForm_KD);
    QString out;
    out.reserve(d.size());
    for (const QChar c : d) {
        const auto cat = c.category();
        if (cat == QChar::Mark_NonSpacing || cat == QChar::Mark_SpacingCombining || cat == QChar::Mark_Enclosing) continue;
        out += c.toCaseFolded();
    }
    return out;
}

QStringList searchTokens(const QString &query)
{
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    return foldForSearch(query).split(ws, Qt::SkipEmptyParts);
}

bool matchesSearch(const Track &t, const QStringList &tokens)
{
    if (tokens.isEmpty()) return true;
    const QString hay = foldForSearch(t.displayTitle() + QLatin1Char('\n') + t.artist + QLatin1Char('\n') + t.albumArtist
                                      + QLatin1Char('\n') + t.album + QLatin1Char('\n') + t.genre);
    for (const QString &tok : tokens)
        if (!hay.contains(tok)) return false;
    return true;
}

// ---- smart rules ------------------------------------------------------------------

SmartRules SmartRules::fromJson(const QString &json)
{
    SmartRules r;
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    r.matchAll = o.value(QStringLiteral("match")).toString(QStringLiteral("all")) != QLatin1String("any");
    r.sort = o.value(QStringLiteral("sort")).toString(QStringLiteral("album"));
    r.limit = qMax(0, o.value(QStringLiteral("limit")).toInt());
    for (const QJsonValue &v : o.value(QStringLiteral("rules")).toArray()) {
        const QJsonObject ro = v.toObject();
        r.rules.append({ro.value(QStringLiteral("field")).toString(), ro.value(QStringLiteral("op")).toString(),
                        ro.value(QStringLiteral("value")).toVariant().toString()});
    }
    return r;
}

QString SmartRules::toJson() const
{
    QJsonArray arr;
    for (const SmartRule &r : rules)
        arr.append(QJsonObject{{QStringLiteral("field"), r.field}, {QStringLiteral("op"), r.op}, {QStringLiteral("value"), r.value}});
    const QJsonObject o{{QStringLiteral("match"), matchAll ? QStringLiteral("all") : QStringLiteral("any")},
                        {QStringLiteral("sort"), sort}, {QStringLiteral("limit"), limit}, {QStringLiteral("rules"), arr}};
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

namespace {

bool ruleMatches(const SmartRule &r, const Track &t, const TrackStats &s, qint64 now)
{
    auto text = [&](const QString &field) -> QString {
        if (field == QLatin1String("title")) return t.displayTitle();
        if (field == QLatin1String("artist")) return t.artist.isEmpty() ? t.albumArtist : t.artist;
        if (field == QLatin1String("album")) return t.album;
        return t.genre;
    };
    const QString f = r.field;
    if (f == QLatin1String("title") || f == QLatin1String("artist") || f == QLatin1String("album") || f == QLatin1String("genre")) {
        const QString a = foldForSearch(text(f)), b = foldForSearch(r.value.trimmed());
        if (r.op == QLatin1String("is")) return a == b;
        if (r.op == QLatin1String("is_not")) return a != b;
        if (r.op == QLatin1String("not_contains")) return !a.contains(b);
        return a.contains(b);   // contains
    }
    if (f == QLatin1String("year") || f == QLatin1String("plays") || f == QLatin1String("skips")) {
        const qint64 n = f == QLatin1String("year") ? t.year : (f == QLatin1String("plays") ? s.playCount : s.skipCount);
        const qint64 v = r.value.toLongLong();
        if (r.op == QLatin1String("ne")) return n != v;
        if (r.op == QLatin1String("gt")) return n > v;
        if (r.op == QLatin1String("lt")) return n < v;
        return n == v;
    }
    if (f == QLatin1String("added_days") || f == QLatin1String("played_days")) {
        const qint64 when = f == QLatin1String("added_days") ? t.added : s.lastPlayed;
        const bool within = when > 0 && now - when <= r.value.toLongLong() * 86400;
        return r.op == QLatin1String("not_within") ? !within : within;
    }
    if (f == QLatin1String("hearted")) {
        const bool want = r.value != QLatin1String("false") && r.value != QLatin1String("0");
        return s.hearted == want;
    }
    return false;
}

} // namespace

bool SmartRules::matches(const Track &t, const TrackStats &s, qint64 now) const
{
    if (rules.isEmpty()) return true;
    for (const SmartRule &r : rules) {
        const bool m = ruleMatches(r, t, s, now);
        if (matchAll && !m) return false;
        if (!matchAll && m) return true;
    }
    return matchAll;
}

QList<Track> SmartRules::apply(const QList<Track> &library, const QHash<QString, TrackStats> &stats,
                               qint64 now, quint32 seed) const
{
    QList<Track> out;
    for (const Track &t : library)
        if (matches(t, stats.value(t.path), now)) out.append(t);   // library is already in album order
    auto st = [&](const Track &t) { return stats.value(t.path); };
    if (sort == QLatin1String("added"))
        std::stable_sort(out.begin(), out.end(), [](const Track &a, const Track &b) { return a.added > b.added; });
    else if (sort == QLatin1String("plays"))
        std::stable_sort(out.begin(), out.end(), [&](const Track &a, const Track &b) { return st(a).playCount > st(b).playCount; });
    else if (sort == QLatin1String("played"))
        std::stable_sort(out.begin(), out.end(), [&](const Track &a, const Track &b) { return st(a).lastPlayed > st(b).lastPlayed; });
    else if (sort == QLatin1String("title"))
        std::stable_sort(out.begin(), out.end(), [](const Track &a, const Track &b) {
            return QString::localeAwareCompare(a.displayTitle(), b.displayTitle()) < 0; });
    else if (sort == QLatin1String("random")) {
        QRandomGenerator rng(seed ? seed : quint32(now));
        std::shuffle(out.begin(), out.end(), rng);
    }
    if (limit > 0 && out.size() > limit) out = out.mid(0, limit);
    return out;
}

QList<AutoList> autoLists()
{
    auto make = [](const char *key, const char *name, QList<SmartRule> rules, const char *sort, int limit) {
        SmartRules r;
        r.rules = std::move(rules);
        r.sort = QString::fromLatin1(sort);
        r.limit = limit;
        return AutoList{QString::fromLatin1(key), QString::fromUtf8(name), r};
    };
    return {
        make("auto:hearted", "hearted", {{QStringLiteral("hearted"), QStringLiteral("is"), QStringLiteral("true")}}, "album", 0),
        make("auto:most", "most played", {{QStringLiteral("plays"), QStringLiteral("gt"), QStringLiteral("0")}}, "plays", 100),
        make("auto:recent", "recently added", {{QStringLiteral("added_days"), QStringLiteral("within"), QStringLiteral("30")}}, "added", 200),
        make("auto:never", "never played", {{QStringLiteral("plays"), QStringLiteral("eq"), QStringLiteral("0")}}, "album", 0),
    };
}

// ---- smart DJ ---------------------------------------------------------------------

QList<Track> smartMix(const QString &seedArtist, const QList<Track> &library,
                      const QHash<QString, TrackStats> &stats, int count, quint32 seed)
{
    const QString seedKey = foldForSearch(seedArtist);
    auto artistKey = [](const Track &t) { return foldForSearch(t.artist.isEmpty() ? t.albumArtist : t.artist); };

    // What the seed artist sounds like: its genres and years.
    QHash<QString, int> genres;
    QList<int> years;
    for (const Track &t : library) {
        if (artistKey(t) != seedKey && foldForSearch(t.albumArtist) != seedKey) continue;
        if (!t.genre.isEmpty()) genres[foldForSearch(t.genre)]++;
        if (t.year > 0) years << t.year;
    }
    std::sort(years.begin(), years.end());
    const int midYear = years.isEmpty() ? 0 : years[years.size() / 2];

    struct Cand { int idx; double w; };
    QList<Cand> cands;
    for (int i = 0; i < library.size(); ++i) {
        const Track &t = library[i];
        const TrackStats s = stats.value(t.path);
        const bool own = artistKey(t) == seedKey || foldForSearch(t.albumArtist) == seedKey;
        double w = own ? 6.0 : 0.0;
        if (!t.genre.isEmpty() && genres.contains(foldForSearch(t.genre))) w += 3.0;
        if (midYear && t.year) {
            const int d = qAbs(t.year - midYear);
            if (d <= 3) w += 2.0; else if (d <= 8) w += 1.0;
        }
        if (w <= 0.0) continue;
        if (s.hearted) w += 2.0;
        if (s.playCount > 0) w += qMin(2.0, s.playCount / 5.0);
        if (s.skipCount > s.playCount + 2) w *= 0.4;   // often skipped
        cands.append({i, w});
    }
    // Too little in common: fall back to the whole library so the mix never runs dry.
    if (cands.size() < qMin(count, 10))
        for (int i = 0; i < library.size(); ++i)
            if (std::none_of(cands.begin(), cands.end(), [i](const Cand &c) { return c.idx == i; })) cands.append({i, 0.5});

    // Weighted sampling without replacement (Efraimidis–Spirakis keys), then spread the
    // same artist apart where possible.
    QRandomGenerator rng(seed);
    QList<std::pair<double, int>> keyed;
    for (const Cand &c : cands) keyed.append({std::pow(rng.generateDouble(), 1.0 / c.w), c.idx});
    std::sort(keyed.begin(), keyed.end(), [](auto &a, auto &b) { return a.first > b.first; });
    QList<Track> picked;
    for (int i = 0; i < keyed.size() && picked.size() < count; ++i) picked.append(library[keyed[i].second]);
    for (int i = 1; i < picked.size(); ++i) {
        if (artistKey(picked[i]) != artistKey(picked[i - 1])) continue;
        for (int j = i + 1; j < picked.size(); ++j)
            if (artistKey(picked[j]) != artistKey(picked[i - 1])) { picked.swapItemsAt(i, j); break; }
    }
    return picked;
}

// ---- playlist files ---------------------------------------------------------------

namespace PlaylistIO {

namespace {

QString normEntry(QString e)
{
    e = e.trimmed();
    // Junk from binary or damaged files: no real path holds control characters or runs
    // past PATH_MAX.
    if (e.size() > 4096) return {};
    for (const QChar c : e)
        if (c.unicode() < 0x20 || c == QChar::ReplacementCharacter) return {};
    if (e.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)) e = QUrl(e).toLocalFile();
    e.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return e;
}

QString textOf(const QByteArray &raw)
{
    // M3U8 is UTF-8; old .m3u files are often Latin-1. Strip a BOM either way.
    QString s = QString::fromUtf8(raw);
    if (s.contains(QChar::ReplacementCharacter)) s = QString::fromLatin1(raw);
    if (s.startsWith(QChar(0xFEFF))) s.remove(0, 1);
    return s;
}

} // namespace

Parsed read(const QString &file)
{
    Parsed p;
    p.name = QFileInfo(file).completeBaseName();
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) { p.error = f.errorString(); return p; }
    const QByteArray raw = f.read(16 * 1024 * 1024);
    const QString ext = QFileInfo(file).suffix().toLower();

    if (ext == QLatin1String("xspf") || ext == QLatin1String("zpl") || ext == QLatin1String("wpl")) {
        QXmlStreamReader x(raw);
        bool inTitle = false;
        while (!x.atEnd()) {
            x.readNext();
            if (x.isStartElement()) {
                const auto n = x.name();
                if (n == QLatin1String("title") && p.entries.isEmpty()) inTitle = true;
                else if (n == QLatin1String("location")) {
                    if (const QString e = normEntry(x.readElementText()); !e.isEmpty()) p.entries << e;
                } else if (n == QLatin1String("media") || n == QLatin1String("ref")) {
                    const QString src = x.attributes().value(QLatin1String("src")).toString();
                    if (const QString e = normEntry(src); !e.isEmpty()) p.entries << e;
                }
            } else if (x.isCharacters() && inTitle) {
                const QString t = x.text().toString().trimmed();
                if (!t.isEmpty()) p.name = t;
                inTitle = false;
            } else if (x.isEndElement()) {
                inTitle = false;
            }
        }
        if (x.hasError() && p.entries.isEmpty()) p.error = x.errorString();
        return p;
    }

    const QString text = textOf(raw);
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("\r\n|\r|\n")));
    if (ext == QLatin1String("pls")) {
        static const QRegularExpression fileN(QStringLiteral("^File\\d+=(.*)$"), QRegularExpression::CaseInsensitiveOption);
        for (const QString &l : lines) {
            if (const auto m = fileN.match(l); m.hasMatch()) { if (const QString e = normEntry(m.captured(1)); !e.isEmpty()) p.entries << e; }
        }
        return p;
    }
    for (const QString &l : lines) {   // m3u / m3u8
        const QString t = l.trimmed();
        if (t.startsWith(QLatin1String("#PLAYLIST:"))) { p.name = t.mid(10).trimmed(); continue; }
        if (t.isEmpty() || t.startsWith(QLatin1Char('#'))) continue;
        if (t.contains(QLatin1String("://")) && !t.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)) continue;   // streams
        if (const QString e = normEntry(t); !e.isEmpty()) p.entries << e;
    }
    return p;
}

QStringList resolve(const QStringList &entries, const QString &playlistDir,
                    const QStringList &libraryPaths, int *unmatched)
{
    const QSet<QString> lib(libraryPaths.begin(), libraryPaths.end());
    // Trailing-part index: "file", "album/file", "artist/album/file" (folded).
    QHash<QString, QString> tail1, tail2, tail3;
    QSet<QString> ambiguous1;
    for (const QString &p : libraryPaths) {
        const QStringList parts = p.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        const int n = int(parts.size());
        if (n >= 1) {
            const QString k = foldForSearch(parts[n - 1]);
            if (tail1.contains(k) && tail1[k] != p) ambiguous1.insert(k);
            tail1.insert(k, p);
        }
        if (n >= 2) tail2.insert(foldForSearch(parts[n - 2] + QLatin1Char('/') + parts[n - 1]), p);
        if (n >= 3) tail3.insert(foldForSearch(parts[n - 3] + QLatin1Char('/') + parts[n - 2] + QLatin1Char('/') + parts[n - 1]), p);
    }
    static const QRegularExpression drive(QStringLiteral("^[A-Za-z]:/"));
    QStringList out;
    int missing = 0;
    for (const QString &e : entries) {
        QString hit;
        if (!drive.match(e).hasMatch()) {
            const QString abs = QFileInfo(e).isAbsolute() ? e : QDir(playlistDir).filePath(e);
            const QString canon = QFileInfo(abs).canonicalFilePath();
            const QString clean = QDir::cleanPath(abs);
            if (lib.contains(canon)) hit = canon;
            else if (lib.contains(clean)) hit = clean;
        }
        if (hit.isEmpty()) {
            const QStringList parts = e.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            const int n = int(parts.size());
            if (n >= 3) hit = tail3.value(foldForSearch(parts[n - 3] + QLatin1Char('/') + parts[n - 2] + QLatin1Char('/') + parts[n - 1]));
            if (hit.isEmpty() && n >= 2) hit = tail2.value(foldForSearch(parts[n - 2] + QLatin1Char('/') + parts[n - 1]));
            if (hit.isEmpty() && n >= 1) {
                const QString k = foldForSearch(parts[n - 1]);
                if (!ambiguous1.contains(k)) hit = tail1.value(k);
            }
        }
        if (hit.isEmpty()) ++missing; else out << hit;
    }
    if (unmatched) *unmatched = missing;
    return out;
}

bool writeM3u8(const QString &file, const QString &name, const QList<Track> &tracks, QString *error)
{
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { if (error) *error = f.errorString(); return false; }
    const QDir base = QFileInfo(file).absoluteDir();
    QByteArray out = "#EXTM3U\n#PLAYLIST:" + name.toUtf8() + "\n";
    for (const Track &t : tracks) {
        out += "#EXTINF:" + QByteArray::number(t.durationMs / 1000) + "," + (t.displayArtist() + QStringLiteral(" - ") + t.displayTitle()).toUtf8() + "\n";
        out += base.relativeFilePath(t.path).toUtf8() + "\n";
    }
    f.write(out);
    if (!f.commit()) { if (error) *error = f.errorString(); return false; }
    return true;
}

} // namespace PlaylistIO

} // namespace hop
