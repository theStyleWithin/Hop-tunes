// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/MpvVideoItem.h"

#include <QCoreApplication>
#include <QMutex>
#include <QOpenGLFramebufferObject>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QRunnable>

#ifdef HOPTUNES_HAVE_MPV
#include <QOpenGLContext>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#endif

namespace hop {

#ifdef HOPTUNES_HAVE_MPV

// ---- the actual GL glue: render-thread only, follows mpv's documented render API -------
//
// Lifetime (this was a real crash: closing a video that is not the whole window, such as the small stream window, aborted inside
// libmpv): mpv requires mpv_render_context_free() BEFORE the handle is destroyed (mpv_terminate_destroy aborts otherwise), and the
// context is freed with the GL context current, so on the render thread. The item (GUI thread) can go while the window lives on, so
// the context is owned by a RenderState shared by the item, the renderer and a render job:
//  * the renderer frees it when the scene graph destroys it (a window going away),
//  * a dying item that has a context hands its MpvCore to the state and schedules a render job that frees the context on the render
//    thread; the core is deleted (on the GUI thread, queued) only after that, and the item's own address is never used again.
// An item that never drew a frame has no context: its core simply goes with it.
struct MpvVideoItem::RenderState {
    QMutex m;
    mpv_render_context *ctx = nullptr;
    QQuickItem *item = nullptr;        // for update(); cleared when the item dies
    MpvCore *pendingCore = nullptr;    // an item that died with a live context leaves its core here
    // render thread, GL context current. (mpv is never called with the lock held: its update callback takes the lock too.)
    void freeContext()
    {
        mpv_render_context *c = nullptr;
        MpvCore *dead = nullptr;
        {
            QMutexLocker l(&m);
            c = ctx; ctx = nullptr;
            dead = pendingCore; pendingCore = nullptr;
        }
        if (c) {
            mpv_render_context_set_update_callback(c, nullptr, nullptr);
            mpv_render_context_free(c);
        }
        if (!dead) return;
        if (QCoreApplication *app = QCoreApplication::instance())
            QMetaObject::invokeMethod(app, [dead] { delete dead; }, Qt::QueuedConnection);   // mpv's own teardown on the GUI thread, after the context
        else
            delete dead;
    }
};

namespace {
class FreeContextJob : public QRunnable {
public:
    explicit FreeContextJob(std::shared_ptr<MpvVideoItem::RenderState> s) : m_state(std::move(s)) {}
    void run() override { m_state->freeContext(); }
private:
    std::shared_ptr<MpvVideoItem::RenderState> m_state;
};
}

class MpvVideoItem::FboRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit FboRenderer(std::shared_ptr<RenderState> state) : m_state(std::move(state)) {}
    ~FboRenderer() override { m_state->freeContext(); }

protected:
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        ensureContext();
        return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
    }

    void render() override
    {
        if (!ensureContext()) return;
        QOpenGLFramebufferObject *fbo = framebufferObject();
        mpv_opengl_fbo target{int(fbo->handle()), fbo->width(), fbo->height(), 0};
        // 0, not 1: confirmed on real hardware (Steam Deck) that 1 renders upside down.
        // Matches mpv's own Qt Quick render-API example (qthelloworld), which uses 0 for
        // exactly this QQuickFramebufferObject integration — Qt Quick's FBO texture is
        // already in the orientation mpv expects, so no flip is needed here.
        int flip = 0;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &target},
            {MPV_RENDER_PARAM_FLIP_Y, &flip},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        mpv_render_context *c = nullptr;
        { QMutexLocker l(&m_state->m); c = m_state->ctx; }       // (only this thread ever frees it)
        if (c) mpv_render_context_render(c, params);
    }

