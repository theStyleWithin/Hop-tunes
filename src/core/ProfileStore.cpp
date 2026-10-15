// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/ProfileStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

namespace hop {

namespace {

constexpr int kPinIterations = 120000;
const auto kOwnerRW = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
const auto kOwnerRWX = kOwnerRW | QFileDevice::ExeOwner;

// PBKDF2-HMAC-SHA256 (RFC 8018), one 32-byte block. Built from QtCore only, so the
// PIN feature adds no dependency. A 4–8 digit PIN is a privacy curtain, not security
// (spec §3.2); the slow hash just stops it being readable at a glance.
QByteArray pbkdf2(const QByteArray &password, const QByteArray &salt, int iterations)
{
    QMessageAuthenticationCode mac(QCryptographicHash::Sha256, password);
    mac.addData(salt);
    mac.addData(QByteArray("\x00\x00\x00\x01", 4));
    QByteArray u = mac.result(), out = u;
    for (int i = 1; i < iterations; ++i) {
        mac.reset();
        mac.addData(u);
        u = mac.result();
        for (int j = 0; j < out.size(); ++j) out[j] = char(out[j] ^ u[j]);
    }
    return out;
}

bool writePrivate(const QString &path, const QByteArray &data)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.setPermissions(kOwnerRW);
    f.write(data);
    return f.commit();
}

} // namespace

ProfileStore::ProfileStore(const QString &rootDir) : m_root(rootDir) {}

bool ProfileStore::isValidPin(const QString &pin)
{
    static const QRegularExpression re(QStringLiteral("^[0-9]{4,8}$"));
    return re.match(pin).hasMatch();
}

bool ProfileStore::isValidName(const QString &name)
{
    const QString n = name.trimmed();
    return !n.isEmpty() && n.size() <= 40;
}

bool ProfileStore::isValidAccent(const QString &accent)
{
    static const QRegularExpression re(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    return re.match(accent).hasMatch();
}

QString ProfileStore::hashPin(const QString &pin)
{
    QByteArray salt(16, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(salt.data()), 4);
    const QByteArray h = pbkdf2(pin.toUtf8(), salt, kPinIterations);
    return QStringLiteral("pbkdf2-sha256$%1$%2$%3")
        .arg(kPinIterations).arg(QString::fromLatin1(salt.toBase64()), QString::fromLatin1(h.toBase64()));
}

bool ProfileStore::verifyPin(const QString &pin, const QString &stored)
{
    const QStringList p = stored.split(QLatin1Char('$'));
    if (p.size() != 4 || p[0] != QLatin1String("pbkdf2-sha256")) return false;
    bool ok = false;
    const int iter = p[1].toInt(&ok);
    if (!ok || iter < 1 || iter > 10'000'000) return false;
    const QByteArray want = QByteArray::fromBase64(p[3].toLatin1());
    const QByteArray got = pbkdf2(pin.toUtf8(), QByteArray::fromBase64(p[2].toLatin1()), iter);
    if (want.size() != got.size()) return false;
    char diff = 0;
    for (int i = 0; i < got.size(); ++i) diff |= char(got[i] ^ want[i]);
    return diff == 0;
}

bool ProfileStore::load()
{
    m_profiles.clear();
    m_pinHashes.clear();
    QFile f(m_root + QStringLiteral("/profiles.json"));
    if (!f.exists()) return true;
    if (!f.open(QIODevice::ReadOnly)) { m_error = f.errorString(); return false; }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) { m_error = err.errorString(); return false; }
    for (const QJsonValue &v : doc.object().value(QStringLiteral("profiles")).toArray()) {
        const QJsonObject o = v.toObject();
        Profile p;
        p.id = o.value(QStringLiteral("id")).toString();
        if (QUuid::fromString(p.id).isNull()) continue;   // also blocks path tricks in ids
        p.name = o.value(QStringLiteral("name")).toString();
        p.accent = o.value(QStringLiteral("accent")).toString();
        if (!isValidAccent(p.accent)) p.accent = QStringLiteral("#4fd1c5");
        p.hasAvatar = o.value(QStringLiteral("avatar")).toBool();
        p.created = QDateTime::fromString(o.value(QStringLiteral("created")).toString(), Qt::ISODate);
        const QString pin = o.value(QStringLiteral("pin")).toString();
        p.hasPin = !pin.isEmpty();
        if (p.hasPin) m_pinHashes.insert(p.id, pin);
        m_profiles.append(p);
    }
    return true;
}

