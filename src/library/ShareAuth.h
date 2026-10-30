// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QString>

namespace hop {

class ProfileDb;

// Who may connect to a shared library (see docs/design-sharing.md). No networking here: this is the logic a
// server will lean on, kept separate so it can be tested on its own.
//   - The host makes a one-time invite code; a guest redeems it once and gets a private key.
//   - Only the hash of that key is stored (in the profile database), next to the guest's name.
//   - Anything but a valid key is refused. Wrong invite codes are rate limited: after a few misses in a row every
//     outstanding invite is cancelled, so guessing cannot work.
class ShareAuth {
public:
    explicit ShareAuth(ProfileDb &db) : m_db(db) {}

    static constexpr int kMaxMisses = 5;           // wrong codes in a row before every invite is cancelled
    static constexpr int kCodeLength = 8;
    static constexpr qint64 kMinTtl = 10 * 60;      // seconds
    static constexpr qint64 kMaxTtl = 24 * 60 * 60;

    // A new one-time code, valid for `ttlSecs` (kept between ten minutes and a day). `now` is seconds since epoch.
    QString createInvite(qint64 ttlSecs, qint64 now);
    int pendingInvites() const { return m_invites.size(); }

    // Trade a code for a private key (64 hex characters), and record the guest. Empty when the code is wrong,
    // already used or expired.
    QString redeem(const QString &code, const QString &guestName, qint64 now);

    // The guest's id for a key they present, or 0 for anything unknown (including a removed guest's key).
    qint64 authenticate(const QString &key) const;

    // A guest is removed: their key stops working and their approvals are forgotten.
    bool removeGuest(qint64 guestId);

    static QString hashKey(const QString &key);
    static QString normalizeCode(const QString &code);   // upper case, no spaces or dashes

private:
    ProfileDb &m_db;
    QHash<QString, qint64> m_invites;   // normalized code -> expiry
    int m_misses = 0;
};

} // namespace hop
