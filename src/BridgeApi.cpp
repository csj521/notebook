#include "BridgeApi.h"
#include "Storage.h"
#include "AutoStart.h"
#include "Scheduler.h"
#include <QApplication>
#include <QMainWindow>
#include <QJsonDocument>

BridgeApi::BridgeApi(QMainWindow* window, SchedulerWorker* scheduler, QObject* parent)
    : QObject(parent), m_window(window), m_scheduler(scheduler) {}

QJsonArray BridgeApi::getNotes() {
    QJsonArray arr;
    for (auto& n : Storage::instance().getAllNotes())
        arr.append(n.toJson());
    return arr;
}

QJsonObject BridgeApi::getNote(int id) {
    return Storage::instance().getNote(id).toJson();
}

QJsonObject BridgeApi::addNote(const QString& title, const QString& content,
                                int priority, const QString& category) {
    Note n;
    n.title = title; n.content = content; n.priority = priority; n.category = category;
    int id = Storage::instance().addNote(n);
    return Storage::instance().getNote(id).toJson();
}

QJsonObject BridgeApi::updateNote(int id, const QJsonObject& fields) {
    Note n = Storage::instance().getNote(id);
    if (n.id < 0) return {};
    if (fields.contains("title")) n.title = fields["title"].toString();
    if (fields.contains("content")) n.content = fields["content"].toString();
    if (fields.contains("priority")) n.priority = fields["priority"].toInt();
    if (fields.contains("category")) n.category = fields["category"].toString();
    if (fields.contains("is_completed")) n.isCompleted = fields["is_completed"].toBool();
    if (fields.contains("is_pinned")) n.isPinned = fields["is_pinned"].toBool();
    Storage::instance().updateNote(id, n);
    return Storage::instance().getNote(id).toJson();
}

bool BridgeApi::deleteNote(int id) {
    return Storage::instance().deleteNote(id);
}

QJsonObject BridgeApi::toggleNoteComplete(int id) {
    return Storage::instance().toggleComplete(id).toJson();
}

QJsonObject BridgeApi::toggleNotePin(int id) {
    return Storage::instance().togglePin(id).toJson();
}

QJsonArray BridgeApi::getReminders() {
    QJsonArray arr;
    for (auto& r : Storage::instance().getAllReminders())
        arr.append(r.toJson());
    return arr;
}

QJsonObject BridgeApi::addReminder(const QJsonObject& data) {
    Reminder r;
    r.title = data["title"].toString();
    r.description = data["description"].toString();
    r.triggerAt = QDateTime::fromString(data["trigger_at"].toString(), "yyyy-MM-dd HH:mm:ss");
    r.isRepeated = data["is_repeated"].toBool();
    r.repeatIntervalMin = data["repeat_interval_min"].toInt();
    r.noteId = data["note_id"].toInt(-1);
    int id = Storage::instance().addReminder(r);
    r.id = id;
    return r.toJson();
}

bool BridgeApi::deleteReminder(int id) {
    return Storage::instance().deleteReminder(id);
}

QJsonObject BridgeApi::updateReminder(int id, const QJsonObject& fields) {
    auto list = Storage::instance().getAllReminders();
    Reminder r;
    for (auto& x : list) { if (x.id == id) { r = x; break; } }
    if (r.id < 0) return {};
    if (fields.contains("title")) r.title = fields["title"].toString();
    if (fields.contains("description")) r.description = fields["description"].toString();
    if (fields.contains("trigger_at")) r.triggerAt = QDateTime::fromString(fields["trigger_at"].toString(), "yyyy-MM-dd HH:mm:ss");
    if (fields.contains("is_repeated")) r.isRepeated = fields["is_repeated"].toBool();
    if (fields.contains("repeat_interval_min")) r.repeatIntervalMin = fields["repeat_interval_min"].toInt();
    if (fields.contains("is_enabled")) r.isEnabled = fields["is_enabled"].toBool();
    Storage::instance().updateReminder(id, r);
    return r.toJson();
}

QString BridgeApi::getSetting(const QString& key) {
    return Storage::instance().getSetting(key);
}

void BridgeApi::setSetting(const QString& key, const QString& value) {
    Storage::instance().setSetting(key, value);
}

bool BridgeApi::setAutoStart(bool on) {
    QString path = QApplication::applicationFilePath();
    bool ok = AutoStart::setEnabled("Notebook", path, on);
    if (ok) Storage::instance().setSetting("auto_start", on ? "true" : "false");
    return ok;
}

bool BridgeApi::getAutoStart() {
    return AutoStart::isEnabled();
}

QJsonArray BridgeApi::getPendingReminders() {
    QJsonArray arr;
    for (auto& r : m_scheduler->getPending())
        arr.append(r.toJson());
    return arr;
}

void BridgeApi::minimizeWindow() {
    if (m_window) m_window->showMinimized();
}

void BridgeApi::closeWindow() {
    if (m_window) m_window->close();
}

void BridgeApi::toggleOnTop() {
    emit toggleOnTopRequested();
}

void BridgeApi::setAlwaysOnTop(bool on) {
    emit alwaysOnTopChanged(on);
}
