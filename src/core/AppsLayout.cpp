// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/AppsLayout.h"

#include <QColor>
#include <QJsonArray>
#include <QRegularExpression>

#include <algorithm>
#include <cstdlib>

namespace hop {

namespace {
const QString kApp = QStringLiteral("app:");
const QString kFolder = QStringLiteral("folder:");
bool isApp(const QString &item) { return item.startsWith(kApp); }
bool isFolder(const QString &item) { return item.startsWith(kFolder); }
QString appOf(const QString &item) { return item.mid(kApp.size()); }
QString folderIdOf(const QString &item) { return item.mid(kFolder.size()); }
bool validFolderId(const QString &id) { static const QRegularExpression re(QStringLiteral("^f[0-9]{1,6}$")); return re.match(id).hasMatch(); }
QString cleanName(const QString &s) { return s.simplified().left(AppsLayout::kMaxName); }
}

AppsLayout::AppsLayout(QObject *parent) : QObject(parent) {}

void AppsLayout::setForm(const QString &f)
{
    const int v = f == QLatin1String("phone") ? 1 : 0;
    if (v == m_form) return;
    m_form = v;
    ++m_revision;
    emit changed();
}

void AppsLayout::touched()
{
    ++m_revision;
    emit changed();
    emit edited();
}

bool AppsLayout::validApp(const QString &id) const
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_-]{1,40}$"));
    return re.match(id).hasMatch();
}

AppsLayout::Folder *AppsLayout::find(const QString &id)
{
    for (Folder &f : m_folders) if (f.id == id) return &f;
    return nullptr;
}

const AppsLayout::Folder *AppsLayout::find(const QString &id) const
{
    for (const Folder &f : m_folders) if (f.id == id) return &f;
    return nullptr;
}

int AppsLayout::firstFree(int form, int from) const
{
    for (int i = qMax(0, from); i < kMaxSlots; ++i) if (!m_slots[form].contains(i)) return i;
    return -1;
}

int AppsLayout::nearestFree(int form, int slot) const
{
    slot = qBound(0, slot, kMaxSlots - 1);
    for (int d = 0; d < kMaxSlots; ++d) {
        if (slot + d < kMaxSlots && !m_slots[form].contains(slot + d)) return slot + d;
        if (slot - d >= 0 && !m_slots[form].contains(slot - d)) return slot - d;
    }
    return -1;
}

void AppsLayout::place(int form, const QString &item, int slot) { m_slots[form].insert(slot, item); }

void AppsLayout::unplaceEverywhere(const QString &item)
{
    for (int f = 0; f < 2; ++f)
        for (auto it = m_slots[f].begin(); it != m_slots[f].end();) it = it.value() == item ? m_slots[f].erase(it) : std::next(it);
}

void AppsLayout::placeEverywhereFree(const QString &item, int hint)
{
    for (int f = 0; f < 2; ++f) {
        const int s = (f == m_form && hint >= 0) ? nearestFree(f, hint) : firstFree(f, 0);
        if (s >= 0) place(f, item, s);
    }
}

int AppsLayout::slotOf(const QString &item) const
{
    for (auto it = m_slots[m_form].constBegin(); it != m_slots[m_form].constEnd(); ++it) if (it.value() == item) return it.key();
    return -1;
}

QString AppsLayout::itemAt(int slot) const { return m_slots[m_form].value(slot); }

int AppsLayout::freeSlotNear(int slot) const { return nearestFree(m_form, slot); }

int AppsLayout::usedSlots() const
{
    int top = -1;
    for (auto it = m_slots[m_form].constBegin(); it != m_slots[m_form].constEnd(); ++it) top = qMax(top, it.key());
    return top + 1;
}

QString AppsLayout::folderOf(const QString &appId) const
{
    for (const Folder &f : m_folders) if (f.members.contains(appId)) return f.id;
    return {};
}

QVariantMap AppsLayout::folder(const QString &id) const
{
    const Folder *f = find(id);
    if (!f) return {};
    return {{QStringLiteral("id"), f->id}, {QStringLiteral("name"), f->name}, {QStringLiteral("color"), f->color}, {QStringLiteral("picture"), f->picture},
            {QStringLiteral("members"), f->members}};
}

QVariantList AppsLayout::folders() const
{
    QVariantList out;
    for (const Folder &f : m_folders) out << folder(f.id);
    return out;
}

void AppsLayout::setKnownApps(const QStringList &ids)
{
    QStringList clean;
    for (const QString &i : ids) if (validApp(i) && !clean.contains(i)) clean << i;
    if (m_knownSet && clean == m_known) return;
    m_known = clean;
    m_knownSet = true;
    if (normalize()) touched();
}

