#pragma once
#include <QString>
#include <QVector>
#include <QMutex>
#include "Note.h"
#include "Reminder.h"

// File-based storage replacing SQLite.
// Notes → notes/*.json, Reminders → reminders.json, Settings → settings.json

class Storage {
public:
    static Storage& instance();
    void init();

    // Notes
    QVector<Note> getAllNotes();
    Note getNote(int id);
    int addNote(const Note& note);
    bool updateNote(int id, const Note& note);
    bool deleteNote(int id);
    Note toggleComplete(int id);
    Note togglePin(int id);

    // Reminders
    QVector<Reminder> getAllReminders();
    QVector<Reminder> getDueReminders();
    int addReminder(const Reminder& r);
    bool updateReminder(int id, const Reminder& r);
    bool deleteReminder(int id);

    // Settings
    QString getSetting(const QString& key);
    void setSetting(const QString& key, const QString& value);

private:
    Storage() = default;
    QString dataDir() const;
    QString notesDir() const;
    QString remindersPath() const;
    QString settingsPath() const;
    int nextNoteId();
    int nextReminderId();
    Note loadNote(const QString& path);
    void saveNote(const Note& note);
    void loadSettings();
    void saveSettings();

    QMutex m_mutex;
    QMap<QString, QString> m_settings;
    bool m_initialized = false;
};
