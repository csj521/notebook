#pragma once
#include <QString>
#include <QDateTime>
#include <QJsonObject>

struct Note {
    int id = -1;
    QString title;
    QString content;
    int priority = 0;
    QString category;
    bool isCompleted = false;
    bool isPinned = false;
    QDateTime createdAt;
    QDateTime updatedAt;

    QJsonObject toJson() const {
        QJsonObject o;
        o["id"] = id;
        o["title"] = title;
        o["content"] = content;
        o["priority"] = priority;
        o["category"] = category;
        o["is_completed"] = isCompleted;
        o["is_pinned"] = isPinned;
        o["created_at"] = createdAt.toString("yyyy-MM-dd HH:mm:ss");
        o["updated_at"] = updatedAt.toString("yyyy-MM-dd HH:mm:ss");
        return o;
    }
};
