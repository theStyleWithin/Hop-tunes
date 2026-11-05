// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game/BeatTracker.h"

#include <QElapsedTimer>
#include <QObject>
#include <QVariantList>
#include <QVector>

namespace hop {

// What the visualizer and the games listen to. Feed it the player's spectrum frames (onSpectrum); it keeps
// smoothed levels, the tempo and announces beats. It only asks the player to analyse while something is
// listening (`active`), so it costs nothing the rest of the time.
class AudioAnalyzer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QVariantList bands READ bands NOTIFY frame)       // 32 values 0..1
    Q_PROPERTY(double bass READ bass NOTIFY frame)
    Q_PROPERTY(double mid READ mid NOTIFY frame)
    Q_PROPERTY(double treble READ treble NOTIFY frame)
    Q_PROPERTY(double level READ level NOTIFY frame)
    Q_PROPERTY(double bpm READ bpm NOTIFY frame)
    Q_PROPERTY(int beatCount READ beatCount NOTIFY beat)
    Q_PROPERTY(double beatStrength READ beatStrength NOTIFY beat)
public:
    explicit AudioAnalyzer(QObject *parent = nullptr) : QObject(parent) {}

    bool available() const { return m_available; }
    void setAvailable(bool a) { if (a != m_available) { m_available = a; emit availableChanged(); } }
    bool active() const { return m_active; }
    void setActive(bool on);
    QVariantList bands() const { return m_bands; }
    double bass() const { return m_t.bass(); }
    double mid() const { return m_t.mid(); }
    double treble() const { return m_t.treble(); }
    double level() const { return m_t.level(); }
    double bpm() const { return m_t.bpm(); }
    int beatCount() const { return m_beats; }
    double beatStrength() const { return m_strength; }

public slots:
    void onSpectrum(const QVector<float> &bandsDb);

signals:
    void availableChanged();
    void activeChanged();
    void frame();
    void beat(double strength);

private:
    BeatTracker m_t;
    QVariantList m_bands;
    QElapsedTimer m_clock;
    bool m_available = false, m_active = false;
    int m_beats = 0;
    double m_strength = 0;
};

} // namespace hop
