#include "mbtilesviewer.h"
#include <QFileInfo>
#include <QSqlRecord>
#include <QVariant>
#include <QColor>
#include <QtMath>
#include <algorithm>

MBTilesViewer::MBTilesViewer(QWidget *parent)
    : QWidget(parent)
    , m_dbReady(false)
    , m_currentZoom(0)
    , m_minZoom(0)
    , m_maxZoom(18)
    , m_tileSize(256)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE", "MBTILES_CONNECTION");
    // При синхронной загрузке сигнал не обязателен, но оставим подключение
    connect(this, &MBTilesViewer::tileLoaded, this, &MBTilesViewer::onTileLoaded, Qt::QueuedConnection);
}

MBTilesViewer::~MBTilesViewer()
{
    if (m_db.isOpen())
        m_db.close();
}

void MBTilesViewer::openFile(const QString &filePath)
{
    if (m_db.isOpen())
        m_db.close();

    m_db.setDatabaseName(filePath);
    if (!m_db.open()) {
        qDebug() << "Error opening database:" << m_db.lastError().text();
        m_dbReady = false;
        return;
    }
    m_dbReady = true;
    qDebug() << "MBTiles database opened successfully:" << filePath;

    m_scheme = getMetadata("scheme").toLower();
    if (m_scheme.isEmpty()) m_scheme = "tms";

    QString minZStr = getMetadata("minzoom");
    QString maxZStr = getMetadata("maxzoom");
    if (!minZStr.isEmpty()) m_minZoom = minZStr.toInt();
    if (!maxZStr.isEmpty()) m_maxZoom = maxZStr.toInt();

    qDebug() << "Metadata scheme:" << m_scheme << "minzoom:" << m_minZoom << "maxzoom:" << m_maxZoom;

    scanAvailableTiles();
    calculateInitialView(); // Вместо resetView используем умное центрирование

    update();
}

void MBTilesViewer::scanAvailableTiles() {
    m_availableTiles.clear();
    m_zoomBounds.clear();

    QSqlQuery query(m_db);
    if (query.exec("SELECT zoom_level, tile_column, tile_row FROM tiles")) {
        while (query.next()) {
            int z = query.value(0).toInt();
            int x = query.value(1).toInt();
            int y = query.value(2).toInt();

            // Сохраняем как есть (TMS координаты из БД)
            TileKey key(z, x, y);
            m_availableTiles.insert(key);

            // Обновляем границы для этого зума
            if (!m_zoomBounds.contains(z)) {
                m_zoomBounds[z] = TileBounds();
            }
            TileBounds &bounds = m_zoomBounds[z];
            bounds.minX = std::min(bounds.minX, x);
            bounds.maxX = std::max(bounds.maxX, x);
            bounds.minY = std::min(bounds.minY, y);
            bounds.maxY = std::max(bounds.maxY, y);
        }
    }
    qDebug() << "Scanned" << m_availableTiles.size() << "available tiles from database.";
}

void MBTilesViewer::calculateInitialView() {
    if (m_zoomBounds.isEmpty()) {
        qDebug() << "No tiles found in database!";
        return;
    }

    // Находим зум с наибольшим количеством тайлов или самый детальный, где есть данные
    int bestZoom = m_maxZoom;
    while (bestZoom >= m_minZoom && !m_zoomBounds.contains(bestZoom)) {
        bestZoom--;
    }

    if (bestZoom < m_minZoom) {
        qDebug() << "No valid zoom levels found!";
        return;
    }

    m_currentZoom = bestZoom;

    // Если хотим начать с конкретного зума (например 12), но проверяем наличие данных
    // Если на 12 нет данных, берем ближайший доступный
    int targetZoom = 12;
    if (m_zoomBounds.contains(targetZoom)) {
        m_currentZoom = targetZoom;
    } else {
        // Ищем ближайший доступный зум к 12
        int diff = 1;
        while (true) {
            if (m_zoomBounds.contains(targetZoom + diff)) {
                m_currentZoom = targetZoom + diff;
                break;
            }
            if (targetZoom - diff >= m_minZoom && m_zoomBounds.contains(targetZoom - diff)) {
                m_currentZoom = targetZoom - diff;
                break;
            }
            diff++;
            if (targetZoom + diff > m_maxZoom && targetZoom - diff < m_minZoom) break;
        }
    }

    const TileBounds &bounds = m_zoomBounds[m_currentZoom];

    // Вычисляем центр доступной области в координатах TMS
    double centerX = (bounds.minX + bounds.maxX) / 2.0;
    double centerY = (bounds.minY + bounds.maxY) / 2.0;

    // Конвертируем TMS Y в XYZ для отображения
    // TMS Y=0 находится внизу, XYZ Y=0 вверху.
    // Но для центрирования нам важно просто попасть в область.
    // Координаты в БД (TMS): y_tms.
    // Координаты для отрисовки (XYZ): y_xyz = (2^z - 1) - y_tms.

    double centerXYZ_Y = ((1 << m_currentZoom) - 1) - centerY;

    // Переводим в пиксели
    double pixelX = centerX * m_tileSize;
    double pixelY = centerXYZ_Y * m_tileSize;

    // Центрируем окно
    m_offset.setX(pixelX - width() / 2.0);
    m_offset.setY(pixelY - height() / 2.0);

    m_tileCache.clear();

    qDebug() << "Initial view set: Zoom" << m_currentZoom
             << "Center Tile (XYZ):" << centerX << centerXYZ_Y
             << "Bounds (TMS):" << bounds.minX << bounds.minY << "-" << bounds.maxX << bounds.maxY;

    updateViewport();
}

