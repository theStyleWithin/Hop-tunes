// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/ShareAuth.h"

#include "library/ProfileDb.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

#include <algorithm>

namespace hop {

namespace {

// No 0/O or 1/I/L: a code is read aloud or typed from a message.
const char kAlphabet[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";

QString randomCode()
{
    QString code;
    for (int i = 0; i < ShareAuth::kCodeLength; ++i)
        code += QLatin1Char(kAlphabet[QRandomGenerator::system()->bounded(int(sizeof(kAlphabet)) - 1)]);
    return code;
}

} // namespace

QString ShareAuth::normalizeCode(const QString &code)
{
    QString out;
    for (const QChar c : code)
        if (!c.isSpace() && c != QLatin1Char('-')) out += c.toUpper();
    return out;
}

QString ShareAuth::hashKey(const QString &key)
{
    return QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString ShareAuth::createInvite(qint64 ttlSecs, qint64 now)
{
    // forget invites that ran out, so the list cannot grow
    for (auto it = m_invites.begin(); it != m_invites.end();)
        it = it.value() <= now ? m_invites.erase(it) : std::next(it);
    QString code;
    do { code = randomCode(); } while (m_invites.contains(code));
    m_invites.insert(code, now + std::clamp(ttlSecs, kMinTtl, kMaxTtl));
    return code;
}

QString ShareAuth::redeem(const QString &code, const QString &guestName, qint64 now)
{
    const QString c = normalizeCode(code);
    const auto it = m_invites.constFind(c);
    if (it == m_invites.constEnd() || it.value() <= now) {
        if (it != m_invites.constEnd()) m_invites.remove(c);
        if (++m_misses >= kMaxMisses) { m_invites.clear(); m_misses = 0; }   // guessing stops here
        return {};
    }
    m_invites.remove(c);   // one use only
    m_misses = 0;
    QByteArray raw(32, 0);
    for (int i = 0; i < raw.size(); ++i) raw[i] = char(QRandomGenerator::system()->bounded(256));
    const QString key = QString::fromLatin1(raw.toHex());
    if (!m_db.addShareGuest(guestName.simplified().isEmpty() ? QStringLiteral("guest") : guestName.simplified(),
                            hashKey(key), now))
        return {};
    return key;
}

qint64 ShareAuth::authenticate(const QString &key) const
{
    if (key.size() != 64) return 0;
    return m_db.shareGuestByKeyHash(hashKey(key));
}

bool ShareAuth::removeGuest(qint64 guestId) { return m_db.removeShareGuest(guestId); }

} // namespace hop