bool AppsLayout::moveItem(const QString &item, int slot)
{
    if (slot < 0 || slot >= kMaxSlots) return false;
    if (isApp(item) && !folderOf(appOf(item)).isEmpty()) {          // out of a folder, onto the grid
        const QString app = appOf(item);
        return removeFromFolder(app, slot);
    }
    const int cur = slotOf(item);
    if (cur < 0) return false;
    if (cur == slot) return true;
    const QString other = m_slots[m_form].value(slot);
    m_slots[m_form].remove(cur);
    if (!other.isEmpty()) m_slots[m_form].insert(cur, other);       // an occupied target swaps places
    m_slots[m_form].insert(slot, item);
    touched();
    return true;
}

QString AppsLayout::createFolder(const QString &name, int slot)
{
    if (m_folders.size() >= kMaxFolders) return {};
    Folder f;
    f.id = QStringLiteral("f") + QString::number(m_next);
    f.name = cleanName(name);
    if (f.name.isEmpty()) f.name = QStringLiteral("folder");
    if (firstFree(m_form) < 0) return {};
    ++m_next;
    m_folders.append(f);
    placeEverywhereFree(kFolder + f.id, slot >= 0 ? slot : -1);
    if (slot >= 0 && slotOf(kFolder + f.id) < 0) { /* current form is full: nothing placed */ }
    touched();
    return f.id;
}

bool AppsLayout::addToFolder(const QString &folderId, const QString &appId)
{
    Folder *f = find(folderId);
    if (!f || !validApp(appId)) return false;
    if (f->members.contains(appId)) return true;
    if (f->members.size() >= kMaxMembers) return false;
    if (m_knownSet && !m_known.contains(appId)) return false;
    for (Folder &o : m_folders) o.members.removeAll(appId);
    unplaceEverywhere(kApp + appId);
    f->members.append(appId);
    touched();
    return true;
}

bool AppsLayout::removeFromFolder(const QString &appId, int slot)
{
    Folder *src = nullptr;
    for (Folder &f : m_folders) if (f.members.contains(appId)) src = &f;
    if (!src) return false;
    if (firstFree(m_form) < 0) return false;
    src->members.removeAll(appId);
    placeEverywhereFree(kApp + appId, slot >= 0 ? slot : 0);
    touched();
    return true;
}

bool AppsLayout::moveInFolder(const QString &folderId, const QString &appId, int toIndex)
{
    Folder *f = find(folderId);
    if (!f) return false;
    const int from = f->members.indexOf(appId);
    if (from < 0) return false;
    const int to = qBound(0, toIndex, int(f->members.size()) - 1);
    if (from == to) return true;
    f->members.move(from, to);
    touched();
    return true;
}

bool AppsLayout::renameFolder(const QString &id, const QString &name)
{
    Folder *f = find(id);
    const QString n = cleanName(name);
    if (!f || n.isEmpty()) return false;
    if (f->name == n) return true;
    f->name = n;
    touched();
    return true;
}

bool AppsLayout::setFolderColor(const QString &id, const QString &color)
{
    Folder *f = find(id);
    if (!f) return false;
    QString c;
    if (!color.isEmpty()) {
        static const QRegularExpression re(QStringLiteral("^#[0-9a-fA-F]{6}$"));
        if (!re.match(color).hasMatch()) return false;
        c = color.toLower();
    }
    if (f->color == c) return true;
    f->color = c;
    touched();
    return true;
}

bool AppsLayout::setFolderPictureFile(const QString &id, const QString &file)
{
    Folder *f = find(id);
    if (!f) return false;
    static const QRegularExpression re(QStringLiteral("^[a-z0-9._-]{1,80}$"));
    if (!file.isEmpty() && !re.match(file).hasMatch()) return false;
    const QString old = f->picture;
    if (old == file) return true;
    f->picture = file;
    touched();
    if (!old.isEmpty()) emit pictureDropped(old);
    return true;
}

bool AppsLayout::deleteFolder(const QString &id)
{
    int idx = -1;
    for (int i = 0; i < m_folders.size(); ++i) if (m_folders.at(i).id == id) idx = i;
    if (idx < 0) return false;
    const Folder gone = m_folders.at(idx);
    const int hint = slotOf(kFolder + id);
    unplaceEverywhere(kFolder + id);
    m_folders.removeAt(idx);
    int at = hint >= 0 ? hint : 0;
    for (const QString &app : gone.members) {                      // its apps go back onto the grid, starting where the folder was
        placeEverywhereFree(kApp + app, at);
        at = qMax(0, slotOf(kApp + app)) + 1;
    }
    touched();
    if (!gone.picture.isEmpty()) emit pictureDropped(gone.picture);
    return true;
}

