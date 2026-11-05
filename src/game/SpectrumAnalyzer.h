// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QVector>
#include <QtGlobal>

namespace hop {

// Turns raw mono samples into the same 32-band picture the desktop gets from GStreamer's "spectrum" element: 32 equal
// slices of 0 Hz .. half the sample rate, each as dB from -80 (silence) to 0 (a full-scale tone). Used on Android,
// where the player tap (QAudioBufferOutput) hands over decoded samples. Pure maths: tested with made-up sounds.
//
// feed() any number of samples; next() gives one frame each time `rate / 30` new samples have come in (about 30 a second).
class SpectrumAnalyzer {
public:
    static constexpr int kWindow = 2048;
    static constexpr int kBands = 32;

    void setSampleRate(int hz);
    int sampleRate() const { return m_rate; }
    void reset();
    void feed(const float *mono, int count);
    bool next(QVector<float> *bandsDb);   // true when a new frame is ready (call until false)

private:
    void transform(const float *window, QVector<float> *bandsDb);
    int m_rate = 44100;
    int m_hop = 1470;
    QVector<float> m_ring;      // the last kWindow samples, oldest first
    QList<QVector<float>> m_ready;   // frames made but not taken yet
    int m_sinceFrame = 0;
    QVector<float> m_hann;
};

} // namespace hop
