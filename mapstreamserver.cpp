#include "mapstreamserver.h"
#include "mbtilesviewer.h"
#include "demreader.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// Протокол:
// Запрос (12 байт заголовок + опциональные данные):
// [0] - тип запроса (1=TILE, 2=ELEVATION, 3=METADATA)
// [1] - zoom (для TILE)
// [2-5] - x (quint32, big-endian)
// [6-9] - y (quint32, big-endian)
// [10-11] - зарезервировано
// Для ELEVATION: вместо x,y используются latitude/longitude как double (байты 2-9 и 10-17)

// Ответ:
// [0-3] - длина данных (quint32, big-endian)
// [4] - статус (0=OK, 1=ERROR)
// [5...] - данные (PNG для тайла, double для высоты, JSON для метаданных)

MapStreamServer::MapStreamServer(QObject *parent)
    : QTcpServer(parent)
    , m_server(nullptr)
    , m_viewer(nullptr)
    , m_demReader(nullptr)
{
}

MapStreamServer::~MapStreamServer()
{
    stop();
}

bool MapStreamServer::start(quint16 port)
{
    if (m_server && m_server->isListening()) {
        return true;
    }
    
    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::Any, port)) {
        qWarning() << "MapStreamServer: Failed to start on port" << port << ":" << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }
    
    connect(m_server, &QTcpServer::newConnection, this, [this]() {
        while (QTcpSocket *clientSocket = m_server->nextPendingConnection()) {
            m_clients.append(clientSocket);
            
            connect(clientSocket, &QTcpSocket::disconnected,
                    this, &MapStreamServer::onClientDisconnected);
            connect(clientSocket, &QTcpSocket::readyRead,
                    this, &MapStreamServer::onReadyRead);
            
            qDebug() << "MapStreamServer: Client connected from" << clientSocket->peerAddress().toString();
        }
    });
    
    qDebug() << "MapStreamServer: Started on port" << m_server->serverPort();
    return true;
}

void MapStreamServer::stop()
{
    if (m_server) {
        // Закрываем все клиентские соединения
        for (QTcpSocket *client : m_clients) {
            client->disconnectFromHost();
            if (client->state() != QAbstractSocket::UnconnectedState) {
                client->waitForDisconnected(1000);
            }
            client->deleteLater();
        }
        m_clients.clear();
        
        m_server->close();
        delete m_server;
        m_server = nullptr;
        
        qDebug() << "MapStreamServer: Stopped";
    }
}

void MapStreamServer::setMBTilesViewer(MBTilesViewer *viewer)
{
    m_viewer = viewer;
}

void MapStreamServer::setDEMReader(DEMReader *demReader)
{
    m_demReader = demReader;
}

void MapStreamServer::incomingConnection(qintptr socketDescriptor)
{
    // Эта функция переопределена, но мы используем другой подход через newConnection
    // Оставляем пустой или вызываем базовую версию если нужно
    QTcpServer::incomingConnection(socketDescriptor);
}

void MapStreamServer::onClientDisconnected()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (clientSocket) {
        m_clients.removeAll(clientSocket);
        qDebug() << "MapStreamServer: Client disconnected";
        clientSocket->deleteLater();
    }
}

void MapStreamServer::onReadyRead()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;
    
    QByteArray data = clientSocket->readAll();
    if (data.size() >= 1) {
        processRequest(clientSocket, data);
    }
}

