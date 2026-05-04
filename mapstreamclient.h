#ifndef MAPSTREAMCLIENT_H
#define MAPSTREAMCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QImage>
#include <QPoint>
#include <QVariant>

class MapStreamClient : public QObject
{
    Q_OBJECT

public:
    explicit MapStreamClient(QObject *parent = nullptr);
    
    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;
    
    // Запрос тайла карты
    void requestTile(int zoom, int x, int y);
    
    // Запрос высоты для координат (широта, долгота)
    void requestElevation(double latitude, double longitude);
    
    // Запрос метаданных
    void requestMetadata();

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    
    void tileReceived(int zoom, int x, int y, const QImage &image);
    void elevationReceived(double latitude, double longitude, double elevation);
    void metadataReceived(const QVariantMap &metadata);

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError socketError);
    void onReadyRead();

private:
    QByteArray createTileRequest(int zoom, int x, int y);
    QByteArray createElevationRequest(double latitude, double longitude);
    QByteArray createMetadataRequest();
    
    QTcpSocket *m_socket;
    QByteArray m_buffer;
};

#endif // MAPSTREAMCLIENT_H
