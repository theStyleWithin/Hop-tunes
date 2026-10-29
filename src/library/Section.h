// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace hop {

// The file-based collection sections beyond music and videos. Each has its own folder list
// per profile, its own extensions, and shares one index (SectionIndex) and scanner.
enum class SectionKind { Pictures, Audiobooks, Ebooks, Podcasts };

constexpr SectionKind kAllSections[] = {SectionKind::Pictures, SectionKind::Audiobooks,
                                        SectionKind::Ebooks, SectionKind::Podcasts};

QString sectionKey(SectionKind k);                    // "pictures" | "audiobooks" | "ebooks" | "podcasts"
bool sectionFromKey(const QString &key, SectionKind *out);
const QStringList &sectionExtensions(SectionKind k);  // lower-case, no dot

// One file in a section (index table `item`).
struct SectionItem {
    qint64  id = 0;
    QString path;          // absolute, canonical
    qint64  mtime = 0;
    qint64  size = 0;
    QString title;         // tag title, else the file name
    QString group;         // album / show / author / folder: what the UI groups or captions by
    qint64  durationMs = 0;
    int     width = 0;     // pictures
    int     height = 0;
    qint64  added = 0;
};

} // namespace hop
