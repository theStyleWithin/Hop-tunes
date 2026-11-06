// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/AnalysisGuard.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace hop {

bool AnalysisGuard::tripped() const { return QFile::exists(m_path); }

void AnalysisGuard::arm()
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QFile f(m_path);
    if (f.open(QIODevice::WriteOnly)) f.write("1");
}

void AnalysisGuard::disarm() { QFile::remove(m_path); }

} // namespace hop
