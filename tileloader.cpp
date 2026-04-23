#include "tileloader.h"
#include <QImage>
#include <QBuffer>
#include <QSqlQuery>
#include <QSqlError>
#include <QPainter>
#include <QDebug>
#include <QUuid>

QPixmap TileLoadTask::createPlaceholderTile(int x, int y)
{
    QPixmap tile(256, 256);
    tile.fill(Qt::lightGray);
    QPainter painter(&tile);
    painter.setPen(Qt::darkGray);
    painter.drawRect(0, 0, 255, 255);
    painter.drawText(tile.rect(), Qt::AlignCenter,
                    QString("Tile %1,%2").arg(x).arg(y));
    return tile;
}

QPixmap TileLoadTask::loadTileFromDatabase()
{
    QSqlQuery query(m_database);
    query.prepare("SELECT tile_data FROM tiles WHERE zoom_level = :zoom AND tile_column = :x AND tile_row = :y");
    query.bindValue(":zoom", m_z);
    query.bindValue(":x", m_x);
    // Преобразование TMS → Google Maps (инвертирование Y)
    int tmsY = (1 << m_z) - 1 - m_y;
    query.bindValue(":y", tmsY);

    QPixmap tile;

    if (query.exec() && query.next()) {
        QByteArray tileData = query.value(0).toByteArray();
        if (tileData.isEmpty()) {
            qDebug() << "Empty tile data for" << m_z << m_x << m_y;
            tile = createPlaceholderTile(m_x, m_y);
        } else if (tile.loadFromData(tileData)) {
            qDebug() << "TileLoadTask: tile loaded successfully" << m_z << m_x << m_y
                      << "size:" << tile.size();
        } else {
            qWarning() << "TileLoadTask: failed to load tile data from database";
            tile = createPlaceholderTile(m_x, m_y);
        }
    } else {
        qDebug() << "TileLoadTask: no tile data for" << m_z << m_x << m_y;
        tile = createPlaceholderTile(m_x, m_y);
    }

    return tile;
}

void TileLoadTask::run()
{
    QPixmap tile = loadTileFromDatabase();
    QImage image = tile.toImage();

    m_loader->tileLoaded(m_z, m_x, m_y, image);
}


TileLoader::TileLoader(QObject *parent)
    : QObject(parent)
{
    m_threadPool = new QThreadPool(this);
    m_threadPool->setMaxThreadCount(4);  // Ограничиваем число потоков
}

TileLoader::~TileLoader()
{
    m_threadPool->waitForDone();  // Ждём завершения всех задач
}

void TileLoader::loadTileAsync(const QSqlDatabase &database, int zoom, int x, int y)
{
    TileLoadTask *task = new TileLoadTask(database, zoom, x, y, this);
    m_threadPool->start(task);
}

void TileLoader::onTileLoadFinished(int z, int x, int y, const QPixmap &tile)
{
    QImage image = tile.toImage();  // Конвертируем QPixmap в QImage
    emit tileLoaded(z, x, y, image);  // Теперь тип соответствует сигналу
}

