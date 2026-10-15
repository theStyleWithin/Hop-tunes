// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/Mpris.h"

#include "playback/Player.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QUrl>

// GIO has a struct member called `signals`, which Qt defines as a macro.
#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")
#include <unistd.h>

namespace hop {

namespace {

const char *kBusName = "org.mpris.MediaPlayer2.hoptunes";
const char *kPath = "/org/mpris/MediaPlayer2";
const char *kRootIface = "org.mpris.MediaPlayer2";
const char *kPlayerIface = "org.mpris.MediaPlayer2.Player";

const char *kXml = R"XML(
<node>
  <interface name="org.mpris.MediaPlayer2">
    <method name="Raise"/>
    <method name="Quit"/>
    <property name="CanQuit" type="b" access="read"/>
    <property name="CanRaise" type="b" access="read"/>
    <property name="HasTrackList" type="b" access="read"/>
    <property name="Identity" type="s" access="read"/>
    <property name="DesktopEntry" type="s" access="read"/>
    <property name="SupportedUriSchemes" type="as" access="read"/>
    <property name="SupportedMimeTypes" type="as" access="read"/>
  </interface>
  <interface name="org.mpris.MediaPlayer2.Player">
    <method name="Next"/>
    <method name="Previous"/>
    <method name="Pause"/>
    <method name="PlayPause"/>
    <method name="Stop"/>
    <method name="Play"/>
    <method name="Seek"><arg direction="in" name="Offset" type="x"/></method>
    <method name="SetPosition"><arg direction="in" name="TrackId" type="o"/><arg direction="in" name="Position" type="x"/></method>
    <method name="OpenUri"><arg direction="in" name="Uri" type="s"/></method>
    <signal name="Seeked"><arg name="Position" type="x"/></signal>
    <property name="PlaybackStatus" type="s" access="read"/>
    <property name="Rate" type="d" access="readwrite"/>
    <property name="Metadata" type="a{sv}" access="read"/>
    <property name="Volume" type="d" access="readwrite"/>
    <property name="Position" type="x" access="read"/>
    <property name="MinimumRate" type="d" access="read"/>
    <property name="MaximumRate" type="d" access="read"/>
    <property name="CanGoNext" type="b" access="read"/>
    <property name="CanGoPrevious" type="b" access="read"/>
    <property name="CanPlay" type="b" access="read"/>
    <property name="CanPause" type="b" access="read"/>
    <property name="CanSeek" type="b" access="read"/>
    <property name="CanControl" type="b" access="read"/>
  </interface>
</node>)XML";

GVariant *strv(std::initializer_list<const char *> items)
{
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE("as"));
    for (const char *s : items) g_variant_builder_add(&b, "s", s);
    return g_variant_builder_end(&b);
}

QByteArray u8(const QString &s) { return s.toUtf8(); }

} // namespace

Mpris::Mpris(Player *player, QObject *parent) : QObject(parent), m_player(player)
{
    connect(player, &Player::currentChanged, this, &Mpris::refresh);
    connect(player, &Player::playingChanged, this, &Mpris::refresh);
    connect(player, &Player::queueChanged, this, &Mpris::refresh);
    connect(player, &Player::volumeChanged, this, &Mpris::refresh);
    connect(player, &Player::durationChanged, this, &Mpris::refresh);
    connect(player, &Player::positionChanged, this, [this] { m_positionUs = m_player->position() * 1000; });
    connect(player, &Player::seeked, this, [this](qint64 ms) {
        m_positionUs = ms * 1000;
        if (m_registered.load())
            g_dbus_connection_emit_signal(m_conn, nullptr, kPath, kPlayerIface, "Seeked",
                                          g_variant_new("(x)", static_cast<gint64>(ms * 1000)), nullptr);
    });
    refresh();
}

