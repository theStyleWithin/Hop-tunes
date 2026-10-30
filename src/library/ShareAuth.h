// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

namespace hop {

class ProfileDb;

// Who may connect to a shared library (see docs/design-sharing.md). No networking here: this is the logic the
// server leans on, kept separate so it can be tested on its own.
//   - The host makes a one-time invite code. Its first four characters are a public label, and the whole code is
//     the shared secret of the first (encrypted) connection, so it never travels in the clear.
//   - A guest who connects with it is recorded and gets a private key (64 hex characters): the shared secret of every
//     later connection. The host keeps it in the profile database; the guest keeps it on their machine.
//   - Wrong attempts on an invite are counted; after a few the invite is cancelled, so guessing cannot work.
class ShareAuth {
public:
    explicit ShareAuth(ProfileDb &db) : m_db(db) {}

    static constexpr int kMaxMisses = 5;            // failed attempts on one invite before it is cancelled
    static constexpr int kLabelLength = 4;
    static constexpr int kCodeLength = 16;          // 4 label characters + 12 secret ones (about 60 bits)
    static constexpr qint64 kMinTtl = 10 * 60;       // seconds
    static constexpr qint64 kMaxTtl = 24 * 60 * 60;

    // A new one-time code, shown as ABCD-EFGH-JKMN-PQRS, valid for `ttlSecs` (kept between ten minutes and a day).
    // `now` is seconds since the epoch.
    QString createInvite(qint64 ttlSecs, qint64 now);
    int pendingInvites() const { return m_invites.size(); }

    // For the server's handshake: the shared secret of an invite (from its public label), empty if there is no such
    // live invite.
    QByteArray inviteSecret(const QString &label, qint64 now) const;
    // A handshake on this invite failed (wrong secret). After kMaxMisses the invite is cancelled.
    void noteInviteMiss(const QString &label);
    // The guest got in with this invite: record them, use the invite up, and return their private key (empty on failure).
    QString redeemInvite(const QString &label, const QString &guestName, qint64 now, qint64 *guestId = nullptr);

    // The shared secret of a known guest (empty when unknown, e.g. a removed guest).
    QByteArray guestSecret(qint64 guestId) const;
    // A guest is removed: their key stops working and their approvals are forgotten.
    bool removeGuest(qint64 guestId);

    static QString normalizeCode(const QString &code);   // upper case, no spaces or dashes
    static QByteArray secretFromCode(const QString &code);   // the shared secret an invite code stands for

private:
    struct Invite { QString code; qint64 expires = 0; int misses = 0; };
    ProfileDb &m_db;
    QHash<QString, Invite> m_invites;   // label -> invite
};

} // namespace hop
