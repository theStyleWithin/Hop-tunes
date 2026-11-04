// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace hop::DiagLog {

// A small log kept in the app's data folder so a problem on a phone (where there is no terminal) can be read
// afterwards: every Qt warning and QML error, plus the app's own notes. The file is trimmed to a few hundred KB,
// and the previous run's log is kept as hoptunes.prev.log. Nothing leaves the device unless the user copies it.
void install(const QString &dir);          // starts a new log (the old one becomes the previous run's)
bool installed();
void note(const QString &line);            // one line, timestamped; does nothing before install()
QString current(int maxBytes = 65536);     // the tail of this run's log
QString previous(int maxBytes = 65536);    // the tail of the previous run's log
QString path();

} // namespace hop::DiagLog
