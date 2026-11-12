// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

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
    KickImageResponse(AppNet *net, const QString &id, const QSize &size) : m_size(size)
    {
        QString address = id;
        if (!address.startsWith(QLatin1String("https:"))) address = QUrl::fromPercentEncoding(id.toUtf8());
        const QUrl url(address);
        // AppNet lives in the main thread; ask from there whatever thread this was created in
        QTimer::singleShot(0, net, [this, net, url, self = QPointer<KickImageResponse>(this)] {
            if (!self) return;
            net->get(QStringLiteral("kick"), url, [self](NetReply r) {
                if (!self) return;
                if (!r.error.isEmpty()) { self->finish(QImage(), r.error); return; }
                QImage img = QImage::fromData(r.body);
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
            if (!img.isNull() && m_size.isValid() && (img.width() > m_size.width() || img.height() > m_size.height()))
                img = img.scaled(m_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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
    explicit KickImageProvider(AppNet *net) : m_net(net) {}
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override
    {
        return new KickImageResponse(m_net, id, requestedSize);
    }
private:
    AppNet *m_net;
};

} // namespace hop
