#include "mapstreamclient.h"
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>

// Протокол совпадает с серверным
enum class StreamRequest : quint8 {
    TILE_REQUEST = 0x01,
    ELEVATION_REQUEST = 0x02,
    METADATA_REQUEST = 0x03
};

enum class StreamResponse : quint8 {
    TILE_RESPONSE = 0x81,
    ELEVATION_RESPONSE = 0x82,
    METADATA_RESPONSE = 0x83,
    ERROR_RESPONSE = 0xFF
};

MapStreamClient::MapStreamClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &MapStreamClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &MapStreamClient::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error), 
            this, &MapStreamClient::onError);
    connect(m_socket, &QTcpSocket::readyRead, this, &MapStreamClient::onReadyRead);
}

void MapStreamClient::connectToServer(const QString &host, quint16 port)
{
    m_socket->connectToHost(host, port);
}

void MapStreamClient::disconnectFromServer()
{
    m_socket->disconnectFromHost();
}

bool MapStreamClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void MapStreamClient::requestTile(int zoom, int x, int y)
{
    if (!isConnected()) {
        emit errorOccurred("Not connected to server");
        return;
    }
    
    QByteArray request = createTileRequest(zoom, x, y);
    m_socket->write(request);
}

void MapStreamClient::requestElevation(double latitude, double longitude)
{
    if (!isConnected()) {
        emit errorOccurred("Not connected to server");
        return;
    }
    
    QByteArray request = createElevationRequest(latitude, longitude);
    m_socket->write(request);
}

void MapStreamClient::requestMetadata()
{
    if (!isConnected()) {
        emit errorOccurred("Not connected to server");
        return;
    }
    
    QByteArray request = createMetadataRequest();
    m_socket->write(request);
}

void MapStreamClient::onConnected()
{
    emit connected();
}

void MapStreamClient::onDisconnected()
{
    emit disconnected();
}

void MapStreamClient::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    emit errorOccurred(m_socket->errorString());
}

void MapStreamClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    
    while (m_buffer.size() >= 1) {
        // Читаем тип ответа
        quint8 responseType = static_cast<quint8>(m_buffer[0]);
        
        switch (static_cast<StreamResponse>(responseType)) {
        case StreamResponse::TILE_RESPONSE: {
            if (m_buffer.size() < 10) // Минимальный размер заголовка
                return;
            
            QDataStream stream(&m_buffer, QIODevice::ReadOnly);
            stream.setByteOrder(QDataStream::BigEndian);
            
            quint8 type;
            quint32 zoom, x, y;
            quint32 dataSize;
            
            stream >> type >> zoom >> x >> y >> dataSize;
            
            int headerSize = 1 + 4 + 4 + 4 + 4; // type + zoom + x + y + dataSize
            if (m_buffer.size() < headerSize + static_cast<int>(dataSize))
                return;
            
            QByteArray tileData = m_buffer.mid(headerSize, dataSize);
            QImage image = QImage::fromData(tileData);
            
            emit tileReceived(zoom, x, y, image);
            
            m_buffer.remove(0, headerSize + dataSize);
            break;
        }
        
        case StreamResponse::ELEVATION_RESPONSE: {
            if (m_buffer.size() < 25) // type + lat(8) + lon(8) + elevation(8)
                return;
            
            QDataStream stream(&m_buffer, QIODevice::ReadOnly);
            stream.setByteOrder(QDataStream::BigEndian);
            
            quint8 type;
            quint64 latBits, lonBits, elevBits;
            
            stream >> type >> latBits >> lonBits >> elevBits;
            
            double latitude, longitude, elevation;
            std::memcpy(&latitude, &latBits, sizeof(double));
            std::memcpy(&longitude, &lonBits, sizeof(double));
            std::memcpy(&elevation, &elevBits, sizeof(double));
            
            emit elevationReceived(latitude, longitude, elevation);
            
            m_buffer.remove(0, 25);
            break;
        }
        
        case StreamResponse::METADATA_RESPONSE: {
            if (m_buffer.size() < 6) // type + dataSize(4)
                return;
            
            QDataStream stream(&m_buffer, QIODevice::ReadOnly);
            stream.setByteOrder(QDataStream::BigEndian);
            
            quint8 type;
            quint32 dataSize;
            
            stream >> type >> dataSize;
            
            int headerSize = 1 + 4;
            if (m_buffer.size() < headerSize + static_cast<int>(dataSize))
                return;
            
            QByteArray jsonData = m_buffer.mid(headerSize, dataSize);
            QJsonDocument doc = QJsonDocument::fromJson(jsonData);
            
            if (!doc.isNull() && doc.isObject()) {
                emit metadataReceived(doc.object().toVariantMap());
            }
            
            m_buffer.remove(0, headerSize + dataSize);
            break;
        }
        
        case StreamResponse::ERROR_RESPONSE: {
            if (m_buffer.size() < 6) // type + errorCode(1) + messageSize(4)
                return;
            
            QDataStream stream(&m_buffer, QIODevice::ReadOnly);
            stream.setByteOrder(QDataStream::BigEndian);
            
            quint8 type;
            quint8 errorCode;
            quint32 messageSize;
            
            stream >> type >> errorCode >> messageSize;
            
            int headerSize = 1 + 1 + 4;
            if (m_buffer.size() < headerSize + static_cast<int>(messageSize))
                return;
            
            QString errorMessage = QString::fromUtf8(m_buffer.mid(headerSize, messageSize));
            emit errorOccurred(errorMessage);
            
            m_buffer.remove(0, headerSize + messageSize);
            break;
        }
        
        default:
            // Неизвестный тип ответа, пропускаем байт
            m_buffer.remove(0, 1);
            break;
        }
    }
}

QByteArray MapStreamClient::createTileRequest(int zoom, int x, int y)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    
    stream << static_cast<quint8>(StreamRequest::TILE_REQUEST)
           << static_cast<quint32>(zoom)
           << static_cast<quint32>(x)
           << static_cast<quint32>(y);
    
    return data;
}

QByteArray MapStreamClient::createElevationRequest(double latitude, double longitude)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    
    quint64 latBits, lonBits;
    std::memcpy(&latBits, &latitude, sizeof(double));
    std::memcpy(&lonBits, &longitude, sizeof(double));
    
    stream << static_cast<quint8>(StreamRequest::ELEVATION_REQUEST)
           << latBits << lonBits;
    
    return data;
}

QByteArray MapStreamClient::createMetadataRequest()
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    
    stream << static_cast<quint8>(StreamRequest::METADATA_REQUEST);
    
    return data;
}
