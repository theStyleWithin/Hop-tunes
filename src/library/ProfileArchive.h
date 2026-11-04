// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace hop {

// The file a person keeps when they export or eject a profile. Plain JSON, readable by anyone:
//   { "app": "hop-tunes", "format": 1, "exportedAt": ms, "version": "1.7.3",
//     "profile": { "name", "accent", "avatar": base64 PNG or "" },
//     "settings": { ... the profile's settings.json ... },
//     "data": { table: [ [row], ... ], ... } }
// A PIN is never written to the file: whoever imports it sets their own.
// Music, videos and pictures are never inside it either, only where they were.
struct ProfileArchive {
    QString name;
    QString accent;
    QString appVersion;
    qint64 exportedAt = 0;
    QByteArray avatarPng;      // empty: none
    QJsonObject settings;
    QJsonObject data;          // ProfileDb::exportData()

    static constexpr int kFormat = 1;
    static constexpr qint64 kMaxFileBytes = 256LL * 1024 * 1024;

    QByteArray toBytes() const;
    // Strict: returns false with a plain-words reason and leaves `out` untouched on any problem.
    static bool parse(const QByteArray &bytes, ProfileArchive *out, QString *error);

    int rowCount(const QString &table) const;
    // Folders the profile pointed at (music, videos, every section), for the "missing here" list.
    QStringList folders() const;
};

} // namespace hop
