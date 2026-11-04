// SPDX-License-Identifier: GPL-3.0-or-later
// A tiny "playbin" for Android on top of Qt Multimedia. See gst/gst.h.
#include "gst/gst.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QPointer>
#include <QQueue>
#include <QUrl>

#include <cstdarg>
#include <cstdlib>
#include <cstring>

namespace {
enum Kind { KElement = 1, KBus = 2, KFactory = 3 };
struct Obj { int kind; };
}

struct _GstBus : Obj {
    QQueue<GstMessage *> q;
    _GstBus() : Obj{KBus} {}
    ~_GstBus() { qDeleteAll(q); }
};

struct _GstElement : Obj {
    QMediaPlayer *player = nullptr;
    QAudioOutput *out = nullptr;
    _GstBus *bus = nullptr;
    GstState state = GST_STATE_NULL;
    bool loaded = false;
    _GstElement() : Obj{KElement} {}
};

namespace {
GstMessage *makeMsg(int type, void *src, const QString &text = {})
{
    auto *m = new GstMessage;
    m->type = type;
    m->src = src;
    if (!text.isEmpty()) m->text = qstrdup(text.toUtf8().constData());
    return m;
}
Obj factorySentinel{KFactory};
}

void g_free(void *p) { std::free(p); }
void g_error_free(GError *e) { if (e) { std::free(e->message); delete e; } }
gboolean gst_is_initialized() { return TRUE; }
void gst_init(int *, char ***) {}
void gst_version(guint *a, guint *b, guint *c, guint *d) { *a = 1; *b = 0; *c = 0; *d = 0; }   // -> plain "playbin"

gchar *gst_filename_to_uri(const char *path, GError **)
{
    const QByteArray u = QUrl::fromLocalFile(QString::fromUtf8(path)).toEncoded();
    return strdup(u.constData());
}

GstElementFactory *gst_element_factory_find(const char *name)
{
    return std::strcmp(name, "playbin") == 0 ? reinterpret_cast<GstElementFactory *>(&factorySentinel) : nullptr;
}

GstElement *gst_element_factory_make(const char *name, const char *)
{
    if (std::strcmp(name, "playbin") != 0) return nullptr;
    auto *e = new _GstElement;
    e->bus = new _GstBus;
    e->out = new QAudioOutput;
    e->player = new QMediaPlayer;
    e->player->setAudioOutput(e->out);
    QObject::connect(e->player, &QMediaPlayer::mediaStatusChanged, e->player, [e](QMediaPlayer::MediaStatus st) {
        if (st == QMediaPlayer::LoadedMedia) {
            e->loaded = true;
            e->bus->q.enqueue(makeMsg(GST_MESSAGE_STREAM_START, e));
            e->bus->q.enqueue(makeMsg(GST_MESSAGE_ASYNC_DONE, e));
        } else if (st == QMediaPlayer::EndOfMedia) {
            e->bus->q.enqueue(makeMsg(GST_MESSAGE_EOS, e));
        }
    });
    QObject::connect(e->player, &QMediaPlayer::errorOccurred, e->player,
                     [e](QMediaPlayer::Error, const QString &text) {
        e->bus->q.enqueue(makeMsg(GST_MESSAGE_ERROR, e, text.isEmpty() ? QStringLiteral("playback error") : text));
    });
    return e;
}

GstElement *gst_parse_launch(const char *, GError **) { return nullptr; }
GstElement *gst_parse_bin_from_description(const char *, gboolean, GError **) { return nullptr; }
GstElement *gst_bin_get_by_name(GstBin *, const char *) { return nullptr; }
GstBus *gst_element_get_bus(GstElement *e) { return e ? e->bus : nullptr; }
void *gst_object_ref_sink(void *o) { return o; }

void gst_object_unref(void *o)
{
    if (!o) return;
    switch (static_cast<Obj *>(o)->kind) {
    case KElement: {
        auto *e = static_cast<_GstElement *>(o);
        delete e->player;
        delete e->out;
        delete e->bus;
        delete e;
        break;
    }
    default: break;   // the bus belongs to its element, the factory is a sentinel
    }
}

void g_object_set(void *obj, const char *first, ...)
{
    auto *e = static_cast<_GstElement *>(obj);
    va_list ap;
    va_start(ap, first);
    for (const char *k = first; k; k = va_arg(ap, const char *)) {
        if (!std::strcmp(k, "uri")) {
            const char *u = va_arg(ap, const char *);
            if (e && e->player) { e->loaded = false; e->player->setSource(QUrl(QString::fromUtf8(u))); }
        } else if (!std::strcmp(k, "volume")) {
            const double v = va_arg(ap, double);
            if (e && e->out) e->out->setVolume(float(qBound(0.0, v, 1.0)));
        } else if (!std::strcmp(k, "pre-amp") || !std::strcmp(k, "fallback-gain")) {
            (void) va_arg(ap, double);
        } else if (!std::strcmp(k, "audio-sink") || !std::strcmp(k, "audio-filter")) {
            (void) va_arg(ap, void *);
        } else {   // "flags", "album-mode": int-sized
            (void) va_arg(ap, int);
        }
    }
    va_end(ap);
}

GstStateChangeReturn gst_element_set_state(GstElement *e, GstState s)
{
    if (!e) return GST_STATE_CHANGE_FAILURE;
    e->state = s;
    switch (s) {
    case GST_STATE_PLAYING: e->player->play(); break;
    case GST_STATE_PAUSED: e->player->pause(); break;
    default: e->player->stop(); break;
    }
    return GST_STATE_CHANGE_SUCCESS;
}

GstStateChangeReturn gst_element_get_state(GstElement *e, GstState *s, GstState *, int64_t)
{
    if (s) *s = e ? e->state : GST_STATE_NULL;
    return e ? GST_STATE_CHANGE_SUCCESS : GST_STATE_CHANGE_FAILURE;
}

gboolean gst_element_seek_simple(GstElement *e, GstFormat, GstSeekFlags, gint64 ns)
{
    if (!e) return FALSE;
    e->player->setPosition(ns / GST_MSECOND);
    return TRUE;
}
gboolean gst_element_query_position(GstElement *e, GstFormat, gint64 *ns)
{
    if (!e || !e->loaded) return FALSE;
    *ns = e->player->position() * GST_MSECOND;
    return TRUE;
}
gboolean gst_element_query_duration(GstElement *e, GstFormat, gint64 *ns)
{
    if (!e || !e->loaded) return FALSE;
    *ns = e->player->duration() * GST_MSECOND;
    return TRUE;
}

GstMessage *gst_bus_pop(GstBus *b) { return b && !b->q.isEmpty() ? b->q.dequeue() : nullptr; }
void gst_bus_set_flushing(GstBus *b, gboolean flush) { if (b && flush) { qDeleteAll(b->q); b->q.clear(); } }
void gst_message_unref(GstMessage *m) { if (m) { std::free(m->text); delete m; } }
void gst_message_parse_error(GstMessage *m, GError **e, gchar **d)
{
    if (e) { *e = new GError; (*e)->message = strdup(m && m->text ? m->text : "playback error"); }
    if (d) *d = nullptr;
}
