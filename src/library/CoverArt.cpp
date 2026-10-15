// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/CoverArt.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <taglib/aifffile.h>
#include <taglib/asffile.h>
#include <taglib/asfpicture.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4file.h>
#include <taglib/mpegfile.h>
#include <taglib/opusfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/xiphcomment.h>

namespace hop::CoverArt {

namespace {

QByteArray bytes(const TagLib::ByteVector &v) { return QByteArray(v.data(), int(v.size())); }
QString str(const TagLib::String &s) { return QString::fromStdString(s.to8Bit(true)); }

Picture fromId3(TagLib::ID3v2::Tag *tag)
{
    if (!tag) return {};
    const TagLib::ID3v2::FrameList frames = tag->frameList("APIC");
    Picture first;
    for (auto *f : frames) {
        auto *pic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(f);
        if (!pic || pic->picture().isEmpty()) continue;
        Picture p{bytes(pic->picture()), str(pic->mimeType())};
        if (pic->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) return p;
        if (first.isNull()) first = p;
    }
    return first;
}

template <typename PictureList>
Picture fromFlacPictures(const PictureList &list)
{
    Picture first;
    for (auto *pic : list) {
        if (!pic || pic->data().isEmpty()) continue;
        Picture p{bytes(pic->data()), str(pic->mimeType())};
        if (pic->type() == TagLib::FLAC::Picture::FrontCover) return p;
        if (first.isNull()) first = p;
    }
    return first;
}

Picture fromXiph(TagLib::Ogg::XiphComment *xc)
{
    return xc ? fromFlacPictures(xc->pictureList()) : Picture{};
}

} // namespace

Picture embedded(const QString &audioPath)
{
    const QByteArray native = QFile::encodeName(audioPath);
    const char *fn = native.constData();
    const QString ext = QFileInfo(audioPath).suffix().toLower();

    if (ext == QLatin1String("mp3")) {
        TagLib::MPEG::File f(fn, false);
        return f.isValid() ? fromId3(f.ID3v2Tag()) : Picture{};
    }
    if (ext == QLatin1String("flac")) {
        TagLib::FLAC::File f(fn, false);
        if (!f.isValid()) return {};
        Picture p = fromFlacPictures(f.pictureList());
        if (p.isNull()) p = fromXiph(f.xiphComment());
        if (p.isNull()) p = fromId3(f.ID3v2Tag());
        return p;
    }
    if (ext == QLatin1String("m4a") || ext == QLatin1String("m4b") || ext == QLatin1String("mp4")
        || ext == QLatin1String("aac")) {
        TagLib::MP4::File f(fn, false);
        if (!f.isValid() || !f.tag() || !f.tag()->contains("covr")) return {};
        const TagLib::MP4::CoverArtList covers = f.tag()->item("covr").toCoverArtList();
        for (const auto &c : covers) {
            if (c.data().isEmpty()) continue;
            const char *mime = c.format() == TagLib::MP4::CoverArt::PNG ? "image/png" : "image/jpeg";
            return Picture{bytes(c.data()), QString::fromLatin1(mime)};
        }
        return {};
    }
    if (ext == QLatin1String("ogg") || ext == QLatin1String("oga")) {
        TagLib::Ogg::Vorbis::File f(fn, false);
        return f.isValid() ? fromXiph(f.tag()) : Picture{};
    }
    if (ext == QLatin1String("opus")) {
        TagLib::Ogg::Opus::File f(fn, false);
        return f.isValid() ? fromXiph(f.tag()) : Picture{};
    }
    if (ext == QLatin1String("wma")) {
        TagLib::ASF::File f(fn, false);
        if (!f.isValid() || !f.tag()) return {};
        const TagLib::ASF::AttributeList attrs = f.tag()->attribute("WM/Picture");
        for (const auto &a : attrs) {
            const TagLib::ASF::Picture pic = a.toPicture();
            if (pic.isValid() && !pic.picture().isEmpty())
                return Picture{bytes(pic.picture()), str(pic.mimeType())};
        }
        return {};
    }
    if (ext == QLatin1String("wav")) {
        TagLib::RIFF::WAV::File f(fn, false);
        return (f.isValid() && f.hasID3v2Tag()) ? fromId3(f.ID3v2Tag()) : Picture{};
    }
    if (ext == QLatin1String("aiff") || ext == QLatin1String("aif")) {
        TagLib::RIFF::AIFF::File f(fn, false);
        return (f.isValid() && f.hasID3v2Tag()) ? fromId3(f.tag()) : Picture{};
    }
    return {};
}

QString folderImage(const QString &directory)
{
    static const QStringList stems = {QStringLiteral("cover"), QStringLiteral("folder"),
                                      QStringLiteral("front"), QStringLiteral("album")};
    static const QStringList exts = {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png")};
    const QFileInfoList files = QDir(directory).entryInfoList(QDir::Files | QDir::Readable);
    for (const QString &stem : stems)
        for (const QFileInfo &fi : files)
            if (fi.completeBaseName().compare(stem, Qt::CaseInsensitive) == 0
                && exts.contains(fi.suffix().toLower()))
                return fi.absoluteFilePath();
    return {};
}

QString store(const Picture &picture, const QString &cacheDir)
{
    if (picture.isNull()) return {};
    QString ext = QStringLiteral("jpg");
    if (picture.mime.contains(QLatin1String("png"), Qt::CaseInsensitive)
        || picture.data.startsWith("\x89PNG"))
        ext = QStringLiteral("png");
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(picture.data, QCryptographicHash::Sha1).toHex());
    const QString path = cacheDir + QLatin1Char('/') + hash + QLatin1Char('.') + ext;
    if (QFileInfo::exists(path)) return path;
    QDir().mkpath(cacheDir);
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(picture.data);
    return f.commit() ? path : QString();
}

QString find(const QString &audioPath, const QString &cacheDir)
{
    const QString cached = store(embedded(audioPath), cacheDir);
    return cached.isEmpty() ? folderImage(QFileInfo(audioPath).absolutePath()) : cached;
}

} // namespace hop::CoverArt
