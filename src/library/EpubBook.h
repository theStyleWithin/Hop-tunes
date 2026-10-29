// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

namespace hop {

// A read-only EPUB: unzips it (stored and deflated entries, via zlib), finds the reading
// order from the package file, and turns each chapter into HTML that Qt's rich text can
// show. Images are extracted into a cache folder and referenced by file URL. Nothing here
// ever writes next to the book.
class EpubBook {
public:
    bool open(const QString &path, const QString &imageCacheDir);
    void close();
    bool isOpen() const { return !m_spine.isEmpty(); }
    QString lastError() const { return m_error; }

    QString title() const { return m_title; }
    QString author() const { return m_author; }
    int chapterCount() const { return m_spine.size(); }
    QString chapterTitle(int i);            // first heading in the chapter, else "chapter N"
    QString chapterHtml(int i);             // empty when out of range or unreadable

    // For tests and for callers that need a raw entry.
    QByteArray readEntry(const QString &name) const;
    QStringList entryNames() const { return m_entries.keys(); }

private:
    struct Entry { quint16 method = 0; quint32 compSize = 0; quint32 size = 0; quint32 localOffset = 0; };

    bool readDirectory();
    bool readPackage();
    QString resolve(const QString &baseDir, const QString &href) const;
    QString extractImage(const QString &chapterPath, const QString &src);

    QByteArray m_zip;                       // the whole file; EPUBs are small
    QHash<QString, Entry> m_entries;
    QStringList m_spine;                    // chapter entry paths, in reading order
    QStringList m_titles;                   // cached chapter titles ("" = not computed yet)
    QString m_title, m_author, m_error, m_cacheDir;
};

} // namespace hop
