// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/BeatTracker.h"

#include <algorithm>

namespace hop {

void BeatTracker::reset() { *this = BeatTracker(); }

bool BeatTracker::feed(const QVector<float> &db, qint64 tMs, double *strength)
{
    if (db.size() < 8) return false;
    auto lin = [&](int i) { return toLinear(db[i]); };
    // Band 0 holds the kick and the bass line (the bands are linear, about 690 Hz wide), band 1 the lower body.
    const double energy = lin(0) * 0.7 + lin(1) * 0.3;
    double mid = 0, treble = 0, all = 0;
    for (int i = 1; i < 6; ++i) mid += lin(i);
    for (int i = 6; i < db.size(); ++i) treble += lin(i);
    for (int i = 0; i < db.size(); ++i) all += lin(i);
    mid /= 5; treble /= double(db.size() - 6); all /= double(db.size());

    auto smooth = [](double &v, double target) { v += (target - v) * (target > v ? 0.6 : 0.12); };   // quick up, slow down
    smooth(m_bass, lin(0)); smooth(m_mid, mid); smooth(m_treble, treble); smooth(m_level, all);

    // a beat: a jump well above the recent average, still rising, with a rest since the last one
    bool beat = false;
    const double jump = energy - m_avg;
    if (energy > m_avg * 1.22 + 0.03 && energy - m_prev > 0.015 && jump > 0.05 && tMs - m_lastBeat >= 280 && m_level > 0.05) {
        beat = true;
        if (strength) *strength = qBound(0.0, jump / 0.30, 1.0);
        if (m_lastBeat > 0) {
            qint64 gap = tMs - m_lastBeat;
            while (gap < 333) gap *= 2;       // fold into 60-180 beats a minute
            while (gap > 1000) gap /= 2;
            m_gaps.append(gap);
            if (m_gaps.size() > 9) m_gaps.removeFirst();
            if (m_gaps.size() >= 4) {
                QList<qint64> sorted = m_gaps;
                std::sort(sorted.begin(), sorted.end());
                m_bpm = qBound(60.0, 60000.0 / double(sorted[sorted.size() / 2]), 180.0);
            }
        }
        m_lastBeat = tMs;
        ++m_beats;
    }
    m_avg += (energy - m_avg) * 0.045;   // about a second's memory at 30 frames a second
    m_prev = energy;
    return beat;
}

} // namespace hop
