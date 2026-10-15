// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace hop {

// Local profiles (spec §3). A profile is an offline container for one person's
// listening world: name, accent, optional avatar and PIN, plus its own settings.
// It is not an account and never leaves the machine.
struct Profile {
    QString id;          // uuid without braces; also the folder name
    QString name;
    QString accent;      // "#rrggbb"
    bool hasAvatar = false;
    bool hasPin = false;
    QDateTime created;
};

// profiles/profiles.json plus one folder per profile. Not thread-safe (UI thread only).
class ProfileStore {
public:
    explicit ProfileStore(const QString &rootDir);   // usually Paths::profilesDir()

    bool load();                                     // missing file = no profiles yet
    QString lastError() const { return m_error; }
    QString rootDir() const { return m_root; }

    QList<Profile> profiles() const { return m_profiles; }
    int count() const { return int(m_profiles.size()); }
    const Profile *find(const QString &id) const;

    // Returns the new profile's id, or an empty string (see lastError()).
    QString create(const QString &name, const QString &accent, const QString &pin = {});
    bool rename(const QString &id, const QString &name);
    bool setAccent(const QString &id, const QString &accent);
    bool setPin(const QString &id, const QString &pin);   // empty pin removes it
    bool setHasAvatar(const QString &id, bool has);
    // Deletes only the profile's own folder (playlists, stats, settings). Never music.
    bool remove(const QString &id);

    bool checkPin(const QString &id, const QString &pin) const;

    QString dirFor(const QString &id) const;
    QString avatarPath(const QString &id) const;      // dirFor(id)/avatar.png
    QJsonObject settings(const QString &id) const;    // dirFor(id)/settings.json
    bool saveSettings(const QString &id, const QJsonObject &settings);

    static bool isValidPin(const QString &pin);       // 4–8 digits
    static bool isValidName(const QString &name);     // 1–40 visible characters
    static bool isValidAccent(const QString &accent);
    // PIN storage format: "pbkdf2-sha256$<iterations>$<salt b64>$<hash b64>".
    static QString hashPin(const QString &pin);
    static bool verifyPin(const QString &pin, const QString &stored);

private:
    bool save();
    Profile *findMutable(const QString &id);

    QString m_root;
    QList<Profile> m_profiles;
    QHash<QString, QString> m_pinHashes;   // id -> stored hash
    QString m_error;
};

} // namespace hop
