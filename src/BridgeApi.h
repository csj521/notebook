#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QMainWindow>

class SchedulerWorker;

class BridgeApi : public QObject {
    Q_OBJECT
public:
    explicit BridgeApi(QMainWindow* window, SchedulerWorker* scheduler, QObject* parent = nullptr);

    Q_INVOKABLE QJsonArray getNotes();
    Q_INVOKABLE QJsonObject getNote(int id);
    Q_INVOKABLE QJsonObject addNote(const QString& title, const QString& content = "",
                                     int priority = 0, const QString& category = "");
    Q_INVOKABLE QJsonObject updateNote(int id, const QJsonObject& fields);
    Q_INVOKABLE bool deleteNote(int id);
    Q_INVOKABLE QJsonObject toggleNoteComplete(int id);
    Q_INVOKABLE QJsonObject toggleNotePin(int id);

    Q_INVOKABLE QJsonArray getReminders();
    Q_INVOKABLE QJsonObject addReminder(const QJsonObject& data);
    Q_INVOKABLE bool deleteReminder(int id);
    Q_INVOKABLE QJsonObject updateReminder(int id, const QJsonObject& fields);

    Q_INVOKABLE QString getSetting(const QString& key);
    Q_INVOKABLE void setSetting(const QString& key, const QString& value);
    Q_INVOKABLE bool setAutoStart(bool on);
    Q_INVOKABLE bool getAutoStart();

    Q_INVOKABLE QJsonArray getPendingReminders();

    Q_INVOKABLE void minimizeWindow();
    Q_INVOKABLE void closeWindow();
    Q_INVOKABLE void toggleOnTop();
    Q_INVOKABLE void setAlwaysOnTop(bool on);

signals:
    void minimizeRequested();
    void closeRequested();
    void toggleOnTopRequested();
    void alwaysOnTopChanged(bool on);
    void themeChanged(const QString& theme);

private:
    QMainWindow* m_window;
    SchedulerWorker* m_scheduler;
};
