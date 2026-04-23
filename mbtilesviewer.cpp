#include "mbtilesviewer.h"
#include "tileloader.h"
#include <QPainter>
#include <QWheelEvent>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QtMath>

MBTilesViewer::MBTilesViewer(QWidget *parent)
    : QWidget(parent)
    , m_tileLoader(new TileLoader(this))
    , m_offset(0, 0)
    , m_tileSize(256)
    , m_viewportSize(800, 600)
    , m_currentZoom(0)  // Инициализация текущего зума
{
    m_databaseReady = false;
    m_minZoom = 0;
    m_maxZoom = 17;
    setMouseTracking(true);
    setupUI();
    connectTileLoader();
    connect(m_tileLoader, &TileLoader::tileLoaded, this, &MBTilesViewer::onTileLoaded);
}

MBTilesViewer::~MBTilesViewer()
{
    if (m_database.isOpen()) {
        m_database.close();
    }
}

void MBTilesViewer::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_databaseReady) {
        updateViewport();
    }
}

void MBTilesViewer::setupUI()
{
    m_layout = new QVBoxLayout(this);

    m_openButton = new QPushButton("Open MBTiles File", this);
    m_openButton->setToolTip("Open an MBTiles database file");
    connect(m_openButton, &QPushButton::clicked,
            this, &MBTilesViewer::openFileDialog);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);

    setupMapWidget();
    m_scrollArea->setWidget(m_mapWidget);

    m_layout->addWidget(m_openButton);
    m_layout->addWidget(m_scrollArea);

    setLayout(m_layout);
}

void MBTilesViewer::setupMapWidget()
{
    m_mapWidget = new MapWidget(this);
    m_mapWidget->setBackgroundRole(QPalette::Base);
    m_mapWidget->setAutoFillBackground(true);
    m_mapWidget->setMinimumSize(800, 600);
}

void MBTilesViewer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QWidget::paintEvent(event);
}

void MBTilesViewer::paintMapWidget(QPaintEvent *event)
{
    if (!m_databaseReady) {
        QPainter painter(m_mapWidget);
        painter.setPen(Qt::red);
        painter.drawText(event->rect(), Qt::AlignCenter,
                "Database not ready - waiting for data...");
        return;
    }

    qDebug() << "paintMapWidget called. Database ready:" << m_databaseReady
             << "Loaded tiles count:" << m_loadedTiles.size();

    QPainter painter(m_mapWidget);
    painter.fillRect(event->rect(), Qt::lightGray);
    painter.translate(m_offset.x(), m_offset.y());
    drawTiles(&painter);
}

void MBTilesViewer::drawTiles(QPainter *painter)
{
    qDebug() << "drawTiles: drawing" << m_loadedTiles.size() << "tiles";

    if (m_loadedTiles.isEmpty()) {
        painter->setPen(Qt::blue);
        painter->drawText(m_mapWidget->rect(), Qt::AlignCenter,
                "No tiles loaded yet. Waiting for data...");
        return;
    }

    QRect widgetRect(0, 0, m_mapWidget->width(), m_mapWidget->height());
    int drawnCount = 0;

    for (auto it = m_loadedTiles.constBegin(); it != m_loadedTiles.constEnd(); ++it) {
        int z = it.key().first;
        int x = it.key().second.first;
        int y = it.key().second.second;
        const QPixmap &tile = it.value();

        if (!tile.isNull()) {
            QPointF pixelPos = tileToPixel(x, y, z);
            pixelPos += m_offset;

            QRectF tileRect(pixelPos, QSizeF(m_tileSize, m_tileSize));

            if (tileRect.intersects(widgetRect)) {
                painter->drawPixmap(pixelPos.toPoint(), tile);
                qDebug() << "drawTiles: DRAWN tile" << z << x << y
                          << "at" << pixelPos << "size:" << m_tileSize;
                drawnCount++;
            } else {
                qDebug() << "drawTiles: SKIPPED tile" << z << x << y
                     << "outside visible area";
            }
        } else {
            qWarning() << "Null tile detected:" << z << x << y;
        }
    }

    // Дополнительная оптимизация: показываем статистику отрисовки
    qDebug() << "drawTiles: FINISHED. Drawn" << drawnCount
              << "tiles out of" << m_loadedTiles.size();

    // Визуальная индикация, если нет видимых тайлов
    if (drawnCount == 0) {
        painter->setPen(Qt::darkRed);
        painter->setFont(QFont("Arial", 12));
        painter->drawText(widgetRect, Qt::AlignCenter,
            QString("No visible tiles at zoom level %1\n"
                "Try zooming out or panning to loaded areas")
            .arg(m_currentZoom));
        qDebug() << "drawTiles: WARNING - no visible tiles at current zoom/offset";
    }
}

