// SPDX-License-Identifier: GPL-3.0-or-later
// A tiny "playbin" for Android on top of Qt Multimedia. See gst/gst.h.
//
// Every QMediaPlayer lives on one shim-owned audio thread with its own event loop. Qt for Android pauses the
// application's main loop while the app is in the background (screen off), which used to leave a finished song
// waiting for the next one until the screen came back on. Here the "about-to-finish" hand-off runs on the audio
// thread, so the next song starts by itself. All calls from other threads are queued (never blocking), and
// position, duration and the message bus are shared through atomics and a mutex.
#include "gst/gst.h"

#include <QAudioBuffer>
#include <QAudioBufferOutput>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QCoreApplication>
#include <QMediaPlayer>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QThread>
#include <QUrl>

#include "game/SpectrumAnalyzer.h"

#include <atomic>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

namespace {
enum Kind { KElement = 1, KBus = 2, KFactory = 3 };
struct Obj { int kind; };

QThread *g_thread = nullptr;
QObject *g_ctx = nullptr;   // lives on g_thread; the target of every queued call

void ensureThread()
{
    static QMutex m;
    QMutexLocker lock(&m);
    if (g_thread) return;
    g_thread = new QThread;
    g_thread->setObjectName(QStringLiteral("hoptunes-audio"));
    g_ctx = new QObject;
    g_ctx->moveToThread(g_thread);
    g_thread->start();
    if (QCoreApplication *app = QCoreApplication::instance())
        QObject::connect(app, &QCoreApplication::aboutToQuit, app, [] { g_thread->quit(); g_thread->wait(2000); });
}

// Runs f on the audio thread: straight away when already there, queued otherwise (never blocks the caller).
template <class F> void post(F f)
{
    ensureThread();
    if (QThread::currentThread() == g_thread) f();
    else QMetaObject::invokeMethod(g_ctx, std::move(f), Qt::QueuedConnection);
}

using FinishFn = void (*)(GstElement *, void *);
using SpectrumFn = void (*)(GstElement *, const float *, int, void *);
}

struct _GstBus : Obj {
    QMutex mutex;
    QQueue<GstMessage *> q;
    _GstBus() : Obj{KBus} {}
    ~_GstBus() { qDeleteAll(q); }
    void push(GstMessage *m) { QMutexLocker l(&mutex); q.enqueue(m); }
    GstMessage *pop() { QMutexLocker l(&mutex); return q.isEmpty() ? nullptr : q.dequeue(); }
    void clear() { QMutexLocker l(&mutex); qDeleteAll(q); q.clear(); }
};

