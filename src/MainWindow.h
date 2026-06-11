#pragma once
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTimer>
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

class QWebEngineView;
class QWebChannel;
class BridgeApi;
class SchedulerWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void setupBridge(BridgeApi* api);
    void setScheduler(SchedulerWorker* s) { m_scheduler = s; }
    void toggleOnTop();
    void setOnTop(bool on);
    void showAndRaise();
    void updateThemeBg(const QString& theme);
    void checkPendingReminders();
    void applyRoundedCorners();
    QWebEngineView* webView() const { return m_webView; }

protected:
    void closeEvent(QCloseEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void setupUi();
    void setupTray();
    void loadSettings();
    void saveSettings();
    void applyOnTop();
    void registerGlobalHotkey();
    void unregisterGlobalHotkey();

    QWebEngineView* m_webView = nullptr;
    QWebChannel* m_channel = nullptr;
    QSystemTrayIcon* m_tray = nullptr;
    BridgeApi* m_api = nullptr;
    SchedulerWorker* m_scheduler = nullptr;
    QTimer* m_pendingTimer = nullptr;

    bool m_onTop = false;
    bool m_closeToTray = true;
    bool m_initDone = false;
    bool m_hotkeyRegistered = false;
    static constexpr int TITLEBAR_H = 40;

    // Window drag
    bool m_dragging = false;
    POINT m_dragStart = {};
    POINT m_dragWinPos = {};
};
