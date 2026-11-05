// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace hop {

// A crash guard for the optional sound analysis (the visualizer and the games). Before it starts listening a small marker
// file is written; when it stops cleanly the marker is removed. If the app finds the marker at start-up, the last run ended
// while the analysis was on (it crashed or was killed), so the feature is switched off until the person turns it back on
// in settings. Music playback never depends on it.
class AnalysisGuard {
public:
    explicit AnalysisGuard(const QString &dir) : m_path(dir + QStringLiteral("/analysis-running")) {}
    bool tripped() const;      // the marker was there at start-up (call before arm())
    void arm();                // the analysis is about to start
    void disarm();             // it stopped cleanly
    void clear() { disarm(); } // the person turned the feature back on
private:
    QString m_path;
};

} // namespace hop