QString MBTilesViewer::getMetadata(const QString &name)
{
    if (!m_dbReady) return QString();
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM metadata WHERE name = :name");
    query.bindValue(":name", name);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

int MBTilesViewer::getMinZoom() { return m_minZoom; }
int MBTilesViewer::getMaxZoom() { return m_maxZoom; }

void MBTilesViewer::resetView() {
    // Оставлено для совместимости, но теперь используется calculateInitialView
    calculateInitialView();
}

void MBTilesViewer::updateViewport() {
    QRectF visibleRect = getVisibleTileRect();
    int startX = static_cast<int>(visibleRect.left());
    int startY = static_cast<int>(visibleRect.top());
    int endX = static_cast<int>(visibleRect.right());
    int endY = static_cast<int>(visibleRect.bottom());

    // Загружаем тайлы с запасом
    for (int x = startX - 1; x <= endX + 1; ++x) {
        for (int y = startY - 1; y <= endY + 1; ++y) {
            loadTile(m_currentZoom, x, y);
        }
    }
    update();
}

void MBTilesViewer::loadTile(int z, int x, int y) {
    if (!m_db.isOpen()) return;

    // Конвертация XYZ (отрисовка) -> TMS (база)
    int tmsY = ((1 << z) - 1) - y;
    TileKey key(z, x, tmsY);

    if (m_tileCache.contains(key)) return;
    if (!m_availableTiles.contains(key)) return;

    QSqlQuery query(m_db);
    query.prepare("SELECT tile_data FROM tiles WHERE zoom_level = :z AND tile_column = :x AND tile_row = :y");
    query.bindValue(":z", z);
    query.bindValue(":x", x);
    query.bindValue(":y", tmsY);

    if (query.exec() && query.next()) {
        QByteArray data = query.value(0).toByteArray();
        QImage img;
        if (img.loadFromData(data)) {
            m_tileCache.insert(key, img);
            // emit tileLoaded(z, x, y, img); // Не нужно при синхронной загрузке
        }
    }
}

void MBTilesViewer::onTileLoaded(int z, int x, int y, const QImage &img) {
    int tmsY = ((1 << z) - 1) - y;
    TileKey key(z, x, tmsY);
    if (!m_tileCache.contains(key)) {
        m_tileCache.insert(key, img);
        update();
    }
}

QRectF MBTilesViewer::getVisibleTileRect() {
    double leftTile = m_offset.x() / m_tileSize;
    double topTile = m_offset.y() / m_tileSize;
    double rightTile = (m_offset.x() + width()) / m_tileSize;
    double bottomTile = (m_offset.y() + height()) / m_tileSize;

    return QRectF(leftTile, topTile, rightTile - leftTile, bottomTile - topTile);
}

QPointF MBTilesViewer::tileToPixel(int x, int y) {
    return QPointF(x * m_tileSize, y * m_tileSize);
}

void MBTilesViewer::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    if (!m_dbReady) {
        painter.setPen(Qt::black);
        painter.drawText(rect(), Qt::AlignCenter, "No MBTiles file loaded");
        return;
    }

    QRectF visible = getVisibleTileRect();
    int startX = static_cast<int>(visible.left());
    int startY = static_cast<int>(visible.top());
    int endX = static_cast<int>(visible.right());
    int endY = static_cast<int>(visible.bottom());

    int drawnCount = 0;

    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            int tmsY = ((1 << m_currentZoom) - 1) - y;
            TileKey key(m_currentZoom, x, tmsY);

            if (m_tileCache.contains(key)) {
                QImage img = m_tileCache.value(key);
                QPointF pos = tileToPixel(x, y);
                QPointF drawPos = pos - m_offset;

                // Оптимизация отрисовки
                if (drawPos.x() + m_tileSize < 0 || drawPos.x() > width() ||
                    drawPos.y() + m_tileSize < 0 || drawPos.y() > height()) {
                    continue;
                }

                painter.drawImage(drawPos, img);
                drawnCount++;
            }
        }
    }

    if (drawnCount == 0) {
         painter.setPen(Qt::red);
         QString msg = "No tiles found for this area/zoom.\n";
         if (m_availableTiles.isEmpty()) {
             msg += "Database is empty or not scanned.";
         } else {
             msg += "Try scrolling or zooming.\nAvailable zooms: " + QString::number(m_minZoom) + "-" + QString::number(m_maxZoom);
         }
         painter.drawText(rect(), Qt::AlignCenter, msg);
    }
}

void MBTilesViewer::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_lastMousePos = event->pos();
    }
}

void MBTilesViewer::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_offset -= delta;
        m_lastMousePos = event->pos();
        updateViewport();
    }
}

void MBTilesViewer::wheelEvent(QWheelEvent *event) {
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 15;

    int oldZoom = m_currentZoom;
    if (numSteps > 0 && m_currentZoom < m_maxZoom)
        m_currentZoom++;
    else if (numSteps < 0 && m_currentZoom > m_minZoom)
        m_currentZoom--;

    if (oldZoom != m_currentZoom) {
        // Проверка: есть ли данные на новом зуме?
        if (!m_zoomBounds.contains(m_currentZoom)) {
            // Если данных нет, отменяем зум или пытаемся найти ближайший
            // Для простоты просто не меняем зум, если там пусто, или позволяем пользователю крутить дальше
            // Но лучше предупредить или не давать зумить в пустоту
            // В данном случае просто разрешаем, но тайлов не будет
        }

        QPoint cursorPos = event->pos();
        double factor = std::pow(2.0, m_currentZoom - oldZoom);
        m_offset = (m_offset + cursorPos) * factor - cursorPos;

        m_tileCache.clear();
        qDebug() << "Current zoom:" << m_currentZoom;
        updateViewport();
    }
}
