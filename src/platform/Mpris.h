// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMutex>
#include <QObject>
#include <QString>

#include <atomic>
#include <thread>

typedef struct _GDBusConnection GDBusConnection;
typedef struct _GMainLoop GMainLoop;
typedef struct _GMainContext GMainContext;
typedef struct _GDBusMethodInvocation GDBusMethodInvocation;
typedef struct _GVariant GVariant;
typedef struct _GError GError;

namespace hop {

class Player;

// MPRIS 2 (org.mpris.MediaPlayer2 + .Player) on the session bus, so media keys, desktop
// media widgets (KDE, GNOME, Steam Deck overlay) and tools like playerctl control playback.
// D-Bus runs on its own GLib thread; calls are marshalled to the Qt thread. If there is no
// session bus (headless, containers) it quietly does nothing.
class Mpris : public QObject {
    Q_OBJECT
public:
    explicit Mpris(Player *player, QObject *parent = nullptr);
    ~Mpris() override;

    void start();
    bool isRegistered() const { return m_registered.load(); }

signals:
    void raiseRequested();
    void quitRequested();
    void registeredChanged(bool registered);

private:
    struct State {
        QString status = QStringLiteral("Stopped");
        QString trackId = QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack");
        QString title, artist, album, artUrl, url;
        qint64 lengthUs = 0;
        int trackNo = 0;
        double volume = 1.0;
        bool canNext = false, canPrevious = false, canPlay = false;
        QString loop = QStringLiteral("None");          // None | Playlist | Track
        bool shuffle = false;
    };

    void run();
    void refresh();                                    // Qt thread: snapshot + PropertiesChanged
    void emitPropertiesChanged(const char *iface, GVariant *changed);
    GVariant *metadata(const State &s) const;

    static void onMethodCall(GDBusConnection *, const char *, const char *, const char *iface,
                             const char *method, GVariant *params, GDBusMethodInvocation *inv, void *self);
    static GVariant *onGetProperty(GDBusConnection *, const char *, const char *, const char *iface,
                                   const char *prop, GError **, void *self);
    static int onSetProperty(GDBusConnection *, const char *, const char *, const char *iface,
                             const char *prop, GVariant *value, GError **, void *self);

    Player *m_player;
    std::thread m_thread;
    GMainContext *m_ctx = nullptr;
    GMainLoop *m_loop = nullptr;
    GDBusConnection *m_conn = nullptr;     // set once on the D-Bus thread, then read-only
    std::atomic<bool> m_registered{false};
    std::atomic<qint64> m_positionUs{0};
    mutable QMutex m_mutex;
    State m_state;
};

} // namespace hop
