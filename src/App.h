// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Scanner.h"
#include "library/TrackIndex.h"
#include "library/TrackListModel.h"
#include "playback/Player.h"

#include <QObject>
#include <QStringList>
#include <QUrl>

namespace hop {

// The object QML talks to: library, scanning, playback. Profiles (spec §3) arrive in
// M3; until then the watched folders live in the app settings.
class App : public QObject {
    Q_OBJECT
    Q_PROPERTY(hop::TrackListModel *songs READ songs CONSTANT)
    Q_PROPERTY(hop::Player *player READ player CONSTANT)
    Q_PROPERTY(QStringList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int scanDone READ scanDone NOTIFY scanProgressChanged)
    Q_PROPERTY(int scanTotal READ scanTotal NOTIFY scanProgressChanged)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit App(QObject *parent = nullptr);
    bool init();   // opens the index, loads the library, starts a background rescan

    TrackListModel *songs() { return &m_songs; }
    Player *player() { return &m_player; }
    QStringList folders() const { return m_folders; }
    bool scanning() const { return m_scanner.isRunning(); }
    int scanDone() const { return m_scanDone; }
    int scanTotal() const { return m_scanTotal; }
    QString version() const;

    Q_INVOKABLE void addFolder(const QUrl &folder);
    Q_INVOKABLE void removeFolder(const QString &path);
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void playSongs(int row);    // play the song list starting at row
    Q_INVOKABLE void shuffleAll();
    Q_INVOKABLE QString formatTime(qint64 ms) const { return TrackListModel::formatDuration(ms); }

signals:
    void foldersChanged();
    void scanningChanged();
    void scanProgressChanged();

private:
    void reload();
    void saveFolders();

    TrackIndex m_index;
    Scanner m_scanner{&m_index};
    TrackListModel m_songs;
    Player m_player;
    QStringList m_folders;
    int m_scanDone = 0;
    int m_scanTotal = 0;
};

} // namespace hop
