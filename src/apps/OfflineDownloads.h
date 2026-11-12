// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "apps/Hls.h"
#include "apps/OfflineStore.h"
#include "net/AppNet.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <deque>

namespace hop {

// Downloads of finished Kick streams and clips, kept inside Hop-tunes' own offline folder (see OfflineStore): one folder for each item
// with the video joined into one file (video.ts), a picture and a small meta.json. Nothing is shown as a loose .mp4 and nothing is sent
// anywhere. Everything is fetched through the gateway, one piece at a time, only while the Kick app is open (leaving it pauses the
// download; opening it again carries on). A download never goes over the storage limit: it stops with a plain reason and waits.
class OfflineDownloads : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)             // something is downloading now

public:
    OfflineDownloads(AppNet *net, OfflineStore *store, QObject *parent = nullptr);

    static constexpr qint64 kMaxPart = 32 * 1024 * 1024;       // one piece of video
    static constexpr int kPartTimeoutMs = 60000;
    static constexpr int kRetries = 3;

    // guests keep nothing: nothing can be downloaded without a profile
    void setEnabled(bool on);
    // look in the offline folder for what is already there (after a profile switch, a drive change, or at start)
    Q_INVOKABLE void reload();
    // wait between retries (tests make it short)
    void setRetryDelayMs(int ms) { m_retryMs = ms; }

    QVariantList items() const;
    int count() const { return m_items.size(); }
    bool busy() const;
    Q_INVOKABLE QVariantMap item(const QString &id) const;
    Q_INVOKABLE bool has(const QString &id) const { return m_items.contains(id); }
    Q_INVOKABLE QString videoPath(const QString &id) const;      // the joined video, "" until there is something to play
    Q_INVOKABLE QString thumbPath(const QString &id) const;

    // info: id, kind ("stream" | "clip"), title, channel, thumb (address), url (a master playlist for a stream, a media playlist for a
    // clip), maxHeight (0 = best). "" = started or queued, otherwise one plain sentence.
    Q_INVOKABLE QString enqueue(const QVariantMap &info);
    Q_INVOKABLE void pause(const QString &id);
    Q_INVOKABLE void resume(const QString &id);
    Q_INVOKABLE void remove(const QString &id);                  // cancels if running; deletes what was downloaded

public slots:
    void appOpened();     // the Kick app opened: carry on with what was waiting for it
    void appClosed();     // it closed: whatever was running waits

signals:
    void changed();
    void finished(const QString &id);

private:
    struct Item {
        QString id, kind, title, channel, thumb, url, quality, state = QStringLiteral("queued"), error;
        int maxHeight = 0;
        QList<HlsPart> parts;
        int done = 0;
        qint64 bytes = 0, estimate = 0;
        double seconds = 0;
        bool userPaused = false;
    };
    QString dirOf(const QString &id) const;
    QString metaPath(const QString &id) const { return dirOf(id) + QStringLiteral("/meta.json"); }
    void save(const Item &it) const;
    bool loadItem(const QString &id);
    void next();                                      // pick the next queued item and run it
    void plan(const QString &id);                     // fetch the playlists
    void fetchPart(const QString &id, int attempt);
    void fail(const QString &id, const QString &why, const QString &state = QStringLiteral("failed"));
    void fetchThumb(const QString &id);
    void setState(Item &it, const QString &state, const QString &error = QString());

    AppNet *m_net;
    OfflineStore *m_store;
    QHash<QString, Item> m_items;
    QStringList m_order;                              // newest first
    QString m_running;
    quint64 m_gen = 0;                                // a stale answer (after pause, remove, close) is ignored
    bool m_enabled = true;
    int m_retryMs = 1500;
};

} // namespace hop
