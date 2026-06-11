#include <QApplication>
#include <QTimer>
#include <QUrl>
#include <QWebEngineView>
#include <QWebEnginePage>
#include "MainWindow.h"
#include "BridgeApi.h"
#include "Storage.h"
#include "Scheduler.h"
#include "ApiServer.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Notebook");
    app.setOrganizationName("NotebookApp");
    app.setQuitOnLastWindowClosed(false);

    Storage::instance().init();

    MainWindow window;
    window.show();
    window.applyRoundedCorners();

    // Scheduler owned by MainWindow for proper lifecycle
    SchedulerWorker* scheduler = new SchedulerWorker(&window);
    window.setScheduler(scheduler);
    scheduler->start(30);

    BridgeApi api(&window, scheduler);
    window.setupBridge(&api);

    // Start HTTP API server for AI agent testing (localhost:18520)
    ApiServer apiServer(&api);
    apiServer.start(18520);

    // Load the HTML page
    QString htmlPath = QApplication::applicationDirPath() + "/assets/index.html";
    window.webView()->load(QUrl::fromLocalFile(htmlPath));

    // Start reminder polling ONLY after page is loaded and JS is ready
    QTimer* pollTimer = new QTimer(&window);
    bool* jsReady = new bool(false);
    QObject::connect(window.webView()->page(), &QWebEnginePage::loadFinished,
        [&window, pollTimer, jsReady](bool ok) {
            if (!ok) return;
            // Inject reminder callback
            window.webView()->page()->runJavaScript(R"(
                window.__onReminders = function(pending) {
                    pending.forEach(function(r) {
                        if (window.onReminderTriggered) window.onReminderTriggered(r);
                    });
                };
            )");
            *jsReady = true;
            pollTimer->start(5000);
        });

    QObject::connect(pollTimer, &QTimer::timeout, &window, [&window, jsReady]() {
        if (*jsReady) window.checkPendingReminders();
    });

    int ret = app.exec();

    scheduler->stop();
    return ret;
}
