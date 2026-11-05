// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QVector>
#include <QtGlobal>

namespace hop {

// Finds the beat in a stream of spectrum frames (the player's `spectrum` signal: 32 bands, -80..0 dB, about 30
// frames a second). Pure maths with no GStreamer in it, so it is unit-tested with made-up frames.
//
// A beat is a sudden rise of low-frequency energy above its own recent average (a kick or a bass note), no
// sooner than 280 ms after the last one. Its strength (0..1) is how far it jumped; a heavy hit is near 1.
// The tempo is the median gap between recent beats, folded into 60-180 beats a minute.
class BeatTracker {
public:
    // Feeds one frame taken at time `tMs`. Returns true when this frame is a beat; `strength` is then set.
    bool feed(const QVector<float> &bandsDb, qint64 tMs, double *strength = nullptr);
    void reset();

    double bass() const { return m_bass; }       // smoothed levels 0..1 for the visualizer
    double mid() const { return m_mid; }
    double treble() const { return m_treble; }
    double level() const { return m_level; }     // overall loudness 0..1
    double bpm() const { return m_bpm; }         // 60..180, 110 until it has heard a few beats
    int beats() const { return m_beats; }        // how many beats so far

    static double toLinear(float db) { return qBound(0.0, (double(db) + 80.0) / 80.0, 1.0); }

private:
    double m_avg = 0;            // slow average of the bass energy
    double m_prev = 0;
    double m_bass = 0, m_mid = 0, m_treble = 0, m_level = 0;
    double m_bpm = 110;
    qint64 m_lastBeat = -100000;
    QList<qint64> m_gaps;        // the last few gaps between beats, folded
    int m_beats = 0;
};

} // namespace hop