bool AppsLayout::normalize()
{
    bool changed = false;
    // folders: sane ids, no doubles, only apps that exist, an app in one folder only
    QSet<QString> seenFolder, inFolder;
    for (int i = 0; i < m_folders.size();) {
        Folder &f = m_folders[i];
        if (!validFolderId(f.id) || seenFolder.contains(f.id) || i >= kMaxFolders) { m_folders.removeAt(i); changed = true; continue; }
        seenFolder.insert(f.id);
        QStringList keep;
        for (const QString &a : std::as_const(f.members))
            if (validApp(a) && !inFolder.contains(a) && (!m_knownSet || m_known.contains(a)) && keep.size() < kMaxMembers) { keep << a; inFolder.insert(a); }
        if (keep != f.members) { f.members = keep; changed = true; }
        ++i;
    }
    for (int form = 0; form < 2; ++form) {
        QHash<int, QString> clean;
        QSet<QString> used;
        QList<int> keys = m_slots[form].keys();
        std::sort(keys.begin(), keys.end());
        for (int k : keys) {
            const QString item = m_slots[form].value(k);
            bool ok = k >= 0 && k < kMaxSlots && !used.contains(item);
            if (ok && isApp(item)) ok = validApp(appOf(item)) && !inFolder.contains(appOf(item)) && (!m_knownSet || m_known.contains(appOf(item)));
            else if (ok && isFolder(item)) ok = seenFolder.contains(folderIdOf(item));
            else if (ok) ok = false;
            if (ok) { clean.insert(k, item); used.insert(item); }
        }
        if (clean != m_slots[form]) { m_slots[form] = clean; changed = true; }
        // whatever is not placed yet takes the first free spot: the known apps in their order, then folders
        auto add = [&](const QString &item) {
            if (used.contains(item)) return;
            const int s = firstFree(form, 0);
            if (s < 0) return;
            m_slots[form].insert(s, item);
            used.insert(item);
            changed = true;
        };
        if (m_knownSet) for (const QString &a : std::as_const(m_known)) if (!inFolder.contains(a)) add(kApp + a);
        for (const Folder &f : std::as_const(m_folders)) add(kFolder + f.id);
    }
    return changed;
}

QJsonObject AppsLayout::toJson() const
{
    QJsonArray fs;
    for (const Folder &f : m_folders)
        fs.append(QJsonObject{{QStringLiteral("id"), f.id}, {QStringLiteral("name"), f.name}, {QStringLiteral("color"), f.color}, {QStringLiteral("picture"), f.picture},
                              {QStringLiteral("members"), QJsonArray::fromStringList(f.members)}});
    QJsonObject slotMap;
    const char *names[2] = {"wide", "phone"};
    for (int form = 0; form < 2; ++form) {
        QJsonObject o;
        for (auto it = m_slots[form].constBegin(); it != m_slots[form].constEnd(); ++it) o.insert(QString::number(it.key()), it.value());
        slotMap.insert(QLatin1String(names[form]), o);
    }
    return QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("next"), m_next}, {QStringLiteral("folders"), fs}, {QStringLiteral("slots"), slotMap}};
}

void AppsLayout::reset()
{
    m_folders.clear();
    m_slots[0].clear(); m_slots[1].clear();
    m_next = 1;
    normalize();
    ++m_revision;
    emit changed();
}

void AppsLayout::fromJson(const QJsonObject &o)
{
    m_folders.clear();
    m_slots[0].clear(); m_slots[1].clear();
    m_next = 1;
    if (o.value(QStringLiteral("version")).toInt() == 1) {
        m_next = qBound(1, o.value(QStringLiteral("next")).toInt(1), 999999);
        for (const QJsonValue &v : o.value(QStringLiteral("folders")).toArray()) {
            if (m_folders.size() >= kMaxFolders) break;
            const QJsonObject fo = v.toObject();
            Folder f;
            f.id = fo.value(QStringLiteral("id")).toString();
            f.name = cleanName(fo.value(QStringLiteral("name")).toString());
            if (f.name.isEmpty()) f.name = QStringLiteral("folder");
            const QString c = fo.value(QStringLiteral("color")).toString();
            static const QRegularExpression col(QStringLiteral("^#[0-9a-fA-F]{6}$"));
            f.color = col.match(c).hasMatch() ? c.toLower() : QString();
            const QString p = fo.value(QStringLiteral("picture")).toString();
            static const QRegularExpression pic(QStringLiteral("^[a-z0-9._-]{1,80}$"));
            f.picture = pic.match(p).hasMatch() ? p : QString();
            for (const QJsonValue &m : fo.value(QStringLiteral("members")).toArray()) if (m.isString()) f.members << m.toString();
            m_folders.append(f);
            if (validFolderId(f.id)) m_next = qMax(m_next, f.id.mid(1).toInt() + 1);
        }
        const QJsonObject slotMap = o.value(QStringLiteral("slots")).toObject();
        const char *names[2] = {"wide", "phone"};
        for (int form = 0; form < 2; ++form) {
            const QJsonObject so = slotMap.value(QLatin1String(names[form])).toObject();
            for (auto it = so.constBegin(); it != so.constEnd(); ++it) {
                bool ok = false;
                const int k = it.key().toInt(&ok);
                if (ok && k >= 0 && k < kMaxSlots && it.value().isString()) m_slots[form].insert(k, it.value().toString());
            }
        }
    }
    normalize();
    ++m_revision;
    emit changed();
}

} // namespace hop
