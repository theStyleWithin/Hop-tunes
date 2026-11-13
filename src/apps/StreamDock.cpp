// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps/StreamDock.h"

#include "net/AppNet.h"

#include <QUrl>

namespace hop {

bool StreamDock::start(const QString &slug, const QString &name, const QString &title, const QString &url)
{
#ifdef Q_OS_ANDROID
    Q_UNUSED(slug) Q_UNUSED(name) Q_UNUSED(title) Q_UNUSED(url)
    return false;                                           // the phone has picture in picture instead
#else
    const QString s = slug.trimmed().toLower();
    if (s.isEmpty()) return false;
    if (active() && s == m_slug) {                          // the same channel again: it keeps playing, only the words are refreshed
        if (name != m_name || title != m_title) { m_name = name.left(80); m_title = title.left(200); emit changed(); }
        return true;
    }
    QString why;
    if (!AppNet::streamUrlAllowed(QStringLiteral("kick"), QUrl(url), &why)) return false;
    if (m_songPlaying && m_songPlaying() && m_pauseSong) m_pauseSong();      // two sounds never fight: the song waits
    m_slug = s; m_name = name.left(80); m_title = title.left(200); m_url = url;
    emit changed();
    return true;
#endif
}

void StreamDock::stop()
{
    if (!active() && !m_fullscreen) return;
    m_url.clear(); m_slug.clear(); m_name.clear(); m_title.clear(); m_fullscreen = false;
    if (m_wantsOpen) { m_wantsOpen = false; emit wantsOpenChanged(); }
    emit changed();
}

} // namespace hop
