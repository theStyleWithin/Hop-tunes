// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtGlobal>

namespace hop {

// What a guest's player should do to stay with the host's (Party Watch). Pure, so it can be tested on its own.
struct PartyAdvice {
    qint64 seekTo = -1;   // jump here, or -1 to leave the position alone
    int pause = -1;       // 1 pause, 0 play, -1 leave as it is
};

// `target` is where the host is now (already moved on by the time since it was sampled). A small drift is left alone so
// the picture does not stutter; a larger one is corrected with one jump.
inline PartyAdvice partyAdvice(qint64 localMs, bool localPaused, qint64 targetMs, bool targetPaused, qint64 slackMs = 1500)
{
    PartyAdvice a;
    const qint64 drift = localMs - targetMs;
    if (drift > slackMs || drift < -slackMs || (targetPaused && (drift > 300 || drift < -300))) a.seekTo = qMax<qint64>(0, targetMs);
    if (targetPaused != localPaused) a.pause = targetPaused ? 1 : 0;
    return a;
}

} // namespace hop