QPointF MBTilesViewer::tileToPixel(int x, int y, int zoom)
{
    // Каждый тайл всегда имеет размер m_tileSize (256) пикселей
    // Позиция тайла = координаты тайла * размер тайла
    double pixelX = x * m_tileSize;
    // Для TMS: Y инвертирован, поэтому считаем от верха карты
    // Общая высота карты на этом зуме = (1 << zoom) * m_tileSize
    // Позиция Y = общая высота - (y + 1) * размер тайла
    double pixelY = ((1 << zoom) * m_tileSize) - (y + 1) * m_tileSize;

    return QPointF(pixelX, pixelY);
}

int MBTilesViewer::tmsToGoogleY(int y, int zoom) const
{
    return (1 << zoom) - 1 - y;
}

int MBTilesViewer::googleToTmsY(int y, int zoom) const
{
    return (1 << zoom) - 1 - y;
}

QRectF MBTilesViewer::getVisibleTileRect(int currentZoom, const QSize& viewportSize,
                   const QPointF& offset, int tileSize)
{
    if (viewportSize.isEmpty() || tileSize <= 0) {
        qDebug() << "getVisibleTileRect: invalid parameters";
        return QRectF();
    }

    // Размер тайла в пикселях на текущем зуме
    double pixelsPerTile = static_cast<double>(tileSize);

    qDebug() << "getVisibleTileRect: zoom" << currentZoom
              << "tileSize" << tileSize
              << "pixelsPerTile" << pixelsPerTile;

    // Смещение в пикселях
    QPointF topLeftPixel = -offset;  // Инвертируем смещение для правильного расчета
    QPointF bottomRightPixel(-offset.x() + viewportSize.width(),
                 -offset.y() + viewportSize.height());

    // Корректное округление для тайловых координат
    int leftTile = qFloor(topLeftPixel.x() / pixelsPerTile);
    int rightTile = qCeil(bottomRightPixel.x() / pixelsPerTile);
    int topTile = qFloor(topLeftPixel.y() / pixelsPerTile);
    int bottomTile = qCeil(bottomRightPixel.y() / pixelsPerTile);

    // Ограничиваем координаты допустимым диапазоном
    int maxTileIndex = (1 << currentZoom) - 1;
    leftTile = qBound(0, leftTile, maxTileIndex);
    rightTile = qBound(0, rightTile, maxTileIndex);
    topTile = qBound(0, topTile, maxTileIndex);
    bottomTile = qBound(0, bottomTile, maxTileIndex);

    QRectF rect(leftTile, topTile, rightTile - leftTile + 1, bottomTile - topTile + 1);

    qDebug() << "getVisibleTileRect:"
              << "viewport" << viewportSize
              << "offset" << offset
              << "->" << rect;

    return rect;
}

void MBTilesViewer::updateViewport()
{
    if (!m_databaseReady) return;

    qDebug() << "updateViewport called. Database ready:" << m_databaseReady;

    static int lastZoom = -1;
    bool zoomChanged = (m_currentZoom != lastZoom);

    if (zoomChanged) {
        m_loadedTiles.clear();
        qDebug() << "Tile cache cleared on zoom change";
        lastZoom = m_currentZoom;
    } else {
        qDebug() << "Zoom unchanged, preserving tile cache with" << m_loadedTiles.size() << "tiles";
    }

    QRectF visibleTiles = getVisibleTileRect
        (m_currentZoom,
        m_viewportSize,
        m_offset,
        m_tileSize
    );

    loadVisibleTiles(visibleTiles, m_currentZoom);
    update();
}

bool MBTilesViewer::isTileAvailable(int zoom, int x, int y)
{
    if (!ensureDatabaseConnection()) {
        return false;
    }

    QSqlQuery query(m_database);
    int tmsY = tmsToGoogleY(y, zoom);
    query.prepare("SELECT 1 FROM tiles WHERE zoom_level = ? AND tile_column = ? AND tile_row = ?");
    query.addBindValue(zoom);
    query.addBindValue(x);
    query.addBindValue(tmsY);

    if (!query.exec()) {
        qWarning() << "isTileAvailable: SQL query failed:" << query.lastError().text();
        return false;
    }

    return query.next();
}