void MapStreamServer::processRequest(QTcpSocket *clientSocket, const QByteArray &data)
{
    if (data.size() < 1) {
        sendError(clientSocket, "Invalid request: too short");
        return;
    }
    
    StreamRequest request;
    request.type = static_cast<quint8>(data[0]);
    
    switch (request.type) {
        case StreamRequest::TILE_REQUEST:
            if (data.size() < 10) {
                sendError(clientSocket, "Invalid tile request: insufficient data");
                return;
            }
            request.zoom = static_cast<quint8>(data[1]);
            // Читаем x и y как big-endian quint32
            request.x = (static_cast<quint32>(static_cast<quint8>(data[2])) << 24) |
                        (static_cast<quint32>(static_cast<quint8>(data[3])) << 16) |
                        (static_cast<quint32>(static_cast<quint8>(data[4])) << 8) |
                        static_cast<quint32>(static_cast<quint8>(data[5]));
            request.y = (static_cast<quint32>(static_cast<quint8>(data[6])) << 24) |
                        (static_cast<quint32>(static_cast<quint8>(data[7])) << 16) |
                        (static_cast<quint32>(static_cast<quint8>(data[8])) << 8) |
                        static_cast<quint32>(static_cast<quint8>(data[9]));
            handleTileRequest(clientSocket, request);
            break;
            
        case StreamRequest::ELEVATION_REQUEST: {
            if (data.size() < 17) {
                sendError(clientSocket, "Invalid elevation request: insufficient data");
                return;
            }
            // Читаем latitude и longitude как big-endian double
            quint64 latBits = 0, lonBits = 0;
            for (int i = 0; i < 8; ++i) {
                latBits = (latBits << 8) | static_cast<quint8>(data[2 + i]);
                lonBits = (lonBits << 8) | static_cast<quint8>(data[10 + i]);
            }
            memcpy(&request.latitude, &latBits, sizeof(double));
            memcpy(&request.longitude, &lonBits, sizeof(double));
            handleElevationRequest(clientSocket, request);
            break;
        }
        
        case StreamRequest::METADATA_REQUEST:
            handleMetadataRequest(clientSocket);
            break;
            
        default:
            sendError(clientSocket, QString("Unknown request type: %1").arg(request.type));
            break;
    }
}

void MapStreamServer::handleTileRequest(QTcpSocket *clientSocket, const StreamRequest &request)
{
    if (!m_viewer || !m_viewer->isMapLoaded()) {
        sendError(clientSocket, "No MBTiles file loaded");
        return;
    }
    
    // Конвертация XYZ -> TMS (как в MBTilesViewer)
    int tmsY = ((1 << request.zoom) - 1) - static_cast<int>(request.y);
    
    // Проверяем наличие тайла в базе
    QSqlDatabase db = QSqlDatabase::database("MBTILES_CONNECTION");
    if (!db.isOpen()) {
        sendError(clientSocket, "Database not open");
        return;
    }
    
    QSqlQuery query(db);
    query.prepare("SELECT tile_data FROM tiles WHERE zoom_level = :z AND tile_column = :x AND tile_row = :y");
    query.bindValue(":z", request.zoom);
    query.bindValue(":x", request.x);
    query.bindValue(":y", tmsY);
    
    if (!query.exec()) {
        sendError(clientSocket, "Database query failed: " + query.lastError().text());
        return;
    }
    
    if (!query.next()) {
        sendError(clientSocket, "Tile not found");
        return;
    }
    
    QByteArray tileData = query.value(0).toByteArray();
    if (tileData.isEmpty()) {
        sendError(clientSocket, "Empty tile data");
        return;
    }
    
    // Формируем ответ: [length:4][status:1][data:N]
    QByteArray response;
    response.reserve(5 + tileData.size());
    
    // Длина данных (big-endian)
    quint32 dataLen = static_cast<quint32>(tileData.size());
    response.append(static_cast<char>((dataLen >> 24) & 0xFF));
    response.append(static_cast<char>((dataLen >> 16) & 0xFF));
    response.append(static_cast<char>((dataLen >> 8) & 0xFF));
    response.append(static_cast<char>(dataLen & 0xFF));
    
    // Статус OK
    response.append(static_cast<char>(0));
    
    // Данные тайла
    response.append(tileData);
    
    sendResponse(clientSocket, response);
    qDebug() << "MapStreamServer: Sent tile z=" << request.zoom << "x=" << request.x << "y=" << request.y;
}

