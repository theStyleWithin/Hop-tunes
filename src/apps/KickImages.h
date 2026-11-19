// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "apps/WebpDecode.h"
#include "net/AppNet.h"

#include <QImage>
#include <QPointer>
#include <QQuickImageProvider>
#include <QQuickTextureFactory>
#include <QTimer>
#include <QUrl>

namespace hop {

// image://kick/<address>: the Kick app's pictures (stream pictures, avatars, emotes) are fetched through the gateway like everything
// else the app does, not by QML's own network access: same host list, no referrer, no cookies, nothing kept on disk. A picture that
// cannot be loaded or read gives an error, and the screen shows a plain tile instead.
class KickImageResponse : public QQuickImageResponse {
public:
    KickImageResponse(AppNet *net, const QString &id, const QSize &size, const QString &app = QStringLiteral("kick")) : m_size(size)
    {
        QString address = id;
        if (!address.startsWith(QLatin1String("https:"))) address = QUrl::fromPercentEncoding(id.toUtf8());
        const QUrl url(address);
        // AppNet lives in the main thread; ask from there whatever thread this was created in
        QTimer::singleShot(0, net, [this, net, url, app, self = QPointer<KickImageResponse>(this)] {
            if (!self) return;
            net->get(app, url, [self](NetReply r) {
                if (!self) return;
                if (!r.error.isEmpty()) { self->finish(QImage(), r.error); return; }
                QImage img = QImage::fromData(r.body);
                if (img.isNull()) img = decodeWebp(r.body);        // Kick's pictures are WebP: read them here when Qt has no plugin for it
                if (img.isNull()) { self->finish(QImage(), QStringLiteral("the picture could not be read")); return; }
                self->finish(img, QString());
            });
        });
    }
    QQuickTextureFactory *textureFactory() const override { return m_image.isNull() ? nullptr : QQuickTextureFactory::textureFactoryForImage(m_image); }
    QString errorString() const override { return m_error; }
    void cancel() override { m_cancelled = true; }

private:
    void finish(QImage img, const QString &error)
    {
        if (m_cancelled) { m_error = QStringLiteral("cancelled"); }
        else {
            // a size with only a width (or only a height) asked for has a 0 for the other: that side is not limited (it used to shrink the picture to nothing)
            if (!img.isNull() && m_size.width() >= 0 && m_size.height() >= 0 && (m_size.width() > 0 || m_size.height() > 0)) {
                const QSize box(m_size.width() > 0 ? m_size.width() : img.width(), m_size.height() > 0 ? m_size.height() : img.height());
                if (img.width() > box.width() || img.height() > box.height()) img = img.scaled(box, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            m_image = img;
            m_error = error;
        }
        emit finished();
    }
    QSize m_size;
    QImage m_image;
    QString m_error;
    bool m_cancelled = false;
};

class KickImageProvider : public QQuickAsyncImageProvider {
public:
    explicit KickImageProvider(AppNet *net, const QString &app = QStringLiteral("kick")) : m_net(net), m_app(app) {}   // (image://deviantart/ is the same, for that app)
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override
    {
        return new KickImageResponse(m_net, id, requestedSize, m_app);
    }
private:
    AppNet *m_net;
    QString m_app;
};

} // namespace hop
