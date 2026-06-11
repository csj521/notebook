#pragma once
#include <QObject>
#include <QTcpServer>
#include <QThread>
#include <QMutex>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

class BridgeApi;

// Embedded HTTP REST API server for AI agent testing.
// Listens on localhost:18520, same API as the Python Flask version.
// Runs in its own thread.

class ApiServer : public QObject {
    Q_OBJECT
public:
    explicit ApiServer(BridgeApi* api, QObject* parent = nullptr);
    ~ApiServer() override;

    bool start(quint16 port = 18520);
    void stop();

    // Route handlers — override for custom endpoints
    std::function<QJsonObject(const QString& method, const QString& path,
                              const QJsonObject& body)> customHandler;

private slots:
    void onNewConnection();
    void onReadyRead();

private:
    void handleRequest(class QTcpSocket* socket, const QByteArray& data);
    QJsonObject routeRequest(const QString& method, const QString& path,
                             const QJsonObject& body);
    void sendResponse(QTcpSocket* socket, int statusCode, const QJsonDocument& body);
    void sendResponseRaw(QTcpSocket* socket, int statusCode,
                         const QString& contentType, const QByteArray& body);

    BridgeApi* m_api;
    QTcpServer* m_server = nullptr;
    QThread* m_thread = nullptr;
    QMap<QTcpSocket*, QByteArray> m_buffers;
    QMutex m_bufMutex;
};
