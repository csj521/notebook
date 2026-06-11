#pragma once
#include <QString>
#include <QDateTime>
#include <QJsonObject>

struct Reminder {
    int id = -1;
    int noteId = -1;
    QString title;
    QString description;
    QDateTime triggerAt;
    bool isRepeated = false;
    int repeatIntervalMin = 0;
    bool isEnabled = true;

    QJsonObject toJson() const {
        QJsonObject o;
        o["id"] = id;
        o["note_id"] = noteId;
        o["title"] = title;
        o["description"] = description;
        o["trigger_at"] = triggerAt.toString("yyyy-MM-dd HH:mm:ss");
        o["is_repeated"] = isRepeated;
        o["repeat_interval_min"] = repeatIntervalMin;
        o["is_enabled"] = isEnabled;
        return o;
    }
};