void MBTilesViewer::loadVisibleTiles(const QRectF& visibleRect, int zoom)
{
    int minX = static_cast<int>(visibleRect.left());
    int maxX = static_cast<int>(visibleRect.right());
    int minY = static_cast<int>(visibleRect.top());
    int maxY = static_cast<int>(visibleRect.bottom());

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            auto tileKey = qMakePair(zoom, qMakePair(x, y));

            if (m_loadedTiles.contains(tileKey)) {
                qDebug() << "Skipping already cached tile:" << zoom << x << y;
                continue;
            }

            if (isTileAvailable(zoom, x, y)) {
                m_tileLoader->loadTileAsync(m_database, zoom, x, y);
                qDebug() << "Requested tile load:" << zoom << x << y;
            } else {
                // Если тайл недоступен, пытаемся загрузить родительский
                qDebug() << "Tile not available:" << zoom << x << y
                          << "attempting to load parent tile";
                loadParentTile(zoom, x, y, 0); // Рекурсия начинается с глубины 0
            }
        }
    }
}

void MBTilesViewer::loadParentTile(int zoom, int x, int y, int recursionDepth)
{
    if (recursionDepth > 5 || zoom <= m_minZoom) {
        qDebug() << "loadParentTile: recursion limit reached or minimum zoom"
                  << zoom << "depth:" << recursionDepth;
        return;
    }

    int parentZoom = zoom - 1;
    int parentX = x / 2;
    int parentY = y / 2;

    auto parentKey = qMakePair(parentZoom, qMakePair(parentX, parentY));

    if (m_loadedTiles.contains(parentKey)) {
        qDebug() << "loadParentTile: parent tile already in cache"
                  << parentZoom << parentX << parentY;
        return;
    }

    if (isTileAvailable(parentZoom, parentX, parentY)) {
        m_tileLoader->loadTileAsync(m_database, parentZoom, parentX, parentY);
        qDebug() << "loadParentTile: loading parent tile" << parentZoom
                  << parentX << parentY << "recursion depth:" << recursionDepth + 1;
    } else {
        // Рекурсивный вызов для загрузки «прародителя»
        qDebug() << "loadParentTile: parent tile not available" << parentZoom
                  << parentX << parentY << "attempting grandparent";
        loadParentTile(parentZoom, parentX, parentY, recursionDepth + 1);
    }
}

bool MBTilesViewer::checkDatabaseStructure()
{
    QSqlQuery query(m_database);
    return query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='tiles'")
           && query.next();
}

bool MBTilesViewer::reopenDatabase()
{
    qWarning() << "Attempting to reopen database...";
    if (m_database.isOpen()) m_database.close();

    if (m_database.open()) {
        qDebug() << "Database reopened successfully";
        return true;
    } else {
        qWarning() << "Failed to reopen database:" << m_database.lastError().text();
        return false;
    }
}

int MBTilesViewer::getMaxZoom() const
{
    if (!m_databaseReady) return 0;

    QSqlQuery query(m_database);
    query.exec("SELECT MAX(zoom_level) FROM tiles");
    if (query.next()) {
        int maxZoom = query.value(0).toInt();
        qDebug() << "getMaxZoom: returning" << maxZoom;
        return maxZoom;
    }
    return 0;
}

int MBTilesViewer::getMinZoom() const
{
    if (!m_databaseReady) return 0;

    QSqlQuery query(m_database);
    query.exec("SELECT MIN(zoom_level) FROM tiles");
    if (query.next()) {
        int minZoom = query.value(0).toInt();
        qDebug() << "getMinZoom: returning" << minZoom;
        return minZoom;
    }
    return 0;
}

QPair<int, int> MBTilesViewer::pixelToTile(const QPointF &pos, int zoom) const
{
    int tileSize = m_tileSize >> (m_maxZoom - zoom);
    if (tileSize <= 0) tileSize = m_tileSize;

    int x = static_cast<int>(pos.x() / tileSize);
    int y = static_cast<int>(pos.y() / tileSize);
    return qMakePair(x, y);
}

