// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/TagReader.h"

#include <QDateTime>
#include <QFile>

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>

namespace hop::TagReader {

const QStringList &audioExtensions()
{
    static const QStringList exts = {
        QStringLiteral("mp3"),  QStringLiteral("flac"), QStringLiteral("m4a"),
        QStringLiteral("m4b"),  QStringLiteral("aac"),  QStringLiteral("ogg"),
        QStringLiteral("oga"),  QStringLiteral("opus"), QStringLiteral("wav"),
        QStringLiteral("wma"),  QStringLiteral("aiff"), QStringLiteral("aif"),
        QStringLiteral("ape"),  QStringLiteral("wv"),   QStringLiteral("mpc"),
    };
    return exts;
}

bool isAudioFile(const QFileInfo &fi)
{
    return fi.isFile() && audioExtensions().contains(fi.suffix().toLower());
}

static QString qs(const TagLib::String &s) { return QString::fromStdString(s.to8Bit(true)).trimmed(); }

static QString firstProp(const TagLib::PropertyMap &props, const char *key)
{
    const auto it = props.find(key);
    if (it == props.end() || it->second.isEmpty()) return {};
    return qs(it->second.front());
}

// "3/12" -> 3
static int leadingInt(const QString &s)
{
    int n = 0;
    for (QChar c : s) {
        if (!c.isDigit()) break;
        n = n * 10 + c.digitValue();
        if (n > 100000) break;
    }
    return n;
}

Track read(const QFileInfo &fi)
{
    Track t;
    t.path = fi.absoluteFilePath();
    t.size = fi.size();
    t.mtime = fi.lastModified().toSecsSinceEpoch();
    t.codec = fi.suffix().toLower();

    const QByteArray native = QFile::encodeName(t.path);
    TagLib::FileRef ref(native.constData(), true, TagLib::AudioProperties::Average);
    if (ref.isNull()) return t;

    if (const TagLib::AudioProperties *ap = ref.audioProperties()) {
        t.durationMs = ap->lengthInMilliseconds();
        t.bitrate = ap->bitrate();
        t.sampleRate = ap->sampleRate();
    }
    if (const TagLib::Tag *tag = ref.tag()) {
        t.title = qs(tag->title());
        t.artist = qs(tag->artist());
        t.album = qs(tag->album());
        t.genre = qs(tag->genre());
        t.year = static_cast<int>(tag->year());
        t.trackNo = static_cast<int>(tag->track());
    }
    if (ref.file()) {
        const TagLib::PropertyMap props = ref.file()->properties();
        t.albumArtist = firstProp(props, "ALBUMARTIST");
        t.disc = leadingInt(firstProp(props, "DISCNUMBER"));
        if (t.trackNo == 0) t.trackNo = leadingInt(firstProp(props, "TRACKNUMBER"));
    }
    return t;
}

} // namespace hop::TagReader
