#include "Storage.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>

Storage& Storage::instance() { static Storage s; return s; }

QString Storage::dataDir() const {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return path;
}
QString Storage::notesDir() const { return dataDir() + "/notes"; }
QString Storage::remindersPath() const { return dataDir() + "/reminders.json"; }
QString Storage::settingsPath() const { return dataDir() + "/settings.json"; }

void Storage::init() {
    QMutexLocker lock(&m_mutex);
    if (m_initialized) return;
    m_initialized = true;
    QDir().mkpath(dataDir());
    QDir().mkpath(notesDir());
    loadSettings();
}

// ── Settings ──
void Storage::loadSettings() {
    m_settings.clear();
    m_settings["always_on_top"] = "false";
    m_settings["auto_start"] = "false";
    m_settings["close_to_tray"] = "true";
    m_settings["theme"] = "default";
    QFile f(settingsPath());
    if (f.open(QIODevice::ReadOnly)) {
        QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            m_settings[it.key()] = it.value().toString();
        f.close();
    }
}
void Storage::saveSettings() {
    QJsonObject obj;
    for (auto it = m_settings.begin(); it != m_settings.end(); ++it)
        obj[it.key()] = it.value();
    QFile f(settingsPath());
    if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(obj).toJson()); f.close(); }
}
QString Storage::getSetting(const QString& key) {
    QMutexLocker lock(&m_mutex);
    return m_settings.value(key);
}
void Storage::setSetting(const QString& key, const QString& value) {
    QMutexLocker lock(&m_mutex);
    m_settings[key] = value;
    saveSettings();
}

// ── Notes ──
Note Storage::loadNote(const QString& path) {
    Note n; n.id = -1;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return n;
    QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    n.id = o["id"].toInt(-1);
    n.title = o["title"].toString();
    n.content = o["content"].toString();
    n.priority = o["priority"].toInt(0);
    n.category = o["category"].toString();
    n.isCompleted = o["is_completed"].toBool(false);
    n.isPinned = o["is_pinned"].toBool(false);
    n.createdAt = QDateTime::fromString(o["created_at"].toString(), "yyyy-MM-dd HH:mm:ss");
    n.updatedAt = QDateTime::fromString(o["updated_at"].toString(), "yyyy-MM-dd HH:mm:ss");
    return n;
}
void Storage::saveNote(const Note& note) {
    QFile f(notesDir() + "/" + QString::number(note.id) + ".json");
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(note.toJson()).toJson());
    f.close();
}
int Storage::nextNoteId() {
    int maxId = 0;
    QDir dir(notesDir());
    for (auto& entry : dir.entryList({"*.json"}, QDir::Files)) {
        int id = entry.chopped(5).toInt();
        if (id > maxId) maxId = id;
    }
    return maxId + 1;
}
QVector<Note> Storage::getAllNotes() {
    QMutexLocker lock(&m_mutex);
    QVector<Note> notes;
    QDir dir(notesDir());
    for (auto& entry : dir.entryList({"*.json"}, QDir::Files)) {
        Note n = loadNote(dir.filePath(entry));
        if (n.id >= 0) notes.append(n);
    }
    std::sort(notes.begin(), notes.end(), [](const Note& a, const Note& b) {
        if (a.isPinned != b.isPinned) return a.isPinned > b.isPinned;
        return a.updatedAt > b.updatedAt;
    });
    return notes;
}
Note Storage::getNote(int id) {
    QMutexLocker lock(&m_mutex);
    return loadNote(notesDir() + "/" + QString::number(id) + ".json");
}
int Storage::addNote(const Note& note) {
    QMutexLocker lock(&m_mutex);
    Note n = note;
    n.id = nextNoteId();
    QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    n.createdAt = QDateTime::fromString(now, "yyyy-MM-dd HH:mm:ss");
    n.updatedAt = n.createdAt;
    saveNote(n);
    return n.id;
}
bool Storage::updateNote(int id, const Note& note) {
    QMutexLocker lock(&m_mutex);
    Note n = getNote(id);
    if (n.id < 0) return false;
    saveNote(note);
    return true;
}
bool Storage::deleteNote(int id) {
    QMutexLocker lock(&m_mutex);
    return QFile::remove(notesDir() + "/" + QString::number(id) + ".json");
}
Note Storage::toggleComplete(int id) {
    QMutexLocker lock(&m_mutex);
    Note n = getNote(id);
    if (n.id < 0) return n;
    n.isCompleted = !n.isCompleted;
    n.updatedAt = QDateTime::currentDateTime();
    saveNote(n);
    return n;
}
Note Storage::togglePin(int id) {
    QMutexLocker lock(&m_mutex);
    Note n = getNote(id);
    if (n.id < 0) return n;
    n.isPinned = !n.isPinned;
    n.updatedAt = QDateTime::currentDateTime();
    saveNote(n);
    return n;
}

