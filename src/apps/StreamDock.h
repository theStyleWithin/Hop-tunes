// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include <functional>

namespace hop {

// A live stream that keeps playing after the Kick app is left (desktop): the picture lives in a small window owned by the main window
// (StreamDock.qml), not by the app, and this object is the one place that knows "a stream is on". The rules are here so they can be tested:
//  * only an address the Kick app may give the video engine is accepted (AppNet::streamUrlAllowed), and never on Android
//  * starting a stream pauses a song that is playing; the same channel asked for again keeps playing (a fresh address does not restart it)
//  * it stops when a song starts, when a video opens, when the person stops it, or when the permission / the apps are switched off
class StreamDock : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString url READ url NOTIFY changed)
    Q_PROPERTY(QString slug READ slug NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(bool fullscreen READ fullscreen WRITE setFullscreen NOTIFY changed)   // the picture fills the window (the page's button)
    Q_PROPERTY(bool wantsOpen READ wantsOpen NOTIFY wantsOpenChanged)                // the person asked to go back to the channel page
public:
    explicit StreamDock(QObject *parent = nullptr) : QObject(parent) {}

    // how the dock reaches the music (set by App; tests give fakes)
    void setSongControl(std::function<bool()> playing, std::function<void()> pause) { m_songPlaying = std::move(playing); m_pauseSong = std::move(pause); }

    bool active() const { return !m_url.isEmpty(); }
    QString url() const { return m_url; }
    QString slug() const { return m_slug; }
    QString name() const { return m_name; }
    QString title() const { return m_title; }
    bool fullscreen() const { return m_fullscreen; }
    void setFullscreen(bool on) { if (on == m_fullscreen) return; m_fullscreen = on; emit changed(); }
    bool wantsOpen() const { return m_wantsOpen; }

    // true when the stream is playing (or already was, for the same channel)
    Q_INVOKABLE bool start(const QString &slug, const QString &name, const QString &title, const QString &url);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void requestOpen() { m_wantsOpen = true; emit wantsOpenChanged(); }       // "take me back to this channel" (the dock's expand button)
    Q_INVOKABLE void openHandled() { if (!m_wantsOpen) return; m_wantsOpen = false; emit wantsOpenChanged(); }

    void songStarted() { stop(); }        // a song began playing
    void videoOpened() { stop(); }        // a video opened

signals:
    void changed();
    void wantsOpenChanged();

private:
    QString m_url, m_slug, m_name, m_title;
    bool m_fullscreen = false, m_wantsOpen = false;
    std::function<bool()> m_songPlaying;
    std::function<void()> m_pauseSong;
};

} // namespace hop