struct _GstElement : Obj {
    // audio-thread objects (created and used there only)
    QMediaPlayer *player = nullptr;
    QAudioOutput *out = nullptr;
    // shared
    _GstBus *bus = nullptr;
    std::atomic<qint64> pos{0}, dur{0};
    std::atomic<bool> loaded{false};
    std::atomic<bool> detached{false};
    std::atomic<FinishFn> aboutToFinish{nullptr};
    std::atomic<FinishFn> streamStarted{nullptr};
    std::atomic<SpectrumFn> spectrum{nullptr};
    std::atomic<bool> analysis{false};
    std::atomic<bool> tapBroken{false};
    std::atomic<void *> cbData{nullptr};
    // main-thread view
    GstState state = GST_STATE_NULL;
    // audio thread only
    QAudioBufferOutput *tap = nullptr;       // the decoded samples, only while analysis is on
    hop::SpectrumAnalyzer *an = nullptr;
    int tapRate = 0;
    bool inFinish = false;
    bool uriChanged = false;
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

QUrl toQUrl(const QString &s) { return s.contains(QStringLiteral("://")) ? QUrl(s) : QUrl::fromLocalFile(s); }

void setSourceOnAudioThread(_GstElement *e, const QString &uri)
{
    e->loaded = false;
    e->pos = 0;
    e->dur = 0;
    if (e->inFinish) e->uriChanged = true;
    e->player->setSource(uri.isEmpty() ? QUrl() : toQUrl(uri));
}

// Decoded samples of the playing song -> mono -> 32 bands -> the player. Audio thread only.
void onTapBuffer(_GstElement *e, const QAudioBuffer &b)
{
    if (!e->analysis || !b.isValid() || !e->an) return;
    const QAudioFormat f = b.format();
    const int ch = qMax(1, f.channelCount()), frames = b.frameCount();
    if (frames <= 0 || f.sampleRate() <= 0) return;
    if (f.sampleRate() != e->tapRate) { e->tapRate = f.sampleRate(); e->an->setSampleRate(e->tapRate); e->an->reset(); }
    QVector<float> mono(frames);
    auto mix = [&](auto *src, float scale, float bias) {
        for (int i = 0; i < frames; ++i) {
            float sum = 0.f;
            for (int c = 0; c < ch; ++c) sum += (float(src[i * ch + c]) - bias) * scale;
            mono[i] = sum / float(ch);
        }
    };
    switch (f.sampleFormat()) {
    case QAudioFormat::Int16: mix(b.constData<qint16>(), 1.f / 32768.f, 0.f); break;
    case QAudioFormat::Int32: mix(b.constData<qint32>(), 1.f / 2147483648.f, 0.f); break;
    case QAudioFormat::UInt8: mix(b.constData<quint8>(), 1.f / 128.f, 128.f); break;
    case QAudioFormat::Float: mix(b.constData<float>(), 1.f, 0.f); break;
    default: return;
    }
    e->an->feed(mono.constData(), frames);
    QVector<float> bands;
    while (e->an->next(&bands)) {
        if (e->detached) return;
        if (SpectrumFn fn = e->spectrum.load()) fn(e, bands.constData(), int(bands.size()), e->cbData.load());
    }
}

// The song ended on the audio thread: let the player queue pick the next one (gapless hand-off), or report the end.
void handleEnd(_GstElement *e)
{
    e->uriChanged = false;
    e->inFinish = true;
    if (!e->detached) if (FinishFn f = e->aboutToFinish.load()) f(e, e->cbData.load());
    e->inFinish = false;
    if (e->uriChanged) {
        e->player->setPosition(0);   // the same song again (repeat one) starts from the top
        e->player->play();
        if (!e->detached) if (FinishFn f = e->streamStarted.load()) f(e, e->cbData.load());
        e->bus->push(makeMsg(GST_MESSAGE_STREAM_START, e));
    } else {
        e->bus->push(makeMsg(GST_MESSAGE_EOS, e));
    }
}
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
    post([e] {
        e->out = new QAudioOutput;
        e->player = new QMediaPlayer;
        e->player->setAudioOutput(e->out);
        QObject::connect(e->player, &QMediaPlayer::mediaStatusChanged, e->player, [e](QMediaPlayer::MediaStatus st) {
            if (st == QMediaPlayer::LoadedMedia) {
                e->loaded = true;
                e->bus->push(makeMsg(GST_MESSAGE_STREAM_START, e));
                e->bus->push(makeMsg(GST_MESSAGE_ASYNC_DONE, e));
            } else if (st == QMediaPlayer::EndOfMedia) {
                handleEnd(e);
            }
        });
        QObject::connect(e->player, &QMediaPlayer::errorOccurred, e->player, [e](QMediaPlayer::Error, const QString &text) {
            // The sound tap is only a picture for the visualizer and the games: if a song fails while it is attached, take it
            // out and play the same song again without it, instead of losing the music for a picture.
            if (e->tap && e->player->audioBufferOutput() == e->tap && !e->tapBroken) {
                e->tapBroken = true;
                e->analysis = false;
                const QUrl src = e->player->source();
                const qint64 at = e->pos.load();
                e->player->setAudioBufferOutput(nullptr);
                e->player->setSource(src);
                if (at > 0) e->player->setPosition(at);
                e->player->play();
                return;
            }
            e->bus->push(makeMsg(GST_MESSAGE_ERROR, e, text.isEmpty() ? QStringLiteral("playback error") : text));
        });
        QObject::connect(e->player, &QMediaPlayer::positionChanged, e->player, [e](qint64 p) { e->pos = p; });
        QObject::connect(e->player, &QMediaPlayer::durationChanged, e->player, [e](qint64 d) { e->dur = d; });
    });
    return e;
}

unsigned long hop_shim_connect(void *obj, const char *signal, GCallback cb, void *data)
{
    auto *e = static_cast<_GstElement *>(obj);
    if (!e || e->kind != KElement) return 0;
    e->cbData = data;
    if (!std::strcmp(signal, "about-to-finish")) e->aboutToFinish = reinterpret_cast<FinishFn>(cb);
    else if (!std::strcmp(signal, "hop-stream-started")) e->streamStarted = reinterpret_cast<FinishFn>(cb);
    else if (!std::strcmp(signal, "hop-spectrum")) e->spectrum = reinterpret_cast<SpectrumFn>(cb);
    return 1;
}

