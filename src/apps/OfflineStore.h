// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

namespace hop {

// The room set aside for things the Kick app keeps for offline viewing (downloaded past streams and clips, recordings).
// It is a limit the person chooses, like a posting limit: a number in MB or GB they can raise or lower at any time, and optionally a
// drive or folder of their choice. Nothing is ever deleted to make room: when the limit (or the drive) is full, new downloads pause and
// the person decides. All of it lives in one folder per profile; the settings are saved with the profile.
class OfflineStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(double limitMB READ limitMB NOTIFY changed)
    Q_PROPERTY(qint64 limitBytes READ limitBytes NOTIFY changed)
    Q_PROPERTY(qint64 usedBytes READ usedBytes NOTIFY changed)
    Q_PROPERTY(qint64 freeBytes READ freeBytes NOTIFY changed)
    Q_PROPERTY(double fraction READ fraction NOTIFY changed)           // used / limit, 0..1
    Q_PROPERTY(bool nearLimit READ nearLimit NOTIFY changed)           // 80% or more
    Q_PROPERTY(bool full READ full NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)           // the folder exists (an unplugged drive is "unavailable", not an error)
    Q_PROPERTY(QString location READ location NOTIFY changed)          // the chosen parent folder ("" = Hop-tunes' own data folder)
    Q_PROPERTY(QString folder READ folder NOTIFY changed)              // where the files go
    Q_PROPERTY(QString summary READ summary NOTIFY changed)            // one plain sentence for the settings screen

public:
    static constexpr qint64 kMB = 1024 * 1024;
    static constexpr double kMinLimitMB = 50;
    static constexpr double kDefaultLimitMB = 5 * 1024;                // 5 GB: video takes room
    static constexpr qint64 kMinFree = 1024 * kMB;                     // always leave this much free on the drive

    explicit OfflineStore(QObject *parent = nullptr) : QObject(parent) {}

    // where it keeps things when no folder was chosen, and the profile it is for (set by the app; tests use a temp folder)
    void setDefaultBase(const QString &dir);
    void setProfileId(const QString &id);
    // tests replace how free room on the drive is found
    void setFreeProbe(std::function<qint64(const QString &)> probe) { m_freeProbe = std::move(probe); }

    double limitMB() const { return m_limitMB; }
    qint64 limitBytes() const { return static_cast<qint64>(m_limitMB * kMB); }
    qint64 usedBytes() const { return m_used; }
    qint64 freeBytes() const;
    double fraction() const { return limitBytes() > 0 ? qMin(1.0, double(m_used) / double(limitBytes())) : 0; }
    bool nearLimit() const { return fraction() >= 0.8; }
    bool full() const { return m_used >= limitBytes(); }
    bool available() const;
    QString location() const { return m_location; }
    QString folder() const;
    QString summary() const;

    // the number the person set: false (and nothing changes) below the smallest limit or for nonsense
    Q_INVOKABLE bool setLimitMB(double mb);
    Q_INVOKABLE bool setLimitGB(double gb) { return setLimitMB(gb * 1024); }
    // choose a drive or folder ("" = back to Hop-tunes' own folder); the existing files are not touched here (see moveTo)
    Q_INVOKABLE bool setLocation(const QString &dir);
    // move what is already stored to the new location, then use it; false (nothing changed) when the new place cannot hold it
    Q_INVOKABLE bool moveTo(const QString &dir, QString *why = nullptr);
    // count what is on disk now (a scan of the folder: called when the screen opens and after a download)
    Q_INVOKABLE void refresh();
    // for the settings screen: the reason in plain words, or "" when it worked
    Q_INVOKABLE QString moveToLocation(const QString &dir) { QString why; return moveTo(dir, &why) ? QString() : why; }
    // may `bytes` more be stored? "" in `why` when yes; otherwise one plain sentence
    bool canStore(qint64 bytes, QString *why = nullptr) const;
    // a download in progress asks again as it goes, with how many bytes it has already written
    bool mayContinue(qint64 writtenSoFar, QString *why = nullptr) const { return canStore(writtenSoFar, why); }

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &o);

signals:
    void changed();
    void edited();     // the person changed a setting: save it with the profile

private:
    QString baseDir() const;
    static qint64 dirSize(const QString &dir);
    double m_limitMB = kDefaultLimitMB;
    QString m_location;
    QString m_defaultBase;
    QString m_profileId = QStringLiteral("profile");
    qint64 m_used = 0;
    std::function<qint64(const QString &)> m_freeProbe;
};

} // namespace hop
