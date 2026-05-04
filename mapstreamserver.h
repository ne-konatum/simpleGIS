#ifndef MAPSTREAMSERVER_H
#define MAPSTREAMSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

class MBTilesViewer;
class DEMReader;

// Структура запроса от клиента
struct StreamRequest {
    enum Type {
        TILE_REQUEST = 1,      // Запрос тайла карты
        ELEVATION_REQUEST = 2, // Запрос высоты
        METADATA_REQUEST = 3   // Запрос метаданных
    };
    
    quint8 type;
    quint8 zoom;
    quint32 x;
    quint32 y;
    double latitude;
    double longitude;
};

class MapStreamServer : public QTcpServer
{
    Q_OBJECT
    
public:
    explicit MapStreamServer(QObject *parent = nullptr);
    ~MapStreamServer();
    
    bool start(quint16 port);
    void stop();
    
    void setMBTilesViewer(MBTilesViewer *viewer);
    void setDEMReader(DEMReader *demReader);
    
    bool isRunning() const { return m_server != nullptr && m_server->isListening(); }
    quint16 serverPort() const { return m_server ? m_server->serverPort() : 0; }

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientDisconnected();
    void onReadyRead();

private:
    void processRequest(QTcpSocket *clientSocket, const QByteArray &data);
    void handleTileRequest(QTcpSocket *clientSocket, const StreamRequest &request);
    void handleElevationRequest(QTcpSocket *clientSocket, const StreamRequest &request);
    void handleMetadataRequest(QTcpSocket *clientSocket);
    
    void sendResponse(QTcpSocket *clientSocket, const QByteArray &data);
    void sendError(QTcpSocket *clientSocket, const QString &message);
    
    QTcpServer *m_server;
    MBTilesViewer *m_viewer;
    DEMReader *m_demReader;
    QList<QTcpSocket*> m_clients;
};

#endif // MAPSTREAMSERVER_H
