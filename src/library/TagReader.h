// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Track.h"

#include <QFileInfo>
#include <QStringList>

namespace hop::TagReader {

// Lower-case extensions the scanner indexes.
const QStringList &audioExtensions();
bool isAudioFile(const QFileInfo &fi);

// Fills tags and audio properties for one file via TagLib. Path, size and mtime come
// from `fi`. Never throws; a file TagLib can't parse still yields a Track whose title
// falls back to the file name.
Track read(const QFileInfo &fi);

} // namespace hop::TagReader
