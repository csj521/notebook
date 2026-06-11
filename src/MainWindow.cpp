#include "MainWindow.h"
#include "BridgeApi.h"
#include "Storage.h"
#include "Scheduler.h"
#include <QWebEngineView>
#include <QWebChannel>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QApplication>
#include <QPainter>
#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    setupTray();
    loadSettings();
    registerGlobalHotkey();
    m_initDone = true;
}

MainWindow::~MainWindow() {
    unregisterGlobalHotkey();
    saveSettings();
}

void MainWindow::setupUi() {
    setWindowTitle(QString::fromUtf8("记事簿"));
    resize(420, 720);
    setMinimumSize(340, 500);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);

    QWidget* central = new QWidget(this);
    central->setStyleSheet("background: #f2f2f7;");
    QVBoxLayout* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_webView = new QWebEngineView(this);
    m_webView->page()->setBackgroundColor(Qt::transparent);
    layout->addWidget(m_webView);
    setCentralWidget(central);
}

void MainWindow::setupBridge(BridgeApi* api) {
    m_api = api;
    m_channel = new QWebChannel(this);
    m_channel->registerObject("api", api);
    m_webView->page()->setWebChannel(m_channel);

    connect(api, &BridgeApi::toggleOnTopRequested, this, &MainWindow::toggleOnTop);
    connect(api, &BridgeApi::alwaysOnTopChanged, this, &MainWindow::setOnTop);
    connect(api, &BridgeApi::themeChanged, this, &MainWindow::updateThemeBg);
}

void MainWindow::setupTray() {
    m_tray = new QSystemTrayIcon(this);
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setFont(QFont("Segoe UI", 18));
    p.drawText(pixmap.rect(), Qt::AlignCenter, QString::fromUtf8("📓"));
    p.end();
    m_tray->setIcon(QIcon(pixmap));
    m_tray->setToolTip(QString::fromUtf8("记事簿"));

    QMenu* menu = new QMenu(this);
    QAction* showAct = menu->addAction(QString::fromUtf8("显示"));
    connect(showAct, &QAction::triggered, this, &MainWindow::showAndRaise);
    QAction* quitAct = menu->addAction(QString::fromUtf8("退出"));
    connect(quitAct, &QAction::triggered, this, [this]() {
        if (m_scheduler) m_scheduler->stop();
        m_tray->hide();
        QApplication::quit();
    });
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::DoubleClick) showAndRaise();
    });
    m_tray->show();
}

void MainWindow::loadSettings() {
    QSettings settings("Notebook", "NotebookApp");
    QByteArray geo = settings.value("window/geometry").toByteArray();
    if (!geo.isEmpty()) restoreGeometry(geo);
    m_onTop = Storage::instance().getSetting("always_on_top") == "true";
    m_closeToTray = Storage::instance().getSetting("close_to_tray") != "false";
    if (m_onTop) applyOnTop();
}

void MainWindow::saveSettings() {
    QSettings settings("Notebook", "NotebookApp");
    settings.setValue("window/geometry", saveGeometry());
}

void MainWindow::applyRoundedCorners() {
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) return;
    int val = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &val, sizeof(val));
}

void MainWindow::applyOnTop() {
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) return;
    SetWindowPos(hwnd, m_onTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void MainWindow::toggleOnTop() {
    m_onTop = !m_onTop;
    Storage::instance().setSetting("always_on_top", m_onTop ? "true" : "false");
    applyOnTop();
}

void MainWindow::setOnTop(bool on) {
    m_onTop = on;
    Storage::instance().setSetting("always_on_top", on ? "true" : "false");
    applyOnTop();
}

void MainWindow::showAndRaise() {
    show();
    applyRoundedCorners();
    raise();
    activateWindow();
}

void MainWindow::updateThemeBg(const QString& theme) {
    QString bg = "#f2f2f7";
    if (theme == "dark") bg = "#0a0a0f";
    else if (theme == "warm") bg = "#faf8f5";
    else if (theme == "minimal") bg = "#ffffff";
    centralWidget()->setStyleSheet(QString("background: %1;").arg(bg));
}

void MainWindow::checkPendingReminders() {
    if (!m_webView || !m_scheduler) return;
    auto pending = m_scheduler->getPending();
    if (pending.isEmpty()) return;
    QString js = "if(window.__onReminders) window.__onReminders(";
    QJsonArray arr;
    for (auto& r : pending) arr.append(r.toJson());
    js += QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    js += ");";
    m_webView->page()->runJavaScript(js);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    if (m_closeToTray && m_initDone) {
        hide();
        event->ignore();
    } else {
        if (m_scheduler) m_scheduler->stop();
        m_tray->hide();
        event->accept();
        QApplication::quit();
    }
}

bool MainWindow::nativeEvent(const QByteArray&, void* message, qintptr* result) {
    MSG* msg = static_cast<MSG*>(message);
    static constexpr UINT WM_HOTKEY_MSG = 0x0312;

    if (msg->message == WM_HOTKEY_MSG && msg->wParam == 1) {
        if (isHidden() || isMinimized()) showAndRaise();
        else hide();
        return true;
    }

    // Window drag via titlebar
    if (msg->message == WM_LBUTTONDOWN && !m_dragging) {
        POINT pt = {GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
        RECT rc; GetWindowRect(reinterpret_cast<HWND>(winId()), &rc);
        int localY = pt.y;
        int localX = pt.x;
        int width = rc.right - rc.left;
        if (localY >= 0 && localY <= TITLEBAR_H && localX >= 0 && localX < width - 120) {
            m_dragging = true;
            m_dragStart = pt;
            ClientToScreen(reinterpret_cast<HWND>(winId()), &m_dragStart);
            m_dragWinPos.x = rc.left;
            m_dragWinPos.y = rc.top;
            return true;
        }
    } else if (msg->message == WM_MOUSEMOVE && m_dragging) {
        POINT pt = {GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
        ClientToScreen(reinterpret_cast<HWND>(winId()), &pt);
        int dx = pt.x - m_dragStart.x;
        int dy = pt.y - m_dragStart.y;
        SetWindowPos(reinterpret_cast<HWND>(winId()), nullptr,
                     m_dragWinPos.x + dx, m_dragWinPos.y + dy,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);
        return true;
    } else if (msg->message == WM_LBUTTONUP && m_dragging) {
        m_dragging = false;
        return true;
    }
    return false;
}

void MainWindow::registerGlobalHotkey() {
    m_hotkeyRegistered = RegisterHotKey(reinterpret_cast<HWND>(winId()), 1,
                                        MOD_CONTROL | MOD_SHIFT, 'N');
}

void MainWindow::unregisterGlobalHotkey() {
    if (m_hotkeyRegistered) {
        UnregisterHotKey(reinterpret_cast<HWND>(winId()), 1);
        m_hotkeyRegistered = false;
    }
}
