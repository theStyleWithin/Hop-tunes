// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/SpectrumAnalyzer.h"

#include <QtMath>

#include <algorithm>
#include <complex>

namespace hop {

void SpectrumAnalyzer::setSampleRate(int hz)
{
    m_rate = qBound(8000, hz, 192000);
    m_hop = qMax(256, m_rate / 30);
}

void SpectrumAnalyzer::reset()
{
    m_ring.clear();
    m_ready.clear();
    m_sinceFrame = 0;
}

void SpectrumAnalyzer::feed(const float *mono, int count)
{
    if (count <= 0 || !mono) return;
    if (m_ring.isEmpty()) m_ring.fill(0.f, kWindow);
    // in steps of at most one hop, so a frame is made from the samples as they were at that moment
    while (count > 0) {
        const int n = qMin(count, qMax(1, m_hop - m_sinceFrame));
        std::move(m_ring.begin() + n, m_ring.end(), m_ring.begin());
        std::copy(mono, mono + n, m_ring.end() - n);
        mono += n;
        count -= n;
        m_sinceFrame += n;
        if (m_sinceFrame >= m_hop) {
            m_sinceFrame = 0;
            QVector<float> frame;
            transform(m_ring.constData(), &frame);
            m_ready.append(frame);
            if (m_ready.size() > 12) m_ready.removeFirst();   // nobody is listening: keep only the latest
        }
    }
}

bool SpectrumAnalyzer::next(QVector<float> *bandsDb)
{
    if (m_ready.isEmpty()) return false;
    *bandsDb = m_ready.takeFirst();
    return true;
}

void SpectrumAnalyzer::transform(const float *window, QVector<float> *bandsDb)
{
    constexpr int N = kWindow;
    if (m_hann.size() != N) {
        m_hann.resize(N);
        for (int i = 0; i < N; ++i) m_hann[i] = 0.5f * (1.f - std::cos(2.f * float(M_PI) * float(i) / float(N - 1)));
    }
    using C = std::complex<float>;
    static thread_local QVector<C> a;
    a.resize(N);
    for (int i = 0; i < N; ++i) a[i] = C(window[i] * m_hann[i], 0.f);

    // iterative radix-2 FFT
    for (int i = 1, j = 0; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (int len = 2; len <= N; len <<= 1) {
        const float ang = -2.f * float(M_PI) / float(len);
        const C wl(std::cos(ang), std::sin(ang));
        for (int i = 0; i < N; i += len) {
            C w(1.f, 0.f);
            for (int k = 0; k < len / 2; ++k) {
                const C u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
    // N/2 usable bins, folded into 32 equal bands; the loudest bin of a band stands for it. A full-scale sine under a
    // Hann window gives |X| = N/4, so that is what 0 dB means.
    bandsDb->resize(kBands);
    const int binsPerBand = (N / 2) / kBands;   // 32
    for (int b = 0; b < kBands; ++b) {
        float peak = 0.f;
        for (int k = b * binsPerBand; k < (b + 1) * binsPerBand; ++k) peak = std::max(peak, std::abs(a[k]));
        const float amp = peak / (float(N) / 4.f);
        const float db = 20.f * std::log10(std::max(amp, 1e-5f));
        (*bandsDb)[b] = std::clamp(db, -80.f, 0.f);
    }
}

} // namespace hop
