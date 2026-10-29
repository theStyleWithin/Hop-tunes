// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/Section.h"

namespace hop {

QString sectionKey(SectionKind k)
{
    switch (k) {
    case SectionKind::Pictures:   return QStringLiteral("pictures");
    case SectionKind::Audiobooks: return QStringLiteral("audiobooks");
    case SectionKind::Ebooks:     return QStringLiteral("ebooks");
    case SectionKind::Podcasts:   return QStringLiteral("podcasts");
    }
    return {};
}

bool sectionFromKey(const QString &key, SectionKind *out)
{
    for (SectionKind k : kAllSections)
        if (sectionKey(k) == key) { if (out) *out = k; return true; }
    return false;
}

const QStringList &sectionExtensions(SectionKind k)
{
    static const QStringList pictures = {"jpg", "jpeg", "png", "webp", "gif", "bmp", "avif", "tif", "tiff"};
    static const QStringList audiobooks = {"m4b", "mp3", "m4a", "aac", "ogg", "opus", "flac"};
    static const QStringList ebooks = {"epub", "pdf"};
    static const QStringList podcasts = {"mp3", "m4a", "aac", "ogg", "opus", "wav", "flac"};
    switch (k) {
    case SectionKind::Pictures:   return pictures;
    case SectionKind::Audiobooks: return audiobooks;
    case SectionKind::Ebooks:     return ebooks;
    case SectionKind::Podcasts:   return podcasts;
    }
    return pictures;
}

} // namespace hop
