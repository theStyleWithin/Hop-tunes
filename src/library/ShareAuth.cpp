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

QByteArray ShareAuth::secretFromCode(const QString &code)
{
    return QCryptographicHash::hash(QByteArray("hop-tunes share invite ") + normalizeCode(code).toUtf8(),
                                    QCryptographicHash::Sha256);
}

QString ShareAuth::createInvite(qint64 ttlSecs, qint64 now)
{
    // forget invites that ran out, so the list cannot grow
    for (auto it = m_invites.begin(); it != m_invites.end();)
        it = it.value().expires <= now ? m_invites.erase(it) : std::next(it);
    QString code;
    do { code = randomCode(); } while (m_invites.contains(code.left(kLabelLength)));
    m_invites.insert(code.left(kLabelLength), Invite{code, now + std::clamp(ttlSecs, kMinTtl, kMaxTtl), 0});
    QString shown;
    for (int i = 0; i < code.size(); i += 4) shown += (i ? QStringLiteral("-") : QString()) + code.mid(i, 4);
    return shown;
}

QByteArray ShareAuth::inviteSecret(const QString &label, qint64 now) const
{
    const auto it = m_invites.constFind(label.toUpper());
    if (it == m_invites.constEnd() || it.value().expires <= now) return {};
    return secretFromCode(it.value().code);
}

void ShareAuth::noteInviteMiss(const QString &label)
{
    const auto it = m_invites.find(label.toUpper());
    if (it == m_invites.end()) return;
    if (++it.value().misses >= kMaxMisses) m_invites.erase(it);   // guessing stops here
}

QString ShareAuth::redeemInvite(const QString &label, const QString &guestName, qint64 now, qint64 *guestId)
{
    const auto it = m_invites.constFind(label.toUpper());
    if (it == m_invites.constEnd() || it.value().expires <= now) return {};
    m_invites.erase(it);   // one use only
    QByteArray raw(32, 0);
    for (int i = 0; i < raw.size(); ++i) raw[i] = char(QRandomGenerator::system()->bounded(256));
    const QString key = QString::fromLatin1(raw.toHex());
    const QString name = guestName.simplified().isEmpty() ? QStringLiteral("guest") : guestName.simplified();
    const qint64 id = m_db.addShareGuest(name, key, now);
    if (guestId) *guestId = id;
    return id ? key : QString();
}

QByteArray ShareAuth::guestSecret(qint64 guestId) const { return m_db.shareGuestSecret(guestId); }

bool ShareAuth::removeGuest(qint64 guestId) { return m_db.removeShareGuest(guestId); }

} // namespace hop
