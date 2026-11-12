// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps/WebpDecode.h"

#ifdef HOPTUNES_HAVE_WEBP
#include <webp/decode.h>
#endif

namespace hop {

bool webpDecoderBuilt()
{
#ifdef HOPTUNES_HAVE_WEBP
    return true;
#else
    return false;
#endif
}

QImage decodeWebp(const QByteArray &bytes)
{
#ifdef HOPTUNES_HAVE_WEBP
    const auto *data = reinterpret_cast<const uint8_t *>(bytes.constData());
    const size_t size = static_cast<size_t>(bytes.size());
    int w = 0, h = 0;
    if (size < 12 || !WebPGetInfo(data, size, &w, &h) || w <= 0 || h <= 0 || w > 8192 || h > 8192) return {};   // (a sane size only)
    QImage img(w, h, QImage::Format_RGBA8888);
    if (img.isNull()) return {};
    if (!WebPDecodeRGBAInto(data, size, img.bits(), static_cast<size_t>(img.sizeInBytes()), static_cast<int>(img.bytesPerLine()))) return {};
    return img;
#else
    Q_UNUSED(bytes);
    return {};
#endif
}

} // namespace hop
