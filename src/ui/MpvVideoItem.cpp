// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/MpvVideoItem.h"

#include <QOpenGLFramebufferObject>
#include <QQmlEngine>

#ifdef HOPTUNES_HAVE_MPV
#include <QOpenGLContext>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#endif

namespace hop {

#ifdef HOPTUNES_HAVE_MPV

// ---- the actual GL glue: render-thread only, follows mpv's documented render API -------
//
// Lifetime note (not exercised by any test — this sandbox has no on-screen GL context to
// verify it against): mpv requires mpv_render_context_free() before the handle is
// destroyed. MpvVideoItem's MpvCore is destroyed with the item (GUI thread); this Renderer
// is destroyed by the scene graph (render thread), normally as part of tearing the item's
// window down before the item itself goes away. If that ordering is ever violated — e.g. a
// video item outliving its window in some embedding this app doesn't do — mpv_terminate_
// destroy() could race a still-alive render context. Flagging this rather than guessing at
// a cross-thread teardown protocol that can't be tested here.
class MpvVideoItem::FboRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit FboRenderer(MpvVideoItem *item) : m_item(item) {}
    ~FboRenderer() override { if (m_ctx) mpv_render_context_free(m_ctx); }

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
        mpv_render_context_render(m_ctx, params);
    }

private:
    bool ensureContext()
    {
        if (m_ctx) return true;
        mpv_handle *h = m_item->core()->handle();
        if (!h) return false;
        mpv_opengl_init_params gl{&getProcAddress, nullptr};
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        if (mpv_render_context_create(&m_ctx, h, params) < 0) return false;
        mpv_render_context_set_update_callback(m_ctx, &FboRenderer::onUpdate, this);
        return true;
    }

    static void *getProcAddress(void *, const char *name)
    {
        QOpenGLContext *ctx = QOpenGLContext::currentContext();
        return ctx ? reinterpret_cast<void *>(ctx->getProcAddress(name)) : nullptr;
    }

    // Called by mpv, possibly from a thread Qt doesn't know about — must only ever
    // queue work onto the item, never touch GL state directly.
    static void onUpdate(void *ctx)
    {
        auto *self = static_cast<FboRenderer *>(ctx);
        QQuickFramebufferObject *item = self->m_item;
        QMetaObject::invokeMethod(item, [item] { item->update(); }, Qt::QueuedConnection);
    }

    MpvVideoItem *m_item;
    mpv_render_context *m_ctx = nullptr;
};

#else   // !HOPTUNES_HAVE_MPV

// No libmpv at build time (see MpvCore.cpp's stub and CMakeLists.txt): nothing to render.
class MpvVideoItem::FboRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit FboRenderer(MpvVideoItem *) {}
protected:
    void render() override {}
};

#endif   // HOPTUNES_HAVE_MPV

// ---- the QML-visible item: GUI thread, all playback state lives in MpvCore -------------

MpvVideoItem::MpvVideoItem(QQuickItem *parent) : QQuickFramebufferObject(parent)
{
    m_core.setOption(QStringLiteral("vo"), QStringLiteral("libmpv"));
    // Hardware decode with an automatic, safe fallback to software (spec §5b targets).
    m_core.setOption(QStringLiteral("hwdec"), QStringLiteral("auto-safe"));
    m_core.init();

    connect(&m_core, &MpvCore::positionChanged, this, &MpvVideoItem::positionChanged);
    connect(&m_core, &MpvCore::durationChanged, this, &MpvVideoItem::durationChanged);
    connect(&m_core, &MpvCore::pausedChanged, this, &MpvVideoItem::pausedChanged);
    connect(&m_core, &MpvCore::subtitleTracksChanged, this, &MpvVideoItem::subtitleTracksChanged);
    connect(&m_core, &MpvCore::errorChanged, this, &MpvVideoItem::errorChanged);
    connect(&m_core, &MpvCore::fileLoaded, this, &MpvVideoItem::idleChanged);
    connect(&m_core, &MpvCore::endOfFile, this, &MpvVideoItem::idleChanged);
    connect(&m_core, &MpvCore::fileLoaded, this, [this] { emit hwdecActiveChanged(); });
}

MpvVideoItem::~MpvVideoItem() = default;

QQuickFramebufferObject::Renderer *MpvVideoItem::createRenderer() const
{
    return new FboRenderer(const_cast<MpvVideoItem *>(this));
}

void MpvVideoItem::setSource(const QString &path)
{
    if (path == m_source) return;
    m_source = path;
    emit sourceChanged();
    if (path.isEmpty()) { m_core.stop(); return; }
    m_core.open(path, m_pendingResumeMs);
    m_pendingResumeMs = 0;
}

void registerVideoQmlTypes()
{
    qmlRegisterType<MpvVideoItem>("HopTunes.Video", 1, 0, "VideoSurface");
}

} // namespace hop
