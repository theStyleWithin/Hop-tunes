// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/ProfileArchive.h"

#include "core/ProfileStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

namespace hop {

namespace {
constexpr int kMaxAvatarBytes = 2 * 1024 * 1024;
constexpr int kMaxSettingsBytes = 2 * 1024 * 1024;
}

QByteArray ProfileArchive::toBytes() const
{
    QJsonObject profile;
    profile.insert(QStringLiteral("name"), name);
    profile.insert(QStringLiteral("accent"), accent);
    profile.insert(QStringLiteral("avatar"), QString::fromLatin1(avatarPng.toBase64()));
    QJsonObject root;
    root.insert(QStringLiteral("app"), QStringLiteral("hop-tunes"));
    root.insert(QStringLiteral("format"), kFormat);
    root.insert(QStringLiteral("exportedAt"), double(exportedAt));
    root.insert(QStringLiteral("version"), appVersion);
    root.insert(QStringLiteral("profile"), profile);
    root.insert(QStringLiteral("settings"), settings);
    root.insert(QStringLiteral("data"), data);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool ProfileArchive::parse(const QByteArray &bytes, ProfileArchive *out, QString *error)
{
    auto fail = [&](const QString &why) { if (error) *error = why; return false; };
    if (bytes.isEmpty() || bytes.size() > kMaxFileBytes) return fail(QStringLiteral("that file is empty or too big to be a hop-tunes profile"));
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
        return fail(QStringLiteral("that is not a hop-tunes profile file"));
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("app")).toString() != QLatin1String("hop-tunes"))
        return fail(QStringLiteral("that is not a hop-tunes profile file"));
    const int format = root.value(QStringLiteral("format")).toInt(-1);
    if (format != kFormat) {
        return fail(format > kFormat ? QStringLiteral("this profile file was made by a newer hop-tunes. update the app to read it")
                                     : QStringLiteral("that is not a hop-tunes profile file"));
    }
    if (!root.value(QStringLiteral("profile")).isObject() || !root.value(QStringLiteral("settings")).isObject()
        || !root.value(QStringLiteral("data")).isObject())
        return fail(QStringLiteral("the profile file is damaged"));

    const QJsonObject profile = root.value(QStringLiteral("profile")).toObject();
    ProfileArchive a;
    a.name = profile.value(QStringLiteral("name")).toString().trimmed();
    a.accent = profile.value(QStringLiteral("accent")).toString();
    if (!ProfileStore::isValidName(a.name)) return fail(QStringLiteral("the profile file has no usable name"));
    if (!ProfileStore::isValidAccent(a.accent)) a.accent = QStringLiteral("#4fd1c5");
    const QString avatar = profile.value(QStringLiteral("avatar")).toString();
    if (!avatar.isEmpty()) {
        if (avatar.size() > kMaxAvatarBytes * 4 / 3 + 8) return fail(QStringLiteral("the profile picture in the file is too big"));
        a.avatarPng = QByteArray::fromBase64(avatar.toLatin1());
        // Only a PNG is ever written (see App::exportProfileTo); anything else is not ours.
        if (a.avatarPng.isEmpty() || a.avatarPng.size() > kMaxAvatarBytes || !a.avatarPng.startsWith("\x89PNG\r\n\x1a\n"))
            return fail(QStringLiteral("the profile picture in the file is not a picture"));
    }
    a.settings = root.value(QStringLiteral("settings")).toObject();
    if (QJsonDocument(a.settings).toJson(QJsonDocument::Compact).size() > kMaxSettingsBytes)
        return fail(QStringLiteral("the settings in the file are too big"));
    a.data = root.value(QStringLiteral("data")).toObject();
    a.appVersion = root.value(QStringLiteral("version")).toString().left(40);
    a.exportedAt = qint64(root.value(QStringLiteral("exportedAt")).toDouble());
    *out = a;
    return true;
}

int ProfileArchive::rowCount(const QString &table) const { return data.value(table).toArray().size(); }

QStringList ProfileArchive::folders() const
{
    QSet<QString> seen;
    QStringList out;
    auto add = [&](const QJsonValue &v) {
        const QString p = v.toString();
        if (!p.isEmpty() && !seen.contains(p)) { seen.insert(p); out << p; }
    };
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        if (it.key() == QLatin1String("folders") || it.key().endsWith(QLatin1String("Folders"))) {
            for (const QJsonValue &v : it.value().toArray()) add(v);
        } else if (it.key() == QLatin1String("videoCategoryFolders")) {
            for (const QJsonValue &arr : it.value().toObject()) for (const QJsonValue &v : arr.toArray()) add(v);
        }
    }
    return out;
}

} // namespace hop