void MapStreamServer::handleElevationRequest(QTcpSocket *clientSocket, const StreamRequest &request)
{
    if (!m_demReader) {
        sendError(clientSocket, "DEM reader not initialized");
        return;
    }
    
    double height = 0.0;
    bool found = false;
    
    // Пробуем получить высоту из текущего загруженного файла
    if (m_demReader->isLoaded()) {
        if (m_demReader->getElevation(request.latitude, request.longitude, height)) {
            found = true;
        } else {
            // Если текущий файл не покрывает точку, пробуем найти и загрузить новый
            if (m_demReader->updateForLocation(request.latitude, request.longitude)) {
                if (m_demReader->getElevation(request.latitude, request.longitude, height)) {
                    found = true;
                }
            }
        }
    } else {
        // Файл еще не загружен, пытаемся загрузить для этой точки
        if (m_demReader->updateForLocation(request.latitude, request.longitude)) {
            if (m_demReader->getElevation(request.latitude, request.longitude, height)) {
                found = true;
            }
        }
    }
    
    // Формируем ответ: [length:4][status:1][height:8]
    QByteArray response;
    response.reserve(13);
    
    // Длина данных (8 байт для double)
    quint32 dataLen = 8;
    response.append(static_cast<char>((dataLen >> 24) & 0xFF));
    response.append(static_cast<char>((dataLen >> 16) & 0xFF));
    response.append(static_cast<char>((dataLen >> 8) & 0xFF));
    response.append(static_cast<char>(dataLen & 0xFF));
    
    // Статус (0=OK, 1=not found)
    response.append(static_cast<char>(found ? 0 : 1));
    
    // Высота (big-endian double)
    quint64 heightBits;
    memcpy(&heightBits, &height, sizeof(double));
    for (int i = 7; i >= 0; --i) {
        response.append(static_cast<char>((heightBits >> (i * 8)) & 0xFF));
    }
    
    sendResponse(clientSocket, response);
    
    if (found) {
        qDebug() << "MapStreamServer: Sent elevation lat=" << request.latitude << "lon=" << request.longitude << "h=" << height;
    } else {
        qDebug() << "MapStreamServer: Elevation not found for lat=" << request.latitude << "lon=" << request.longitude;
    }
}

void MapStreamServer::handleMetadataRequest(QTcpSocket *clientSocket)
{
    if (!m_viewer || !m_viewer->isMapLoaded()) {
        sendError(clientSocket, "No MBTiles file loaded");
        return;
    }
    
    QJsonObject metadata;
    metadata["minzoom"] = m_viewer->getMinZoom();
    metadata["maxzoom"] = m_viewer->getMaxZoom();
    metadata["scheme"] = m_viewer->getMetadata("scheme");
    metadata["name"] = m_viewer->getMetadata("name");
    metadata["description"] = m_viewer->getMetadata("description");
    metadata["version"] = m_viewer->getMetadata("version");
    
    QJsonDocument doc(metadata);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    
    // Формируем ответ: [length:4][status:1][data:N]
    QByteArray response;
    response.reserve(5 + jsonData.size());
    
    // Длина данных (big-endian)
    quint32 dataLen = static_cast<quint32>(jsonData.size());
    response.append(static_cast<char>((dataLen >> 24) & 0xFF));
    response.append(static_cast<char>((dataLen >> 16) & 0xFF));
    response.append(static_cast<char>((dataLen >> 8) & 0xFF));
    response.append(static_cast<char>(dataLen & 0xFF));
    
    // Статус OK
    response.append(static_cast<char>(0));
    
    // JSON данные
    response.append(jsonData);
    
    sendResponse(clientSocket, response);
    qDebug() << "MapStreamServer: Sent metadata";
}

void MapStreamServer::sendResponse(QTcpSocket *clientSocket, const QByteArray &data)
{
    if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState) {
        clientSocket->write(data);
        clientSocket->flush();
    }
}

void MapStreamServer::sendError(QTcpSocket *clientSocket, const QString &message)
{
    qWarning() << "MapStreamServer: Error:" << message;
    
    // Формируем ответ с ошибкой: [length:4][status:1][message:N]
    QByteArray errorMsg = message.toUtf8();
    QByteArray response;
    response.reserve(5 + errorMsg.size());
    
    // Длина данных (big-endian)
    quint32 dataLen = static_cast<quint32>(errorMsg.size());
    response.append(static_cast<char>((dataLen >> 24) & 0xFF));
    response.append(static_cast<char>((dataLen >> 16) & 0xFF));
    response.append(static_cast<char>((dataLen >> 8) & 0xFF));
    response.append(static_cast<char>(dataLen & 0xFF));
    
    // Статус ERROR
    response.append(static_cast<char>(1));
    
    // Сообщение об ошибке
    response.append(errorMsg);
    
    sendResponse(clientSocket, response);
}