private:
    bool ensureContext()
    {
        mpv_handle *h = nullptr;
        {
            QMutexLocker l(&m_state->m);
            if (m_state->ctx) return true;
            if (!m_state->item) return false;            // the item is gone: no new context
            h = static_cast<MpvVideoItem *>(m_state->item)->core()->handle();
        }
        if (!h) return false;
        mpv_opengl_init_params gl{&getProcAddress, nullptr};
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        mpv_render_context *c = nullptr;
        if (mpv_render_context_create(&c, h, params) < 0) return false;
        { QMutexLocker l(&m_state->m); m_state->ctx = c; }
        mpv_render_context_set_update_callback(c, &FboRenderer::onUpdate, m_state.get());   // (may call back at once: the lock is not held)
        return true;
    }

    static void *getProcAddress(void *, const char *name)
    {
        QOpenGLContext *ctx = QOpenGLContext::currentContext();
        return ctx ? reinterpret_cast<void *>(ctx->getProcAddress(name)) : nullptr;
    }

    // Called by mpv, possibly from a thread Qt doesn't know about — must only ever
    // queue work onto the item, never touch GL state directly (and never a dead item).
    static void onUpdate(void *userdata)
    {
        auto *s = static_cast<RenderState *>(userdata);
        QMutexLocker l(&s->m);
        if (!s->item) return;
        QQuickItem *item = s->item;
        QMetaObject::invokeMethod(item, [item] { item->update(); }, Qt::QueuedConnection);   // (dropped if the item is gone by then)
    }

    std::shared_ptr<RenderState> m_state;
};

#else   // !HOPTUNES_HAVE_MPV

// No libmpv at build time (see MpvCore.cpp's stub and CMakeLists.txt): nothing to render.
class MpvVideoItem::FboRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit FboRenderer(std::shared_ptr<RenderState>) {}
protected:
    void render() override {}
};

#endif   // HOPTUNES_HAVE_MPV

// ---- the QML-visible item: GUI thread, all playback state lives in MpvCore -------------

MpvVideoItem::MpvVideoItem(QQuickItem *parent) : QQuickFramebufferObject(parent), m_core(std::make_unique<MpvCore>()), m_state(std::make_shared<RenderState>())
{
    m_state->item = this;
    m_core->setOption(QStringLiteral("vo"), QStringLiteral("libmpv"));
    // Hardware decode with an automatic, safe fallback to software (spec §5b targets).
    m_core->setOption(QStringLiteral("hwdec"), QStringLiteral("auto-safe"));
    m_core->init();

    connect(m_core.get(), &MpvCore::positionChanged, this, &MpvVideoItem::positionChanged);
    connect(m_core.get(), &MpvCore::durationChanged, this, &MpvVideoItem::durationChanged);
    connect(m_core.get(), &MpvCore::pausedChanged, this, &MpvVideoItem::pausedChanged);
    connect(m_core.get(), &MpvCore::subtitleTracksChanged, this, &MpvVideoItem::subtitleTracksChanged);
    connect(m_core.get(), &MpvCore::errorChanged, this, &MpvVideoItem::errorChanged);
    connect(m_core.get(), &MpvCore::fileLoaded, this, &MpvVideoItem::idleChanged);
    connect(m_core.get(), &MpvCore::endOfFile, this, &MpvVideoItem::idleChanged);
    connect(m_core.get(), &MpvCore::fileLoaded, this, [this] { emit hwdecActiveChanged(); });
}

MpvVideoItem::~MpvVideoItem()
{
#ifdef HOPTUNES_HAVE_MPV
    bool handOver = false;
    {
        QMutexLocker l(&m_state->m);
        m_state->item = nullptr;                      // nothing may reach this item from now on
        if (m_state->ctx) { m_state->pendingCore = m_core.release(); handOver = true; }   // the context goes first, then mpv (on the render thread, see the note above)
    }
    if (handOver) {
        if (QQuickWindow *w = window()) {
            w->scheduleRenderJob(new FreeContextJob(m_state), QQuickWindow::BeforeSynchronizingStage);
            w->update();
        }
        // (without a window the scene graph's own teardown frees the renderer and with it the context)
    }
#endif
}

QQuickFramebufferObject::Renderer *MpvVideoItem::createRenderer() const
{
    return new FboRenderer(m_state);
}

void MpvVideoItem::setSource(const QString &path)
{
    if (path == m_source) return;
    m_source = path;
    emit sourceChanged();
    if (path.isEmpty()) { m_core->stop(); return; }
    m_core->open(path, m_pendingResumeMs);
    m_pendingResumeMs = 0;
}

void registerVideoQmlTypes()
{
    qmlRegisterType<MpvVideoItem>("HopTunes.Video", 1, 0, "VideoSurface");
}

} // namespace hop
