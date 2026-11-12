// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps/OfflineDownloads.h"
#include "apps/WebpDecode.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>

namespace hop {

namespace {
const QString kApp = QStringLiteral("kick");
QString cleanId(const QString &id)
{
    QString c;
    for (QChar ch : id) c += (ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_')) ? ch : QLatin1Char('_');
    return c;
}
} // namespace

OfflineDownloads::OfflineDownloads(AppNet *net, OfflineStore *store, QObject *parent) : QObject(parent), m_net(net), m_store(store) {}

void OfflineDownloads::setEnabled(bool on)
{
    m_enabled = on;
    if (!on) { ++m_gen; m_running.clear(); m_items.clear(); m_order.clear(); emit changed(); }
}

QString OfflineDownloads::dirOf(const QString &id) const { return QDir(m_store->folder()).filePath(cleanId(id)); }
QString OfflineDownloads::videoPath(const QString &id) const
{
    const auto it = m_items.constFind(id);
    if (it == m_items.constEnd() || it->done == 0) return {};
    return dirOf(id) + QStringLiteral("/video.ts");
}
QString OfflineDownloads::thumbPath(const QString &id) const
{
    const QString p = dirOf(id) + QStringLiteral("/thumb.jpg");
    return QFileInfo::exists(p) ? p : QString();
}

bool OfflineDownloads::busy() const { return !m_running.isEmpty() || !m_recording.isEmpty(); }

QVariantMap OfflineDownloads::item(const QString &id) const
{
    const auto f = m_items.constFind(id);
    if (f == m_items.constEnd()) return {};
    const Item &it = *f;
    const double total = it.parts.size();
    QVariantMap m{{"id", it.id}, {"kind", it.kind}, {"title", it.title}, {"channel", it.channel}, {"quality", it.quality},
                  {"state", it.state}, {"error", it.error}, {"bytes", it.bytes}, {"estimate", it.estimate}, {"seconds", it.seconds},
                  {"doneParts", it.done}, {"totalParts", it.parts.size()}, {"progress", total > 0 ? it.done / total : 0.0},
                  {"thumb", thumbPath(it.id)}};
    return m;
}

QVariantList OfflineDownloads::items() const
{
    QVariantList l;
    for (const QString &id : m_order) l << item(id);
    return l;
}

void OfflineDownloads::save(const Item &it) const
{
    QDir().mkpath(dirOf(it.id));
    QJsonArray parts;
    for (const HlsPart &p : it.parts) parts.append(QJsonObject{{"u", p.url.toString()}, {"s", p.seconds}, {"b", double(p.bytes)}, {"r", double(p.rangeStart)}});
    QJsonObject o{{"id", it.id}, {"kind", it.kind}, {"title", it.title}, {"channel", it.channel}, {"thumb", it.thumb}, {"url", it.url},
                  {"quality", it.quality}, {"maxHeight", it.maxHeight}, {"done", it.done}, {"bytes", double(it.bytes)},
                  {"estimate", double(it.estimate)}, {"seconds", it.seconds}, {"userPaused", it.userPaused}, {"state", it.state},
                  {"error", it.error}, {"parts", parts}};
    QFile f(metaPath(it.id));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool OfflineDownloads::loadItem(const QString &id)
{
    QFile f(metaPath(id));
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o.value("id").toString().isEmpty()) return false;
    Item it;
    it.id = o.value("id").toString();
    it.kind = o.value("kind").toString();     // stream | clip | recording
    it.title = o.value("title").toString();
    it.channel = o.value("channel").toString();
    it.thumb = o.value("thumb").toString();
    it.url = o.value("url").toString();
    it.quality = o.value("quality").toString();
    it.maxHeight = o.value("maxHeight").toInt();
    it.done = o.value("done").toInt();
    it.bytes = qint64(o.value("bytes").toDouble());
    it.estimate = qint64(o.value("estimate").toDouble());
    it.seconds = o.value("seconds").toDouble();
    it.userPaused = o.value("userPaused").toBool();
    for (const QJsonValue &v : o.value("parts").toArray()) {
        const QJsonObject p = v.toObject();
        HlsPart part;
        part.url = QUrl(p.value("u").toString());
        part.seconds = p.value("s").toDouble();
        part.bytes = qint64(p.value("b").toDouble(-1));
        part.rangeStart = qint64(p.value("r").toDouble(-1));
        it.parts << part;
    }
    QString st = o.value("state").toString();
    if (st == QLatin1String("recording")) { st = QStringLiteral("done"); it.error = QStringLiteral("ended when hop-tunes closed"); }
    // something that was running when the app closed is only waiting; a finished one stays finished
    it.state = st == QLatin1String("done") ? st : (it.userPaused ? QStringLiteral("paused") : QStringLiteral("waiting"));
    if (it.state == QLatin1String("waiting")) it.error = QStringLiteral("waiting for the kick app to be open");
    else if (it.state != QLatin1String("done")) it.error.clear();
    if (it.state == QLatin1String("done") && it.kind != QLatin1String("recording") && !it.parts.isEmpty() && it.done < it.parts.size()) it.state = QStringLiteral("waiting");
    m_items.insert(it.id, it);
    m_order.append(it.id);
    return true;
}

void OfflineDownloads::reload()
{
    ++m_gen;
    ++m_recGen;
    m_recording.clear();
    m_running.clear();
    m_items.clear();
    m_order.clear();
    if (m_enabled && m_store->available()) {
        const QDir base(m_store->folder());
        QStringList ids = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
        for (const QString &d : ids) loadItem(d);
    }
    emit changed();
    if (m_net->isOpen(kApp)) next();
}

void OfflineDownloads::setState(Item &it, const QString &state, const QString &error)
{
    it.state = state;
    it.error = error;
}

void OfflineDownloads::fail(const QString &id, const QString &why, const QString &state)
{
    auto f = m_items.find(id);
    if (f == m_items.end()) return;
    ++m_gen;
    setState(*f, state, why);
    save(*f);
    if (m_running == id) m_running.clear();
    m_store->refresh();
    emit changed();
    QTimer::singleShot(0, this, [this] { next(); });
}

QString OfflineDownloads::enqueue(const QVariantMap &info)
{
    if (!m_enabled) return QStringLiteral("downloads are kept in a profile: guests keep nothing");
    if (!m_store->available()) return QStringLiteral("the place you chose for offline kick is not available right now");
    const QString id = cleanId(info.value("id").toString());
    const QUrl url(info.value("url").toString());
    QString why;
    if (id.isEmpty() || !url.isValid() || !AppNet::urlAllowed(kApp, url, &why)) return why.isEmpty() ? QStringLiteral("that cannot be downloaded") : why;
    if (m_items.contains(id)) {
        const QString st = m_items[id].state;
        if (st == QLatin1String("done")) return QStringLiteral("you already have this one");
        if (st != QLatin1String("failed")) return QStringLiteral("this one is already in your downloads");
        remove(id);                                                  // a failed one starts over
    }
    if (m_store->full()) return QStringLiteral("your offline storage is full: raise the limit in settings or delete something");
    Item it;
    it.id = id;
    it.kind = info.value("kind").toString() == QLatin1String("clip") ? QStringLiteral("clip") : QStringLiteral("stream");
    it.title = info.value("title").toString();
    it.channel = info.value("channel").toString();
    it.thumb = info.value("thumb").toString();
    it.url = url.toString();
    it.maxHeight = info.value("maxHeight").toInt();
    it.estimate = qint64(info.value("estimate").toDouble());
    it.seconds = info.value("seconds").toDouble();
    m_items.insert(id, it);
    m_order.prepend(id);
    save(m_items[id]);
    emit changed();
    if (m_net->isOpen(kApp)) next();
    else { setState(m_items[id], QStringLiteral("waiting"), QStringLiteral("waiting for the kick app to be open")); emit changed(); }
    return {};
}

void OfflineDownloads::next()
{
    if (!m_running.isEmpty() || !m_net->isOpen(kApp) || !m_enabled) return;
    for (const QString &id : m_order) {
        Item &it = m_items[id];
        if (it.kind != QLatin1String("recording") && (it.state == QLatin1String("queued") || it.state == QLatin1String("waiting"))) {
            m_running = id;
            setState(it, QStringLiteral("running"));
            emit changed();
            if (it.parts.isEmpty()) plan(id); else fetchPart(id, 0);
            return;
        }
    }
}

void OfflineDownloads::plan(const QString &id)
{
    const quint64 gen = ++m_gen;
    QPointer<OfflineDownloads> self(this);
    const QUrl url(m_items[id].url);
    const bool isStream = m_items[id].kind == QLatin1String("stream");
    m_net->get(kApp, url, [self, id, gen, url, isStream](NetReply r) {
        if (!self || self->m_gen != gen || !self->m_items.contains(id)) return;
        if (!r.error.isEmpty()) { self->fail(id, QStringLiteral("could not get the video's playlist: ") + r.error); return; }
        QUrl mediaUrl = url;
        QString text = QString::fromUtf8(r.body);
        auto finish = [self, id, gen](const QString &mediaText, const QUrl &mediaBase, const QString &quality, qint64 bandwidth) {
            if (!self || self->m_gen != gen || !self->m_items.contains(id)) return;
            const HlsMedia m = parseHlsMedia(mediaText, mediaBase);
            Item &it = self->m_items[id];
            if (!m.unsupported.isEmpty()) { self->fail(id, m.unsupported); return; }
            if (!m.finished) { self->fail(id, QStringLiteral("this video is not finished yet: use record on a live stream")); return; }
            it.parts = m.parts;
            it.seconds = m.seconds;
            if (!quality.isEmpty()) it.quality = quality;
            qint64 known = 0; bool all = true;
            for (const HlsPart &p : m.parts) { if (p.bytes >= 0) known += p.bytes; else all = false; }
            it.estimate = all ? known : (bandwidth > 0 ? qint64(bandwidth / 8.0 * m.seconds) : it.estimate);
            QString why;
            if (!self->m_store->canStore(it.estimate, &why)) { self->fail(id, why, QStringLiteral("limit")); return; }
            self->save(it);
            self->fetchThumb(id);
            emit self->changed();
            self->fetchPart(id, 0);
        };
        if (isStream) {
            const QList<HlsVariant> vs = parseHlsMaster(text, url);
            const int maxH = self->m_items[id].maxHeight;
            const HlsVariant *v = chooseHlsVariant(vs, maxH);
            if (!v) {                                                 // not a master: maybe it already is a media playlist
                finish(text, url, QString(), 0);
                return;
            }
            const HlsVariant pick = *v;
            QString why;
            if (!AppNet::urlAllowed(kApp, pick.url, &why)) { self->fail(id, why); return; }
            self->m_net->get(kApp, pick.url, [self, id, gen, pick, finish](NetReply r2) {
                if (!self || self->m_gen != gen || !self->m_items.contains(id)) return;
                if (!r2.error.isEmpty()) { self->fail(id, QStringLiteral("could not get the video's playlist: ") + r2.error); return; }
                finish(QString::fromUtf8(r2.body), pick.url, pick.name, pick.bandwidth);
            });
        } else {
            finish(text, url, QString(), 0);
        }
    });
}

void OfflineDownloads::fetchThumb(const QString &id)
{
    const QUrl url(m_items[id].thumb);
    QString why;
    if (!url.isValid() || !AppNet::urlAllowed(kApp, url, &why)) return;
    QPointer<OfflineDownloads> self(this);
    m_net->get(kApp, url, [self, id](NetReply r) {
        if (!self || !self->m_items.contains(id) || !r.error.isEmpty()) return;
        QImage img = QImage::fromData(r.body);
        if (img.isNull()) img = decodeWebp(r.body);
        if (img.isNull()) return;
        if (img.width() > 640) img = img.scaledToWidth(640, Qt::SmoothTransformation);
        QDir().mkpath(self->dirOf(id));
        img.convertToFormat(QImage::Format_RGB32).save(self->dirOf(id) + QStringLiteral("/thumb.jpg"), "JPG", 85);
        emit self->changed();
    });
}

void OfflineDownloads::fetchPart(const QString &id, int attempt)
{
    auto f = m_items.find(id);
    if (f == m_items.end() || m_running != id) return;
    Item &it = *f;
    if (it.done >= it.parts.size()) {                                // all of it is here
        setState(it, QStringLiteral("done"));
        save(it);
        m_running.clear();
        m_store->refresh();
        emit changed();
        emit finished(id);
        QTimer::singleShot(0, this, [this] { next(); });
        return;
    }
    if (it.done % 8 == 0) m_store->refresh();
    QString why;
    if (!m_store->canStore(0, &why) || m_store->full()) { fail(id, why.isEmpty() ? QStringLiteral("your offline storage is full") : why, QStringLiteral("limit")); return; }
    const quint64 gen = ++m_gen;
    const HlsPart part = it.parts[it.done];
    QPointer<OfflineDownloads> self(this);
    NetLimits lim;
    lim.maxBody = kMaxPart;
    lim.timeoutMs = kPartTimeoutMs;
    lim.rangeStart = part.rangeStart;
    lim.rangeLength = part.bytes;
    m_net->get(kApp, part.url, [self, id, gen, attempt, expect = part.rangeStart >= 0 ? part.bytes : qint64(-1)](NetReply r) {
        if (!self || self->m_gen != gen || !self->m_items.contains(id) || self->m_running != id) return;
        Item &it = self->m_items[id];
        if (r.error.isEmpty() && expect >= 0 && r.body.size() != expect) r.error = QStringLiteral("a piece of the video arrived the wrong size");
        if (!r.error.isEmpty() || r.body.isEmpty()) {
            if (attempt + 1 < kRetries) {
                QTimer::singleShot(self->m_retryMs, self, [self, id, attempt] { if (self) self->fetchPart(id, attempt + 1); });
                return;
            }
            self->fail(id, r.error.isEmpty() ? QStringLiteral("an empty piece came back") : r.error);
            return;
        }
        // the new piece goes on the end of the joined file; a half-written earlier piece is cut off first
        const QString path = self->dirOf(id) + QStringLiteral("/video.ts");
        QDir().mkpath(self->dirOf(id));
        QFile out(path);
        if (!out.open(QIODevice::ReadWrite)) { self->fail(id, QStringLiteral("could not write to the offline folder")); return; }
        out.resize(it.bytes);
        out.seek(it.bytes);
        if (out.write(r.body) != r.body.size()) { out.close(); self->fail(id, QStringLiteral("the drive is full or cannot be written to"), QStringLiteral("limit")); return; }
        out.close();
        it.bytes += r.body.size();
        it.done += 1;
        if (it.done % 5 == 0 || it.done == it.parts.size()) self->save(it);
        emit self->changed();
        QTimer::singleShot(0, self, [self, id] { if (self) self->fetchPart(id, 0); });
    }, lim);
}

void OfflineDownloads::pause(const QString &id)
{
    auto f = m_items.find(id);
    if (f == m_items.end() || f->state == QLatin1String("done") || f->kind == QLatin1String("recording")) return;     // (a recording is stopped, not paused)
    ++m_gen;
    f->userPaused = true;
    setState(*f, QStringLiteral("paused"));
    save(*f);
    if (m_running == id) m_running.clear();
    emit changed();
    QTimer::singleShot(0, this, [this] { next(); });
}

void OfflineDownloads::resume(const QString &id)
{
    auto f = m_items.find(id);
    if (f == m_items.end() || f->kind == QLatin1String("recording") || f->state == QLatin1String("done") || f->state == QLatin1String("running")) return;
    f->userPaused = false;
    setState(*f, QStringLiteral("queued"));
    save(*f);
    emit changed();
    next();
}

void OfflineDownloads::remove(const QString &id)
{
    if (!m_items.contains(id)) return;
    ++m_gen;
    if (m_running == id) m_running.clear();
    if (m_recording == id) { ++m_recGen; m_recording.clear(); }
    const QString dir = dirOf(id);
    m_items.remove(id);
    m_order.removeAll(id);
    if (QFileInfo(dir).isDir() && QFileInfo(dir).absoluteFilePath().startsWith(QFileInfo(m_store->folder()).absoluteFilePath() + QLatin1Char('/')))
        QDir(dir).removeRecursively();                              // only ever inside the offline folder
    m_store->refresh();
    emit changed();
    QTimer::singleShot(0, this, [this] { next(); });
}

void OfflineDownloads::appOpened()
{
    reload();      // picks up what is on disk (and carries on with whatever was waiting)
}

void OfflineDownloads::appClosed()
{
    if (!m_recording.isEmpty()) finishRecording(m_recording, QStringLiteral("ended when the kick app closed"));
    ++m_gen;                                                        // an answer still on the way is ignored
    m_running.clear();
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (it->state == QLatin1String("running") || it->state == QLatin1String("queued")) {
            setState(*it, QStringLiteral("waiting"), QStringLiteral("waiting for the kick app to be open"));
            save(*it);
        }
    }
    emit changed();
}

// ---- recording a live stream ------------------------------------------------------------------------------

QString OfflineDownloads::startRecording(const QVariantMap &info)
{
    if (!m_enabled) return QStringLiteral("recordings are kept in a profile: guests keep nothing");
    if (!m_net->isOpen(kApp)) return QStringLiteral("open the kick app first: recording only runs while it is open");
    if (!m_recording.isEmpty()) return QStringLiteral("you are already recording a stream: stop that one first");
    if (!m_store->available()) return QStringLiteral("the place you chose for offline kick is not available right now");
    const QString id = cleanId(info.value("id").toString());
    const QUrl url(info.value("url").toString());
    QString why;
    if (id.isEmpty() || !url.isValid() || !AppNet::urlAllowed(kApp, url, &why)) return why.isEmpty() ? QStringLiteral("that cannot be recorded") : why;
    if (m_items.contains(id)) return QStringLiteral("this recording already exists");
    m_store->refresh();
    if (!m_store->canStore(0, &why)) return why;
    Item it;
    it.id = id;
    it.kind = QStringLiteral("recording");
    it.title = info.value("title").toString();
    it.channel = info.value("channel").toString();
    it.url = url.toString();
    it.maxHeight = info.value("maxHeight").toInt();
    it.state = QStringLiteral("recording");
    m_items.insert(id, it);
    m_order.prepend(id);
    m_recording = id;
    const quint64 gen = ++m_recGen;
    save(m_items[id]);
    emit changed();
    QPointer<OfflineDownloads> self(this);
    m_net->get(kApp, url, [self, id, gen, url](NetReply r) {
        if (!self || self->m_recGen != gen || !self->m_items.contains(id)) return;
        if (!r.error.isEmpty()) { self->finishRecording(id, QStringLiteral("could not start: ") + r.error); return; }
        Item &it = self->m_items[id];
        const QList<HlsVariant> vs = parseHlsMaster(QString::fromUtf8(r.body), url);
        const HlsVariant *v = chooseHlsVariant(vs, it.maxHeight);
        if (v) { it.mediaUrl = v->url; it.quality = v->name; } else it.mediaUrl = url;      // (already a media playlist)
        QString why;
        if (!AppNet::urlAllowed(kApp, it.mediaUrl, &why)) { self->finishRecording(id, why); return; }
        self->pollRecording(id, gen);
    });
    return {};
}

void OfflineDownloads::pollRecording(const QString &id, quint64 gen)
{
    if (m_recGen != gen || !m_items.contains(id)) return;
    Item &it = m_items[id];
    QPointer<OfflineDownloads> self(this);
    m_net->get(kApp, it.mediaUrl, [self, id, gen](NetReply r) {
        if (!self || self->m_recGen != gen || !self->m_items.contains(id)) return;
        Item &it = self->m_items[id];
        const HlsMedia m = r.error.isEmpty() ? parseHlsMedia(QString::fromUtf8(r.body), it.mediaUrl) : HlsMedia{};
        if (!r.error.isEmpty() || !m.unsupported.isEmpty()) {
            // the channel going offline answers with errors: a few in a row end the recording, keeping what there is
            if (++it.pollFailures >= 4) { self->finishRecording(id, it.done > 0 ? QStringLiteral("the stream ended") : (r.error.isEmpty() ? m.unsupported : r.error)); return; }
            QTimer::singleShot(self->m_pollMinMs * 2, self, [self, id, gen] { if (self) self->pollRecording(id, gen); });
            return;
        }
        it.pollFailures = 0;
        QList<HlsPart> fresh;
        qint64 startAt = it.lastSeq + 1;
        if (it.lastSeq < 0) startAt = qMax<qint64>(m.firstSequence, m.firstSequence + m.parts.size() - 3);   // the live edge: from now, not from the window's start
        for (int i = 0; i < m.parts.size(); ++i)
            if (m.firstSequence + i >= startAt) fresh << m.parts[i];
        if (it.lastSeq < 0 && !fresh.isEmpty()) it.lastSeq = startAt - 1;
        it.pollDelayMs = qMax(self->m_pollMinMs, int((m.targetSeconds > 0 ? m.targetSeconds : 4.0) * 500));
        it.streamEnded = m.finished;
        self->recordPieces(id, gen, fresh, 0);          // keeps the new pieces in order, one at a time; then polls again (or ends)
    });
}

void OfflineDownloads::recordPieces(const QString &id, quint64 gen, QList<HlsPart> queue, int attempt)
{
    if (m_recGen != gen || !m_items.contains(id)) return;
    Item &it = m_items[id];
    if (queue.isEmpty()) {
        if (it.streamEnded) { finishRecording(id, QStringLiteral("the stream ended")); return; }
        QTimer::singleShot(it.pollDelayMs, this, [this, id, gen] { pollRecording(id, gen); });
        return;
    }
    if (it.done % 8 == 0) m_store->refresh();
    QString why;
    if (!m_store->canStore(0, &why) || m_store->full()) { finishRecording(id, QStringLiteral("stopped early: ") + (why.isEmpty() ? QStringLiteral("your offline storage is full") : why)); return; }
    const HlsPart part = queue.first();
    QPointer<OfflineDownloads> self(this);
    NetLimits lim;
    lim.maxBody = kMaxPart;
    lim.timeoutMs = kPartTimeoutMs;
    m_net->get(kApp, part.url, [self, id, gen, attempt, queue, part](NetReply r) mutable {
        if (!self || self->m_recGen != gen || !self->m_items.contains(id)) return;
        Item &it = self->m_items[id];
        if (!r.error.isEmpty() || r.body.isEmpty()) {
            if (attempt + 1 < kRetries) {
                QTimer::singleShot(self->m_retryMs, self, [self, id, gen, queue, attempt] { if (self) self->recordPieces(id, gen, queue, attempt + 1); });
                return;
            }
            // a piece that cannot be had is lost (a live stream does not wait); carry on with the next one
            queue.removeFirst();
            ++it.lastSeq;
            self->recordPieces(id, gen, queue, 0);
            return;
        }
        const QString path = self->dirOf(id) + QStringLiteral("/video.ts");
        QDir().mkpath(self->dirOf(id));
        QFile out(path);
        if (!out.open(QIODevice::ReadWrite)) { self->finishRecording(id, QStringLiteral("could not write to the offline folder")); return; }
        out.resize(it.bytes);
        out.seek(it.bytes);
        const bool ok = out.write(r.body) == r.body.size();
        out.close();
        if (!ok) { self->finishRecording(id, QStringLiteral("stopped early: the drive is full or cannot be written to")); return; }
        it.bytes += r.body.size();
        it.done += 1;
        it.seconds += part.seconds;
        ++it.lastSeq;
        if (it.done % 5 == 0) self->save(it);
        emit self->changed();
        queue.removeFirst();
        self->recordPieces(id, gen, queue, 0);
    }, lim);
}

void OfflineDownloads::finishRecording(QString id, const QString &note)   // (a copy: callers may pass m_recording itself)
{
    if (m_recording != id) return;
    ++m_recGen;                                                     // nothing still on the way is used
    m_recording.clear();
    auto f = m_items.find(id);
    if (f == m_items.end()) return;
    if (f->done == 0) {                                             // nothing was recorded: no empty item is kept
        const QString why = note.isEmpty() ? QStringLiteral("nothing was recorded") : note;
        setState(*f, QStringLiteral("failed"), why);
    } else {
        setState(*f, QStringLiteral("done"), note);
    }
    save(*f);
    m_store->refresh();
    emit changed();
    if (f->state == QLatin1String("done")) emit finished(id);
}

} // namespace hop