// ── Reminders ──
int Storage::nextReminderId() {
    int maxId = 0;
    QFile f(remindersPath());
    if (!f.open(QIODevice::ReadOnly)) return maxId + 1;
    QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    f.close();
    for (auto v : arr) { int id = v.toObject()["id"].toInt(); if (id > maxId) maxId = id; }
    return maxId + 1;
}
QVector<Reminder> Storage::getAllReminders() {
    QMutexLocker lock(&m_mutex);
    QVector<Reminder> list;
    QFile f(remindersPath());
    if (!f.open(QIODevice::ReadOnly)) return list;
    QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    f.close();
    for (auto v : arr) {
        QJsonObject o = v.toObject();
        Reminder r;
        r.id = o["id"].toInt(); r.noteId = o["note_id"].toInt(-1);
        r.title = o["title"].toString(); r.description = o["description"].toString();
        r.triggerAt = QDateTime::fromString(o["trigger_at"].toString(), "yyyy-MM-dd HH:mm:ss");
        r.isRepeated = o["is_repeated"].toBool(); r.repeatIntervalMin = o["repeat_interval_min"].toInt();
        r.isEnabled = o["is_enabled"].toBool(true);
        list.append(r);
    }
    return list;
}
QVector<Reminder> Storage::getDueReminders() {
    QMutexLocker lock(&m_mutex);
    QVector<Reminder> list;
    QDateTime now = QDateTime::currentDateTime();
    QDateTime cutoff = now.addSecs(-600);

    QFile f(remindersPath());
    if (!f.open(QIODevice::ReadOnly)) return list;
    QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    f.close();

    QJsonArray updated;
    for (auto v : arr) {
        QJsonObject o = v.toObject();
        Reminder r;
        r.id = o["id"].toInt(); r.noteId = o["note_id"].toInt(-1);
        r.title = o["title"].toString(); r.description = o["description"].toString();
        r.triggerAt = QDateTime::fromString(o["trigger_at"].toString(), "yyyy-MM-dd HH:mm:ss");
        r.isRepeated = o["is_repeated"].toBool(); r.repeatIntervalMin = o["repeat_interval_min"].toInt();
        r.isEnabled = o["is_enabled"].toBool(true);

        if (!r.isEnabled) { updated.append(o); continue; }
        if (r.triggerAt <= now && r.triggerAt >= cutoff) {
            list.append(r);
            if (!r.isRepeated) { o["is_enabled"] = false; }
        } else if (r.triggerAt < cutoff && !r.isRepeated) {
            o["is_enabled"] = false;  // Auto-disable old one-shot reminders
        }
        updated.append(o);
    }
    // Save updates
    QFile wf(remindersPath());
    if (wf.open(QIODevice::WriteOnly)) { wf.write(QJsonDocument(updated).toJson()); wf.close(); }
    return list;
}
int Storage::addReminder(const Reminder& r) {
    QMutexLocker lock(&m_mutex);
    Reminder rr = r;
    rr.id = nextReminderId();
    QFile f(remindersPath());
    QJsonArray arr;
    if (f.open(QIODevice::ReadOnly)) { arr = QJsonDocument::fromJson(f.readAll()).array(); f.close(); }
    arr.append(rr.toJson());
    if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(arr).toJson()); f.close(); }
    return rr.id;
}
bool Storage::updateReminder(int id, const Reminder& r) {
    QMutexLocker lock(&m_mutex);
    QFile f(remindersPath());
    QJsonArray arr;
    if (!f.open(QIODevice::ReadOnly)) return false;
    arr = QJsonDocument::fromJson(f.readAll()).array(); f.close();
    for (int i = 0; i < arr.size(); i++) {
        if (arr[i].toObject()["id"].toInt() == id) { arr[i] = r.toJson(); break; }
    }
    if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(arr).toJson()); f.close(); }
    return true;
}
bool Storage::deleteReminder(int id) {
    QMutexLocker lock(&m_mutex);
    QFile f(remindersPath());
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array(); f.close();
    QJsonArray filtered;
    for (auto v : arr) if (v.toObject()["id"].toInt() != id) filtered.append(v);
    if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(filtered).toJson()); f.close(); }
    return filtered.size() < arr.size();
}
