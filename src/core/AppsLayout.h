// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace hop {

// Where the apps are on the apps page, and the folders they can be filed in. Pure data, no screen: the grid draws from it and the person's drags
// change it. An app is either on the grid (in a slot) or inside exactly one folder; a folder is on the grid like an app. Slots are numbered across
// pages (slot / perPage is the page) and can have gaps, so an app can sit anywhere. The wide layout (five across) and the phone layout (three
// across) are remembered separately, so turning a device does not scramble either.
// Items are named "app:<id>" and "folder:<id>". Everything that comes from a settings file is sanitised on load.
class AppsLayout : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString form READ form WRITE setForm NOTIFY changed)       // "wide" | "phone": which of the two layouts is shown
    Q_PROPERTY(int revision READ revision NOTIFY changed)                 // changes whenever anything does (a binding on it re-reads the slots)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY changed)          // [{id, name, color, picture, members: [appId]}]
public:
    static constexpr int kMaxSlots = 120, kMaxFolders = 30, kMaxMembers = 60, kMaxName = 40;

    explicit AppsLayout(QObject *parent = nullptr);

    QString form() const { return m_form == 1 ? QStringLiteral("phone") : QStringLiteral("wide"); }
    void setForm(const QString &f);
    int revision() const { return m_revision; }
    QVariantList folders() const;

    // The apps that exist (in their default order): apps not placed yet get the first free spot, apps that are gone are dropped everywhere.
    Q_INVOKABLE void setKnownApps(const QStringList &ids);

    Q_INVOKABLE QString itemAt(int slot) const;                          // "" for a free slot
    Q_INVOKABLE int slotOf(const QString &item) const;                   // -1 when it is not on the grid
    Q_INVOKABLE QString folderOf(const QString &appId) const;            // the folder an app is in, or ""
    Q_INVOKABLE QVariantMap folder(const QString &id) const;             // {} when there is none
    Q_INVOKABLE int freeSlotNear(int slot) const;                        // the free slot closest to `slot` (-1 when the page is full)
    Q_INVOKABLE int usedSlots() const;                                   // the highest used slot + 1 (how many slots the pages must cover)

    // A drag ended on `slot`: a free slot moves the item there, an occupied one swaps places; an app that was in a folder comes out onto the grid.
    Q_INVOKABLE bool moveItem(const QString &item, int slot);
    Q_INVOKABLE QString createFolder(const QString &name, int slot = -1);   // the new folder's id, or "" (too many folders, no room)
    Q_INVOKABLE bool addToFolder(const QString &folderId, const QString &appId);
    Q_INVOKABLE bool removeFromFolder(const QString &appId, int slot = -1);
    Q_INVOKABLE bool moveInFolder(const QString &folderId, const QString &appId, int toIndex);
    Q_INVOKABLE bool renameFolder(const QString &id, const QString &name);
    Q_INVOKABLE bool setFolderColor(const QString &id, const QString &color);   // "#rrggbb", or "" for the clear glass
    Q_INVOKABLE bool deleteFolder(const QString &id);                    // its apps go back onto the grid; nothing else is lost
    // The file name (inside the profile's folder pictures folder) of the folder's own picture; "" removes it. The old file name goes out through pictureDropped().
    bool setFolderPictureFile(const QString &id, const QString &file);

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &o);                                  // sanitised; an unreadable value gives the default layout
    void reset();                                                         // the default layout (everything on the grid in order)

signals:
    void changed();
    void edited();                                                        // something worth saving in the profile changed
    void pictureDropped(const QString &file);                             // a folder picture file is no longer used (delete it)

private:
    struct Folder { QString id, name, color, picture; QStringList members; };
    int formIndex() const { return m_form; }
    Folder *find(const QString &id);
    const Folder *find(const QString &id) const;
    bool validApp(const QString &id) const;
    int firstFree(int form, int from = 0) const;
    int nearestFree(int form, int slot) const;
    void place(int form, const QString &item, int slot);
    void unplaceEverywhere(const QString &item);
    void placeEverywhereFree(const QString &item, int formSlotHint = -1);
    bool normalize();                                                     // true when it changed anything
    void touched();

    int m_form = 0;
    int m_revision = 0;
    int m_next = 1;
    QVector<Folder> m_folders;
    QHash<int, QString> m_slots[2];
    QStringList m_known;
    bool m_knownSet = false;
};

} // namespace hop
