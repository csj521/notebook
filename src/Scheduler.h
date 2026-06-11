#pragma once
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QVector>
#include <atomic>
#include "Reminder.h"

class SchedulerWorker : public QObject {
    Q_OBJECT
public:
    explicit SchedulerWorker(QObject* parent = nullptr);
    ~SchedulerWorker() override;

    void start(int intervalSecs = 30);
    void stop();
    QVector<Reminder> getPending();

signals:
    void reminderTriggered(const Reminder& r);

private:
    QThread m_thread;
    QMutex m_mutex;
    QVector<Reminder> m_pending;
    std::atomic<bool> m_running{false};
    int m_interval = 30;
    bool m_started = false;  // Prevent duplicate start()
};
