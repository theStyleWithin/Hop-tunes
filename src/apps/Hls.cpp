// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps/Hls.h"

#include <QRegularExpression>
#include <algorithm>

namespace hop {

namespace {
// KEY=value or KEY="value" pairs after a tag's colon
QString attr(const QString &line, const QString &key)
{
    const QRegularExpression re(QStringLiteral("(?:^|[:,])") + QRegularExpression::escape(key) + QStringLiteral("=(\"[^\"]*\"|[^,]*)"));
    const auto m = re.match(line);
    if (!m.hasMatch()) return {};
    QString v = m.captured(1);
    if (v.startsWith(QLatin1Char('"'))) v = v.mid(1, v.size() - 2);
    return v;
}
bool isTag(const QString &l) { return l.startsWith(QLatin1Char('#')); }
QStringList lines(const QString &text)
{
    QStringList out;
    for (QString l : text.split(QLatin1Char('\n'))) { l = l.trimmed(); if (!l.isEmpty()) out << l; }
    return out;
}
} // namespace

QList<HlsVariant> parseHlsMaster(const QString &text, const QUrl &base)
{
    QList<HlsVariant> out;
    const QStringList ls = lines(text);
    if (ls.isEmpty() || ls.first() != QLatin1String("#EXTM3U")) return out;
    for (int i = 0; i < ls.size(); ++i) {
        if (!ls[i].startsWith(QLatin1String("#EXT-X-STREAM-INF"))) continue;
        HlsVariant v;
        v.bandwidth = attr(ls[i], QStringLiteral("BANDWIDTH")).toLongLong();
        const QString res = attr(ls[i], QStringLiteral("RESOLUTION"));
        const int x = res.indexOf(QLatin1Char('x'));
        if (x > 0) { v.width = res.left(x).toInt(); v.height = res.mid(x + 1).toInt(); }
        v.name = attr(ls[i], QStringLiteral("VIDEO"));
        int j = i + 1;
        while (j < ls.size() && isTag(ls[j])) ++j;                           // the address is the next line that is not a tag
        if (j >= ls.size()) break;
        v.url = base.resolved(QUrl(ls[j]));
        if (v.name.isEmpty()) v.name = v.height > 0 ? QStringLiteral("%1p").arg(v.height) : QStringLiteral("video");
        if (v.url.isValid()) out << v;
        i = j;
    }
    std::stable_sort(out.begin(), out.end(), [](const HlsVariant &a, const HlsVariant &b) { return a.bandwidth > b.bandwidth; });
    return out;
}

HlsMedia parseHlsMedia(const QString &text, const QUrl &base)
{
    HlsMedia m;
    const QStringList ls = lines(text);
    if (ls.isEmpty() || ls.first() != QLatin1String("#EXTM3U")) { m.unsupported = QStringLiteral("not a playlist"); return m; }
    double dur = 0;
    qint64 rangeLen = -1, rangeOff = -1, nextOff = 0;
    for (const QString &l : ls) {
        if (l.startsWith(QLatin1String("#EXT-X-KEY"))) {
            if (attr(l, QStringLiteral("METHOD")) != QLatin1String("NONE")) { m.unsupported = QStringLiteral("this video is encrypted, which Hop-tunes does not download"); return m; }
        } else if (l.startsWith(QLatin1String("#EXT-X-MAP"))) {
            m.unsupported = QStringLiteral("this video uses a format Hop-tunes cannot join yet");
            return m;
        } else if (l.startsWith(QLatin1String("#EXTINF:"))) {
            dur = l.mid(8).section(QLatin1Char(','), 0, 0).toDouble();
        } else if (l.startsWith(QLatin1String("#EXT-X-BYTERANGE:"))) {
            const QString v = l.mid(17);
            rangeLen = v.section(QLatin1Char('@'), 0, 0).toLongLong();
            rangeOff = v.contains(QLatin1Char('@')) ? v.section(QLatin1Char('@'), 1, 1).toLongLong() : nextOff;
        } else if (l.startsWith(QLatin1String("#EXT-X-MEDIA-SEQUENCE:"))) {
            m.firstSequence = l.mid(22).toLongLong();
        } else if (l.startsWith(QLatin1String("#EXT-X-TARGETDURATION:"))) {
            m.targetSeconds = l.mid(22).toDouble();
        } else if (l.startsWith(QLatin1String("#EXT-X-ENDLIST"))) {
            m.finished = true;
        } else if (!isTag(l)) {
            HlsPart p;
            p.url = base.resolved(QUrl(l));
            p.seconds = dur;
            if (rangeLen >= 0) { p.bytes = rangeLen; p.rangeStart = rangeOff; nextOff = rangeOff + rangeLen; }
            if (p.url.isValid()) {
                m.seconds += dur;
                // a slice that starts where the one before it ended, in the same file, joins it: one request instead of many
                if (!m.parts.isEmpty() && p.rangeStart >= 0) {
                    HlsPart &prev = m.parts.last();
                    if (prev.url == p.url && prev.rangeStart >= 0 && prev.rangeStart + prev.bytes == p.rangeStart && prev.bytes + p.bytes <= kHlsMaxJoinedPart) {
                        prev.bytes += p.bytes;
                        prev.seconds += p.seconds;
                        dur = 0; rangeLen = -1; rangeOff = -1;
                        continue;
                    }
                }
                m.parts << p;
            }
            dur = 0; rangeLen = -1; rangeOff = -1;
        }
    }
    if (m.parts.isEmpty() && m.unsupported.isEmpty()) m.unsupported = QStringLiteral("the playlist has no video in it");
    return m;
}

const HlsVariant *chooseHlsVariant(const QList<HlsVariant> &variants, int maxHeight)
{
    if (variants.isEmpty()) return nullptr;
    if (maxHeight <= 0) return &variants.first();
    const HlsVariant *best = nullptr;
    for (const HlsVariant &v : variants)
        if (v.height > 0 && v.height <= maxHeight && (!best || v.bandwidth > best->bandwidth)) best = &v;
    if (best) return best;
    const HlsVariant *smallest = &variants.first();
    for (const HlsVariant &v : variants) if (v.bandwidth < smallest->bandwidth) smallest = &v;
    return smallest;
}

} // namespace hop