void MBTilesViewer::openFileDialog()
{
    QString filename = QFileDialog::getOpenFileName
    (
        this,
        "Open MBTiles File",
        QString(),
        "MBTiles Files (*.mbtiles);;All Files (*)"
    );

    if (!filename.isEmpty()) {
        if (openMBTiles(filename)) {
            qDebug() << "MBTiles file opened successfully:" << filename;
            m_databaseReady = true;
            // Обновляем кэшированные значения зума
            m_minZoom = getMinZoom();
            m_maxZoom = getMaxZoom();
            resetView();
            updateViewport();
        } else {
            QMessageBox::critical(this, "Error", "Failed to open MBTiles file: " + filename);
            qWarning() << "Failed to open MBTiles file:" << filename;
        }
    }
}

bool MBTilesViewer::openMBTiles(const QString &filename)
{
    qDebug() << "Attempting to open MBTiles file:" << filename;

    // Закрываем предыдущую базу данных, если открыта
    if (m_database.isOpen()) {
        m_database.close();
    }

    m_database = QSqlDatabase::addDatabase("QSQLITE");
    m_database.setDatabaseName(filename);

    if (!m_database.open()) {
        qWarning() << "Failed to open database:" << m_database.lastError().text();
        return false;
    }

    // Проверяем, что это валидный MBTiles файл — проверяем наличие таблицы tiles
    if (!checkDatabaseStructure()) {
        qWarning() << "Invalid MBTiles file — missing 'tiles' table or structure error";
        m_database.close();
        return false;
    }

    qDebug() << "MBTiles database opened successfully:" << filename;
    debugDatabaseContents();
    return true;
}

void MBTilesViewer::debugDatabaseContents()
{
    QSqlQuery query(m_database);
    query.exec("SELECT DISTINCT zoom_level, COUNT(*) FROM tiles GROUP BY zoom_level");
    while (query.next()) {
        qDebug() << "Zoom level:" << query.value(0).toInt()
                  << "Tile count:" << query.value(1).toInt();
    }
}

QPair<int, int> MBTilesViewer::findFirstAvailableTile(int zoom)
{
    QSqlQuery query(m_database);
    query.prepare("SELECT tile_column, tile_row FROM tiles WHERE zoom_level = :zoom LIMIT 1");
    query.bindValue(":zoom", zoom);

    if (query.exec() && query.next()) {
        return qMakePair(query.value(0).toInt(), query.value(1).toInt());
    }
    qWarning() << "No tiles found for zoom level" << zoom;
    return qMakePair(-1, -1);
}

void MBTilesViewer::resetView()
{
    if (!m_databaseReady) return;

    // Устанавливаем zoom level 12 по умолчанию, или ближайший доступный
    m_currentZoom = qBound(m_minZoom, 12, m_maxZoom);
    
    qDebug() << "resetView: setting zoom to" << m_currentZoom << "(min:" << m_minZoom << ", max:" << m_maxZoom << ")";
    
    // Получаем центр карты из метаданных или используем (0, 0)
    double centerX = 0.0;
    double centerY = 0.0;
    
    QSqlQuery query(m_database);
    query.exec("SELECT value FROM metadata WHERE name='center'");
    if (query.next()) {
        QString centerStr = query.value(0).toString();
        QStringList parts = centerStr.split(',');
        if (parts.size() >= 2) {
            centerX = parts[0].trimmed().toDouble();
            centerY = parts[1].trimmed().toDouble();
            qDebug() << "resetView: center from metadata:" << centerX << centerY;
        }
    }
    
    // Конвертируем географические координаты в тайловые координаты XYZ
    // Формула для конвертации lon/lat в tile x/y
    int n = 1 << m_currentZoom;
    int tileX = static_cast<int>((centerX + 180.0) / 360.0 * n);
    
    // Для latitude нужно использовать формулу Меркатора
    double latRad = centerY * M_PI / 180.0;
    double mercN = std::log(std::tan(latRad) + 1.0 / std::cos(latRad));
    int tileY = static_cast<int>((1.0 - mercN / M_PI) / 2.0 * n);
    
    // Конвертируем из XYZ в TMS (так как база использует TMS)
    int tmsY = n - 1 - tileY;
    
    qDebug() << "resetView: tile coordinates at zoom" << m_currentZoom << ":" << tileX << tmsY << "(TMS)";
    
    // Проверяем, доступен ли этот тайл, если нет - ищем ближайший доступный
    if (!isTileAvailable(m_currentZoom, tileX, tmsY)) {
        qDebug() << "resetView: tile" << tileX << tmsY << "not available, searching for available tiles...";
        QPair<int, int> foundTile = findFirstAvailableTile(m_currentZoom);
        if (foundTile.first >= 0) {
            tileX = foundTile.first;
            tmsY = foundTile.second;
            qDebug() << "resetView: using first available tile:" << tileX << tmsY;
        }
    }
    
    // Центрируем вид на выбранном тайле
    // Позиция тайла в пикселях (для TMS)
    double pixelX = tileX * m_tileSize;
    double pixelY = ((1 << m_currentZoom) * m_tileSize) - (tmsY + 1) * m_tileSize;
    
    // Центрируем: смещение должно быть таким, чтобы центр виджета совпадал с центром тайла
    m_offset = QPointF(-pixelX + (m_viewportSize.width() - m_tileSize) / 2.0,
                       -pixelY + (m_viewportSize.height() - m_tileSize) / 2.0);

    // Очищаем кэш загруженных тайлов при сбросе вида
    m_loadedTiles.clear();
    qDebug() << "View reset: zoom set to" << m_currentZoom
              << "offset reset to" << m_offset << ", tile cache cleared";

    updateViewport();
}

