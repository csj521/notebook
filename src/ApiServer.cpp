#include "ApiServer.h"
#include "BridgeApi.h"
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QMutexLocker>
#include <QDebug>

ApiServer::ApiServer(BridgeApi* api, QObject* parent)
    : QObject(parent), m_api(api) {}

ApiServer::~ApiServer() { stop(); }

bool ApiServer::start(quint16 port) {
    m_thread = new QThread(this);
    m_server = new QTcpServer();
    m_server->moveToThread(m_thread);

    connect(m_thread, &QThread::started, [this]() {
        if (!m_server->listen(QHostAddress::LocalHost, 18520)) {
            qWarning() << "ApiServer: failed to listen on port 18520";
            return;
        }
        qDebug() << "ApiServer: listening on http://127.0.0.1:18520";
    });
    connect(m_server, &QTcpServer::newConnection, this, &ApiServer::onNewConnection);

    m_thread->start();
    return true;
}

void ApiServer::stop() {
    if (m_server) { m_server->close(); m_server->deleteLater(); m_server = nullptr; }
    if (m_thread) { m_thread->quit(); m_thread->wait(3000); m_thread = nullptr; }
}

void ApiServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket* sock = m_server->nextPendingConnection();
        connect(sock, &QTcpSocket::readyRead, this, &ApiServer::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);
        QMutexLocker lock(&m_bufMutex);
        m_buffers[sock] = QByteArray();
    }
}

void ApiServer::onReadyRead() {
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    {
        QMutexLocker lock(&m_bufMutex);
        m_buffers[sock].append(sock->readAll());
    }

    QByteArray buf;
    {
        QMutexLocker lock(&m_bufMutex);
        buf = m_buffers[sock];
    }

    // Check if we have a complete HTTP request (ends with \r\n\r\n + optional body)
    int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0) return;

    // Parse Content-Length for body (case-insensitive)
    int contentLength = 0;
    QString headers = QString::fromUtf8(buf.left(headerEnd));
    QRegularExpression clRe("Content-Length:\\s*(\\d+)",
                            QRegularExpression::CaseInsensitiveOption);
    auto clMatch = clRe.match(headers);
    if (clMatch.hasMatch())
        contentLength = clMatch.captured(1).toInt();

    int totalLen = headerEnd + 4 + contentLength;
    if (buf.size() < totalLen) return; // Not all body received yet

    handleRequest(sock, buf.left(totalLen));

    // Remove processed data from buffer
    {
        QMutexLocker lock(&m_bufMutex);
        m_buffers[sock] = m_buffers[sock].mid(totalLen);
    }
}

void ApiServer::handleRequest(QTcpSocket* socket, const QByteArray& data) {
    // Parse request line
    int firstSpace = data.indexOf(' ');
    int secondSpace = data.indexOf(' ', firstSpace + 1);
    if (firstSpace < 0 || secondSpace < 0) return;

    QByteArray method = data.mid(0, firstSpace);
    QByteArray path = data.mid(firstSpace + 1, secondSpace - firstSpace - 1);

    // Parse body
    QJsonObject body;
    int headerEnd = data.indexOf("\r\n\r\n");
    if (headerEnd >= 0 && data.size() > headerEnd + 4) {
        QByteArray bodyData = data.mid(headerEnd + 4);
        if (!bodyData.isEmpty()) {
            QJsonParseError err;
            QJsonDocument d = QJsonDocument::fromJson(bodyData, &err);
            if (err.error == QJsonParseError::NoError && d.isObject())
                body = d.object();
        }
    }

    // Route
    QJsonObject result = routeRequest(QString::fromUtf8(method), QString::fromUtf8(path), body);

    // Build response
    if (result.contains("__error__")) {
        sendResponse(socket, result["__status__"].toInt(500),
                     QJsonDocument(QJsonObject{{"error", result["__error__"]}}));
    } else if (result.contains("__raw__")) {
        sendResponseRaw(socket, result["__status__"].toInt(200),
                        "application/json", result["__raw__"].toString().toUtf8());
    } else {
        sendResponse(socket, result["__status__"].toInt(200), QJsonDocument(result));
    }
}

