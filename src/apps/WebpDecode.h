// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QImage>

namespace hop {

// Kick serves every stream picture and avatar as WebP, and Qt reads WebP only through an optional plugin that many builds do not have
// (the Android app, some Linux installs, plain AppImages). This reads WebP itself with libwebp, so the pictures never depend on that plugin.
// Returns a null image when the bytes are not a WebP picture, or when this build was made without libwebp.
QImage decodeWebp(const QByteArray &bytes);
bool webpDecoderBuilt();

} // namespace hop