Mpris::~Mpris()
{
    if (!m_thread.joinable()) return;
    // Queue the quit on the loop's own context: safe whether the loop is running, not yet
    // started (it runs the request first thing) or already finished (no session bus).
    g_main_context_invoke(m_ctx, [](gpointer loop) -> gboolean {
        g_main_loop_quit(static_cast<GMainLoop *>(loop));
        return G_SOURCE_REMOVE;
    }, m_loop);
    m_thread.join();
    g_main_loop_unref(m_loop);
    g_main_context_unref(m_ctx);
}

void Mpris::start()
{
    if (m_thread.joinable() || qEnvironmentVariableIsSet("HOPTUNES_NO_MPRIS")) return;
    m_ctx = g_main_context_new();
    m_loop = g_main_loop_new(m_ctx, FALSE);
    m_thread = std::thread([this] { run(); });
}

void Mpris::run()
{
    g_main_context_push_thread_default(m_ctx);
    GError *err = nullptr;
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &err);
    if (!conn) {
        if (err) g_error_free(err);      // no session bus: media keys simply unavailable
    } else {
        GDBusNodeInfo *node = g_dbus_node_info_new_for_xml(kXml, nullptr);
        const GDBusInterfaceVTable vtable = {
            reinterpret_cast<GDBusInterfaceMethodCallFunc>(&Mpris::onMethodCall),
            reinterpret_cast<GDBusInterfaceGetPropertyFunc>(&Mpris::onGetProperty),
            reinterpret_cast<GDBusInterfaceSetPropertyFunc>(&Mpris::onSetProperty),
            {nullptr}};
        bool ok = node != nullptr;
        for (int i = 0; ok && node->interfaces[i]; ++i)
            ok = g_dbus_connection_register_object(conn, kPath, node->interfaces[i], &vtable, this, nullptr, nullptr) > 0;
        if (node) g_dbus_node_info_unref(node);
        if (ok) {
            m_conn = conn;
            // Own the well-known name; on clash (a second instance) add a unique suffix as MPRIS allows.
            GVariant *r = g_dbus_connection_call_sync(conn, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                                                      "org.freedesktop.DBus", "RequestName",
                                                      g_variant_new("(su)", kBusName, 4u /*DO_NOT_QUEUE*/),
                                                      G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr);
            guint32 reply = 0;
            if (r) { g_variant_get(r, "(u)", &reply); g_variant_unref(r); }
            if (reply != 1 /*PRIMARY_OWNER*/) {
                const QByteArray alt = QByteArray(kBusName) + ".instance" + QByteArray::number(qint64(getpid()));
                g_dbus_connection_call_sync(conn, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                                            "RequestName", g_variant_new("(su)", alt.constData(), 4u), G_VARIANT_TYPE("(u)"),
                                            G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr);
            }
            m_registered = true;
            QMetaObject::invokeMethod(this, [this] { emit registeredChanged(true); }, Qt::QueuedConnection);
            g_main_loop_run(m_loop);
            m_registered = false;
            m_conn = nullptr;
        }
        g_object_unref(conn);
    }
    g_main_context_pop_thread_default(m_ctx);   // loop/context are freed by the destructor after join
}

// ---- Qt thread -----------------------------------------------------------------------------