void MBTilesViewer::debugCoordinateSystem()
{
    qDebug() << "=== Coordinate System Debug ===";
    qDebug() << "Current zoom:" << m_currentZoom;
    qDebug() << "Offset:" << m_offset;
    qDebug() << "Max zoom:" << m_maxZoom;
    qDebug() << "Min zoom:" << m_minZoom;

    // Тестируем преобразование координат для нескольких тайлов
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            QPointF pixel = tileToPixel(x, y, m_currentZoom);
            qDebug() << "Tile (" << x << "," << y << ") at zoom" << m_currentZoom
                      << "-> pixel position:" << pixel;
        }
    }
    qDebug() << "==============================";
}

void MBTilesViewer::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Зум при Ctrl + колёсико мыши
        int delta = event->angleDelta().y();
        if (delta > 0) {
            m_currentZoom++;
        } else {
            m_currentZoom--;
        }

        // Ограничиваем зум допустимыми значениями
        m_currentZoom = qBound(m_minZoom, m_currentZoom, m_maxZoom);

        qDebug() << "Current zoom:" << m_currentZoom;

        updateViewport();
        event->accept();
    } else {
        // Прокрутка карты без изменения масштаба
        m_offset += QPointF(event->angleDelta().x(), event->angleDelta().y());
        m_mapWidget->update();
        event->accept();
    }
}

void MBTilesViewer::resizeEvent(QResizeEvent *event)
{
    m_viewportSize = event->size();
    updateViewport();  // Пересчитываем видимые тайлы
    QWidget::resizeEvent(event);
}

void MBTilesViewer::onTileLoaded(int zoom, int x, int y, const QImage& tileImage)
{
    if (tileImage.isNull()) {
        qWarning() << "onTileLoaded: received null image for" << zoom << x << y;
        return;
    }

    QPixmap tilePixmap = QPixmap::fromImage(tileImage);  // Конвертируем при сохранении
    auto key = qMakePair(zoom, qMakePair(x, y));
    m_loadedTiles[key] = tilePixmap;

    update();
}

void MBTilesViewer::connectTileLoader()
{
    connect(m_tileLoader, &TileLoader::tileLoaded,
            this, &MBTilesViewer::onTileLoaded);
    qDebug() << "MBTilesViewer: Connected tileLoaded signal to onTileLoaded slot";
}

void MBTilesViewer::loadChildTiles(int zoom, int x, int y)
{
    if (zoom >= m_maxZoom) {
        qDebug() << "loadChildTiles: reached maximum zoom, cannot load children" << zoom;
        return;
    }

    int childZoom = zoom + 1;
    int childX = x * 2;
    int childY = y * 2;

    // Загружаем 4 дочерних тайла (2x2 сетка)
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            int cx = childX + i;
            int cy = childY + j;

            auto childKey = qMakePair(childZoom, qMakePair(cx, cy));
            if (!m_loadedTiles.contains(childKey) && isTileAvailable(childZoom, cx, cy)) {
                m_tileLoader->loadTileAsync(m_database, childZoom, cx, cy);
                qDebug() << "loadChildTiles: loading child tile" << childZoom << cx << cy;
            }
        }
    }
}

bool MBTilesViewer::ensureDatabaseConnection()
{
    if (m_database.isOpen()) {
        return true;
    }

    qWarning() << "Database not open, attempting to reopen...";
    if (!m_database.open()) {
        qCritical() << "Failed to reopen database!";
        m_databaseReady = false;
        return false;
    }
    m_databaseReady = true;
    return true;
}