bool ProfileStore::save()
{
    if (!QDir().mkpath(m_root)) { m_error = QStringLiteral("cannot create ") + m_root; return false; }
    QFile::setPermissions(m_root, kOwnerRWX);
    QJsonArray arr;
    for (const Profile &p : m_profiles) {
        QJsonObject o{{QStringLiteral("id"), p.id},
                      {QStringLiteral("name"), p.name},
                      {QStringLiteral("accent"), p.accent},
                      {QStringLiteral("avatar"), p.hasAvatar},
                      {QStringLiteral("created"), p.created.toString(Qt::ISODate)}};
        if (p.hasPin) o.insert(QStringLiteral("pin"), m_pinHashes.value(p.id));
        arr.append(o);
    }
    const QJsonObject root{{QStringLiteral("version"), 1}, {QStringLiteral("profiles"), arr}};
    if (!writePrivate(m_root + QStringLiteral("/profiles.json"), QJsonDocument(root).toJson())) {
        m_error = QStringLiteral("cannot write profiles.json");
        return false;
    }
    return true;
}

const Profile *ProfileStore::find(const QString &id) const
{
    for (const Profile &p : m_profiles)
        if (p.id == id) return &p;
    return nullptr;
}

Profile *ProfileStore::findMutable(const QString &id)
{
    for (Profile &p : m_profiles)
        if (p.id == id) return &p;
    return nullptr;
}

QString ProfileStore::create(const QString &name, const QString &accent, const QString &pin)
{
    if (!isValidName(name)) { m_error = QStringLiteral("name must be 1–40 characters"); return {}; }
    if (!pin.isEmpty() && !isValidPin(pin)) { m_error = QStringLiteral("PIN must be 4–8 digits"); return {}; }
    Profile p;
    p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    p.name = name.trimmed();
    p.accent = isValidAccent(accent) ? accent.toLower() : QStringLiteral("#4fd1c5");
    p.hasPin = !pin.isEmpty();
    p.created = QDateTime::currentDateTimeUtc();
    if (!QDir().mkpath(dirFor(p.id))) { m_error = QStringLiteral("cannot create profile folder"); return {}; }
    QFile::setPermissions(dirFor(p.id), kOwnerRWX);
    if (p.hasPin) m_pinHashes.insert(p.id, hashPin(pin));
    m_profiles.append(p);
    if (!save()) {
        m_profiles.removeLast();
        m_pinHashes.remove(p.id);
        QDir(dirFor(p.id)).removeRecursively();
        return {};
    }
    return p.id;
}

bool ProfileStore::rename(const QString &id, const QString &name)
{
    Profile *p = findMutable(id);
    if (!p || !isValidName(name)) return false;
    p->name = name.trimmed();
    return save();
}

bool ProfileStore::setAccent(const QString &id, const QString &accent)
{
    Profile *p = findMutable(id);
    if (!p || !isValidAccent(accent)) return false;
    p->accent = accent.toLower();
    return save();
}

bool ProfileStore::setPin(const QString &id, const QString &pin)
{
    Profile *p = findMutable(id);
    if (!p || (!pin.isEmpty() && !isValidPin(pin))) return false;
    p->hasPin = !pin.isEmpty();
    if (p->hasPin) m_pinHashes.insert(id, hashPin(pin)); else m_pinHashes.remove(id);
    return save();
}

bool ProfileStore::setHasAvatar(const QString &id, bool has)
{
    Profile *p = findMutable(id);
    if (!p) return false;
    p->hasAvatar = has;
    if (!has) QFile::remove(avatarPath(id));
    return save();
}

bool ProfileStore::remove(const QString &id)
{
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].id != id) continue;
        const Profile old = m_profiles.takeAt(i);
        const QString oldPin = m_pinHashes.take(id);
        if (!save()) {
            m_profiles.insert(i, old);
            if (!oldPin.isEmpty()) m_pinHashes.insert(id, oldPin);
            return false;
        }
        QDir(dirFor(id)).removeRecursively();
        return true;
    }
    return false;
}

bool ProfileStore::checkPin(const QString &id, const QString &pin) const
{
    const Profile *p = find(id);
    if (!p) return false;
    if (!p->hasPin) return true;
    return verifyPin(pin, m_pinHashes.value(id));
}

QString ProfileStore::dirFor(const QString &id) const { return m_root + QLatin1Char('/') + id; }
QString ProfileStore::avatarPath(const QString &id) const { return dirFor(id) + QStringLiteral("/avatar.png"); }

QJsonObject ProfileStore::settings(const QString &id) const
{
    QFile f(dirFor(id) + QStringLiteral("/settings.json"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

bool ProfileStore::saveSettings(const QString &id, const QJsonObject &settings)
{
    if (!find(id)) return false;
    return writePrivate(dirFor(id) + QStringLiteral("/settings.json"), QJsonDocument(settings).toJson());
}

} // namespace hop