void Mpris::refresh()
{
    State s;
    s.status = m_player->isPlaying() ? QStringLiteral("Playing")
             : m_player->hasTrack() ? QStringLiteral("Paused") : QStringLiteral("Stopped");
    if (const Track *t = m_player->currentTrack()) {
        s.trackId = QStringLiteral("/io/github/thestylewithin/hoptunes/track/%1").arg(t->id > 0 ? t->id : m_player->currentIndex() + 1);
        s.title = t->displayTitle();
        s.artist = t->displayArtist();
        s.album = t->displayAlbum();
        s.artUrl = m_player->artUrl();
        s.url = QUrl::fromLocalFile(t->path).toString();
        s.lengthUs = m_player->duration() * 1000;
        s.trackNo = t->trackNo;
    }
    s.volume = m_player->volume();
    s.canNext = m_player->currentIndex() + 1 < m_player->queueLength();
    s.canPrevious = m_player->hasTrack();
    s.canPlay = m_player->hasTrack();

    State old;
    {
        QMutexLocker lock(&m_mutex);
        old = m_state;
        m_state = s;
    }
    if (!m_registered.load()) return;

    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
    bool any = false;
    if (s.status != old.status) { g_variant_builder_add(&b, "{sv}", "PlaybackStatus", g_variant_new_string(u8(s.status))); any = true; }
    if (s.trackId != old.trackId || s.title != old.title || s.lengthUs != old.lengthUs || s.artUrl != old.artUrl) {
        g_variant_builder_add(&b, "{sv}", "Metadata", metadata(s)); any = true;
    }
    if (!qFuzzyCompare(s.volume + 1, old.volume + 1)) { g_variant_builder_add(&b, "{sv}", "Volume", g_variant_new_double(s.volume)); any = true; }
    if (s.canNext != old.canNext) { g_variant_builder_add(&b, "{sv}", "CanGoNext", g_variant_new_boolean(s.canNext)); any = true; }
    if (s.canPrevious != old.canPrevious) { g_variant_builder_add(&b, "{sv}", "CanGoPrevious", g_variant_new_boolean(s.canPrevious)); any = true; }
    if (s.canPlay != old.canPlay) {
        g_variant_builder_add(&b, "{sv}", "CanPlay", g_variant_new_boolean(s.canPlay));
        g_variant_builder_add(&b, "{sv}", "CanPause", g_variant_new_boolean(s.canPlay));
        g_variant_builder_add(&b, "{sv}", "CanSeek", g_variant_new_boolean(s.canPlay));
        any = true;
    }
    GVariant *changed = g_variant_builder_end(&b);
    if (any) emitPropertiesChanged(kPlayerIface, changed);
    else g_variant_unref(g_variant_ref_sink(changed));
}

void Mpris::emitPropertiesChanged(const char *iface, GVariant *changed)
{
    // GDBusConnection is thread-safe, so signals can be emitted from the Qt thread.
    g_dbus_connection_emit_signal(m_conn, nullptr, kPath, "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                  g_variant_new("(s@a{sv}@as)", iface, changed, g_variant_new_strv(nullptr, 0)),
                                  nullptr);
}

GVariant *Mpris::metadata(const State &s) const
{
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&b, "{sv}", "mpris:trackid", g_variant_new_object_path(u8(s.trackId)));
    if (!s.title.isEmpty()) {
        g_variant_builder_add(&b, "{sv}", "xesam:title", g_variant_new_string(u8(s.title)));
        const QByteArray artist = u8(s.artist);
        const char *artists[] = {artist.constData(), nullptr};
        g_variant_builder_add(&b, "{sv}", "xesam:artist", g_variant_new_strv(artists, 1));
        g_variant_builder_add(&b, "{sv}", "xesam:album", g_variant_new_string(u8(s.album)));
        g_variant_builder_add(&b, "{sv}", "xesam:url", g_variant_new_string(u8(s.url)));
        g_variant_builder_add(&b, "{sv}", "mpris:length", g_variant_new_int64(s.lengthUs));
        if (s.trackNo > 0) g_variant_builder_add(&b, "{sv}", "xesam:trackNumber", g_variant_new_int32(s.trackNo));
        if (!s.artUrl.isEmpty()) g_variant_builder_add(&b, "{sv}", "mpris:artUrl", g_variant_new_string(u8(s.artUrl)));
    }
    return g_variant_builder_end(&b);
}

// ---- D-Bus thread --------------------------------------------------------------------------

