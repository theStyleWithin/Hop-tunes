// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/AudioAnalyzer.h"

namespace hop {

void AudioAnalyzer::setActive(bool on)
{
    if (on == m_active) return;
    m_active = on;
    if (on) { m_t.reset(); m_clock.start(); m_beats = 0; }
    emit activeChanged();
}

void AudioAnalyzer::want(const QString &who, bool on)
{
    if (on) m_wants.insert(who); else m_wants.remove(who);
    setActive(!m_wants.isEmpty());
}

void AudioAnalyzer::onSpectrum(const QVector<float> &db)
{
    if (!m_active) return;
    if (!m_clock.isValid()) m_clock.start();
    double strength = 0;
    const bool b = m_t.feed(db, m_clock.elapsed(), &strength);
    m_bands.clear();
    for (float v : db) m_bands.append(BeatTracker::toLinear(v));
    emit frame();
    if (b) { ++m_beats; m_strength = strength; emit beat(strength); }
}

} // namespace hop