gboolean hop_shim_analysis_broken(GstElement *e)
{
    return e && e->kind == KElement && e->tapBroken.load();
}

void hop_shim_set_analysis(GstElement *e, gboolean on)
{
    if (!e || e->kind != KElement || e->tapBroken) return;
    e->analysis = on != 0;
    post([e, on] {
        if (!e->player || e->detached) return;
        if (on) {
            if (!e->tap) {
                e->an = new hop::SpectrumAnalyzer;
                e->tap = new QAudioBufferOutput(e->player);
                QObject::connect(e->tap, &QAudioBufferOutput::audioBufferReceived, e->player,
                                 [e](const QAudioBuffer &b) { onTapBuffer(e, b); });
            }
            e->tapRate = 0;
            e->player->setAudioBufferOutput(e->tap);
        } else {
            e->player->setAudioBufferOutput(nullptr);   // no decoding for nothing
        }
    });
}

void hop_shim_disconnect(void *obj, void *)
{
    auto *e = static_cast<_GstElement *>(obj);
    if (!e || e->kind != KElement) return;
    e->detached = true;
    e->aboutToFinish = nullptr;
    e->streamStarted = nullptr;
    e->spectrum = nullptr;
}

GstElement *gst_parse_launch(const char *, GError **) { return nullptr; }
GstElement *gst_parse_bin_from_description(const char *, gboolean, GError **) { return nullptr; }
GstElement *gst_bin_get_by_name(GstBin *, const char *) { return nullptr; }
GstBus *gst_element_get_bus(GstElement *e) { return e ? e->bus : nullptr; }
void *gst_object_ref_sink(void *o) { return o; }

void gst_object_unref(void *o)
{
    if (!o) return;
    if (static_cast<Obj *>(o)->kind != KElement) return;   // the bus belongs to its element; the factory is a sentinel
    auto *e = static_cast<_GstElement *>(o);
    e->detached = true;
    post([e] {   // after everything already queued for it
        delete e->player;
        delete e->an;
        delete e->out;
        delete e->bus;
        delete e;
    });
}

void g_object_set(void *obj, const char *first, ...)
{
    auto *e = static_cast<_GstElement *>(obj);
    va_list ap;
    va_start(ap, first);
    for (const char *k = first; k; k = va_arg(ap, const char *)) {
        if (!std::strcmp(k, "uri")) {
            const QString u = QString::fromUtf8(va_arg(ap, const char *));
            if (e) post([e, u] { if (e->player) setSourceOnAudioThread(e, u); });
        } else if (!std::strcmp(k, "volume")) {
            const double v = va_arg(ap, double);
            if (e) post([e, v] { if (e->out) e->out->setVolume(float(qBound(0.0, v, 1.0))); });
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
    post([e, s] {
        if (!e->player) return;
        switch (s) {
        case GST_STATE_PLAYING: e->player->play(); break;
        case GST_STATE_PAUSED: e->player->pause(); break;
        default: e->player->stop(); break;
        }
    });
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
    const qint64 ms = ns / GST_MSECOND;
    e->pos = ms;
    post([e, ms] { if (e->player) e->player->setPosition(ms); });
    return TRUE;
}
gboolean gst_element_query_position(GstElement *e, GstFormat, gint64 *ns)
{
    if (!e || !e->loaded) return FALSE;
    *ns = e->pos.load() * GST_MSECOND;
    return TRUE;
}
gboolean gst_element_query_duration(GstElement *e, GstFormat, gint64 *ns)
{
    if (!e || !e->loaded) return FALSE;
    *ns = e->dur.load() * GST_MSECOND;
    return TRUE;
}

GstMessage *gst_bus_pop(GstBus *b) { return b ? b->pop() : nullptr; }
void gst_bus_set_flushing(GstBus *b, gboolean flush) { if (b && flush) b->clear(); }
void gst_message_unref(GstMessage *m) { if (m) { std::free(m->text); delete m; } }
void gst_message_parse_error(GstMessage *m, GError **e, gchar **d)
{
    if (e) { *e = new GError; (*e)->message = strdup(m && m->text ? m->text : "playback error"); }
    if (d) *d = nullptr;
}