void Mpris::onMethodCall(GDBusConnection *, const char *, const char *, const char *iface, const char *method,
                         GVariant *params, GDBusMethodInvocation *inv, void *self)
{
    auto *m = static_cast<Mpris *>(self);
    Player *p = m->m_player;
    const QByteArray name(method);
    auto onQt = [m](auto fn) { QMetaObject::invokeMethod(m, fn, Qt::QueuedConnection); };

    if (g_strcmp0(iface, kRootIface) == 0) {
        if (name == "Raise") onQt([m] { emit m->raiseRequested(); });
        else if (name == "Quit") onQt([m] { emit m->quitRequested(); });
    } else if (name == "Next") onQt([p] { p->next(); });
    else if (name == "Previous") onQt([p] { p->previous(); });
    else if (name == "Pause") onQt([p] { p->pause(); });
    else if (name == "Play") onQt([p] { p->play(); });
    else if (name == "PlayPause") onQt([p] { p->toggle(); });
    else if (name == "Stop") onQt([p] { p->stop(); });
    else if (name == "Seek") {
        gint64 offset = 0;
        g_variant_get(params, "(x)", &offset);
        onQt([p, offset] { p->seek(p->position() + offset / 1000); });
    } else if (name == "SetPosition") {
        const char *track = nullptr;
        gint64 pos = 0;
        g_variant_get(params, "(&ox)", &track, &pos);
        QString current;
        { QMutexLocker lock(&m->m_mutex); current = m->m_state.trackId; }
        if (current == QString::fromUtf8(track)) onQt([p, pos] { p->seek(pos / 1000); });   // stale ids are ignored, per spec
    }
    g_dbus_method_invocation_return_value(inv, nullptr);
}

GVariant *Mpris::onGetProperty(GDBusConnection *, const char *, const char *, const char *iface, const char *prop,
                               GError **, void *self)
{
    auto *m = static_cast<Mpris *>(self);
    const QByteArray name(prop);
    if (g_strcmp0(iface, kRootIface) == 0) {
        if (name == "CanQuit" || name == "CanRaise") return g_variant_new_boolean(TRUE);
        if (name == "HasTrackList") return g_variant_new_boolean(FALSE);
        if (name == "Identity") return g_variant_new_string("Hop-tunes");
        if (name == "DesktopEntry") return g_variant_new_string("io.github.thestylewithin.hoptunes");
        if (name == "SupportedUriSchemes") return strv({"file"});
        if (name == "SupportedMimeTypes")
            return strv({"audio/mpeg", "audio/flac", "audio/ogg", "audio/opus", "audio/mp4", "audio/x-m4a", "audio/wav", "audio/x-ms-wma"});
        return nullptr;
    }
    State s;
    { QMutexLocker lock(&m->m_mutex); s = m->m_state; }
    if (name == "PlaybackStatus") return g_variant_new_string(u8(s.status));
    if (name == "Rate" || name == "MinimumRate" || name == "MaximumRate") return g_variant_new_double(1.0);
    if (name == "Metadata") return m->metadata(s);
    if (name == "Volume") return g_variant_new_double(s.volume);
    if (name == "Position") return g_variant_new_int64(m->m_positionUs.load());
    if (name == "CanGoNext") return g_variant_new_boolean(s.canNext);
    if (name == "CanGoPrevious") return g_variant_new_boolean(s.canPrevious);
    if (name == "CanPlay" || name == "CanPause" || name == "CanSeek") return g_variant_new_boolean(s.canPlay);
    if (name == "CanControl") return g_variant_new_boolean(TRUE);
    return nullptr;
}

int Mpris::onSetProperty(GDBusConnection *, const char *, const char *, const char *, const char *prop,
                         GVariant *value, GError **, void *self)
{
    auto *m = static_cast<Mpris *>(self);
    if (g_strcmp0(prop, "Volume") == 0) {
        const double v = g_variant_get_double(value);
        Player *p = m->m_player;
        QMetaObject::invokeMethod(m, [p, v] { p->setVolume(v); }, Qt::QueuedConnection);
        return TRUE;
    }
    return g_strcmp0(prop, "Rate") == 0;   // rate is fixed at 1.0; accept and ignore
}

} // namespace hop
