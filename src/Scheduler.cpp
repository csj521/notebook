#include "Scheduler.h"
#include "Storage.h"
#include <QTimer>
#include <QDebug>

SchedulerWorker::SchedulerWorker(QObject* parent) : QObject(parent) {
    connect(this, &SchedulerWorker::reminderTriggered, this,
            [this](const Reminder& r) {
                QMutexLocker lock(&m_mutex);
                m_pending.append(r);
            });
}

SchedulerWorker::~SchedulerWorker() { stop(); }

void SchedulerWorker::start(int intervalSecs) {
    if (m_started) return;  // Prevent duplicate calls
    m_started = true;
    m_interval = intervalSecs;
    m_running.store(true);

    QTimer* timer = new QTimer(this);
    timer->moveToThread(&m_thread);
    connect(timer, &QTimer::timeout, [this, timer]() {
        if (!m_running.load()) { timer->stop(); return; }
        // Run at the actual configured interval
        static int tick = 0;
        if (++tick < m_interval) return;
        tick = 0;
        auto due = Storage::instance().getDueReminders();
        for (auto& r : due) {
            if (!m_running.load()) break;
            emit reminderTriggered(r);
            if (r.isRepeated && r.repeatIntervalMin > 0) {
                Reminder u = r;
                u.triggerAt = r.triggerAt.addSecs(r.repeatIntervalMin * 60);
                Storage::instance().updateReminder(r.id, u);
            } else {
                Reminder u = r;
                u.isEnabled = false;
                Storage::instance().updateReminder(r.id, u);
            }
        }
    });
    connect(&m_thread, &QThread::started, [timer]() { timer->start(1000); });
    m_thread.start();
}

void SchedulerWorker::stop() {
    m_running.store(false);
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait(5000);  // 5s timeout
    }
    m_started = false;
}

QVector<Reminder> SchedulerWorker::getPending() {
    QMutexLocker lock(&m_mutex);
    auto result = m_pending;
    m_pending.clear();
    return result;
}