QJsonObject ApiServer::routeRequest(const QString& method, const QString& rawPath,
                                     const QJsonObject& body) {
    // Strip query string
    QString path = rawPath.section('?', 0, 0);

    // Custom handler first
    if (customHandler) {
        auto result = customHandler(method, path, body);
        if (!result.isEmpty()) return result;
    }

    auto ok = []() { QJsonObject o; o["__status__"] = 200; return o; };
    auto err = [](int code, const QString& msg) {
        QJsonObject o; o["__status__"] = code; o["__error__"] = msg; return o;
    };
    auto raw = [](int code, const QString& data) {
        QJsonObject o; o["__status__"] = code; o["__raw__"] = data; return o;
    };

    // GET /api/notes
    if (method == "GET" && path == "/api/notes") {
        QJsonObject o = ok();
        o["notes"] = m_api->getNotes();
        return o;
    }
    // GET /api/notes/{id}
    QRegularExpression noteRe("^/api/notes/(\\d+)$");
    auto noteMatch = noteRe.match(path);
    if (method == "GET" && noteMatch.hasMatch()) {
        int id = noteMatch.captured(1).toInt();
        QJsonObject n = m_api->getNote(id);
        if (n.isEmpty() || n["id"].toInt() < 0) return err(404, "not found");
        QJsonObject o = ok();
        for (auto it = n.begin(); it != n.end(); ++it) o[it.key()] = it.value();
        return o;
    }
    // POST /api/notes
    if (method == "POST" && path == "/api/notes") {
        QJsonObject n = m_api->addNote(body["title"].toString(), body["content"].toString(),
                                        body["priority"].toInt(), body["category"].toString());
        QJsonObject o = ok();
        for (auto it = n.begin(); it != n.end(); ++it) o[it.key()] = it.value();
        return o;
    }
    // PUT /api/notes/{id}
    auto putMatch = noteRe.match(path);
    if (method == "PUT" && putMatch.hasMatch()) {
        int id = putMatch.captured(1).toInt();
        QJsonObject n = m_api->updateNote(id, body);
        QJsonObject o = ok();
        for (auto it = n.begin(); it != n.end(); ++it) o[it.key()] = it.value();
        return o;
    }
    // DELETE /api/notes/{id}
    if (method == "DELETE" && noteMatch.hasMatch()) {
        int id = noteMatch.captured(1).toInt();
        m_api->deleteNote(id);
        return ok();
    }
    // POST /api/notes/{id}/toggle-complete
    QRegularExpression toggleRe("^/api/notes/(\\d+)/toggle-complete$");
    auto toggleMatch = toggleRe.match(path);
    if (method == "POST" && toggleMatch.hasMatch()) {
        int id = toggleMatch.captured(1).toInt();
        QJsonObject n = m_api->toggleNoteComplete(id);
        QJsonObject o = ok();
        for (auto it = n.begin(); it != n.end(); ++it) o[it.key()] = it.value();
        return o;
    }
    // POST /api/notes/{id}/toggle-pin
    QRegularExpression pinRe("^/api/notes/(\\d+)/toggle-pin$");
    auto pinMatch = pinRe.match(path);
    if (method == "POST" && pinMatch.hasMatch()) {
        int id = pinMatch.captured(1).toInt();
        QJsonObject n = m_api->toggleNotePin(id);
        QJsonObject o = ok();
        for (auto it = n.begin(); it != n.end(); ++it) o[it.key()] = it.value();
        return o;
    }

    // GET /api/reminders
    if (method == "GET" && path == "/api/reminders")
        { QJsonObject o = ok(); o["reminders"] = m_api->getReminders(); return o; }
    // POST /api/reminders
    if (method == "POST" && path == "/api/reminders") {
        QJsonObject r = m_api->addReminder(body);
        QJsonObject o = ok();
        for (auto it = r.begin(); it != r.end(); ++it) o[it.key()] = it.value();
        return o;
    }
    // DELETE /api/reminders/{id}
    QRegularExpression remRe("^/api/reminders/(\\d+)$");
    auto remMatch = remRe.match(path);
    if (method == "DELETE" && remMatch.hasMatch()) {
        m_api->deleteReminder(remMatch.captured(1).toInt());
        return ok();
    }
    // PUT /api/reminders/{id}
    if (method == "PUT" && remMatch.hasMatch()) {
        QJsonObject r = m_api->updateReminder(remMatch.captured(1).toInt(), body);
        QJsonObject o = ok();
        for (auto it = r.begin(); it != r.end(); ++it) o[it.key()] = it.value();
        return o;
    }

    // GET /api/settings/{key}
    QRegularExpression setRe("^/api/settings/(.+)$");
    auto setMatch = setRe.match(path);
    if (method == "GET" && setMatch.hasMatch())
        { QJsonObject o = ok(); o["value"] = m_api->getSetting(setMatch.captured(1)); return o; }
    // PUT /api/settings/{key}
    if (method == "PUT" && setMatch.hasMatch()) {
        m_api->setSetting(setMatch.captured(1), body["value"].toString());
        return ok();
    }

    // GET /api/auto-start
    if (method == "GET" && path == "/api/auto-start")
        { QJsonObject o = ok(); o["enabled"] = m_api->getAutoStart(); return o; }
    // PUT /api/auto-start
    if (method == "PUT" && path == "/api/auto-start") {
        bool on = body["enabled"].toBool();
        QJsonObject o = ok(); o["ok"] = m_api->setAutoStart(on); return o;
    }

    // GET /api/pending-reminders
    if (method == "GET" && path == "/api/pending-reminders")
        { QJsonObject o = ok(); o["reminders"] = m_api->getPendingReminders(); return o; }

    // POST /api/bridge/{cmd} — window controls
    QRegularExpression bridgeRe("^/api/bridge/(.+)$");
    auto bridgeMatch = bridgeRe.match(path);
    if (method == "POST" && bridgeMatch.hasMatch()) {
        QString cmd = bridgeMatch.captured(1);
        if (cmd == "minimize") m_api->minimizeWindow();
        else if (cmd == "close") m_api->closeWindow();
        else if (cmd == "toggle-on-top") m_api->toggleOnTop();
        return ok();
    }

    // GET /api/health
    if (method == "GET" && path == "/api/health")
        { QJsonObject o = ok(); o["status"] = "ok"; return o; }

    return err(404, "not found");
}

void ApiServer::sendResponse(QTcpSocket* socket, int statusCode, const QJsonDocument& body) {
    sendResponseRaw(socket, statusCode, "application/json", body.toJson(QJsonDocument::Compact));
}

void ApiServer::sendResponseRaw(QTcpSocket* socket, int statusCode,
                                 const QString& contentType, const QByteArray& body) {
    QString statusText = (statusCode == 200) ? "OK" :
                         (statusCode == 404) ? "Not Found" : "Error";
    QByteArray header = QString(
        "HTTP/1.1 %1 %2\r\n"
        "Content-Type: %3\r\n"
        "Content-Length: %4\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n"
    ).arg(statusCode).arg(statusText).arg(contentType).arg(body.size()).toUtf8();

    socket->write(header);
    socket->write(body);
    socket->flush();
    socket->disconnectFromHost();
}
