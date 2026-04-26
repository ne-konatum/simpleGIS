#include "demreader.h"
#include <QIODevice>
#include <QTextStream>
#include <QRegularExpression>
#include <QVector>
#include <cmath>

DEMReader::DEMReader()
    : m_isLoaded(false)
    , m_minElevation(0.0)
    , m_maxElevation(0.0)
{
    m_header = DEMHeader();
}

DEMReader::~DEMReader()
{
    closeFile();
}

bool DEMReader::openFile(const QString &filePath)
{
    closeFile();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "DEMReader: Cannot open file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty()) {
        qDebug() << "DEMReader: File is empty:" << filePath;
        return false;
    }

    qDebug() << "DEMReader: File size:" << data.size() << "bytes";
    
    m_filePath = filePath;

    // Пытаемся распарсить заголовок
    qDebug() << "DEMReader: Parsing header...";
    if (!parseHeader(data)) {
        qDebug() << "DEMReader: Failed to parse header";
        return false;
    }
    qDebug() << "DEMReader: Header parsed successfully";

    // Парсим данные высот
    qDebug() << "DEMReader: Parsing elevation data...";
    if (!parseElevationData(data)) {
        qDebug() << "DEMReader: Failed to parse elevation data";
        return false;
    }
    qDebug() << "DEMReader: Elevation data parsed successfully";

    m_isLoaded = true;
    qDebug() << "DEMReader: Successfully loaded DEM file:" << filePath;
    qDebug() << "  Bounds: [" << m_header.xmin << "," << m_header.xmax << "] x [" 
             << m_header.ymin << "," << m_header.ymax << "]";
    qDebug() << "  Grid size:" << m_header.rows << "x" << m_header.cols;
    qDebug() << "  Elevation range:" << m_minElevation << "-" << m_maxElevation << "m";

    return true;
}

void DEMReader::closeFile()
{
    m_isLoaded = false;
    m_filePath.clear();
    m_elevationData.clear();
    m_header = DEMHeader();
    m_minElevation = 0.0;
    m_maxElevation = 0.0;
}

bool DEMReader::getElevation(double latitude, double longitude, double &height) const
{
    if (!m_isLoaded) {
        height = 0.0;
        return false;
    }

    // Проверка границ
    if (longitude < m_header.xmin || longitude > m_header.xmax ||
        latitude < m_header.ymin || latitude > m_header.ymax) {
        height = 0.0;
        return false;
    }

    // Интерполяция высоты
    height = interpolateElevation(latitude, longitude);
    return true;
}

bool DEMReader::parseHeader(const QByteArray &data)
{
    qDebug() << "DEMReader: parseHeader called, data size:" << data.size();
    
    QTextStream stream(data);
    stream.setCodec("UTF-8");

    // USGS DEM формат имеет специфическую структуру заголовка
    // Пытаемся найти ключевые поля в первых строках файла
    
    QString content = QString::fromUtf8(data.left(1024));
    qDebug() << "DEMReader: First 1024 bytes as string:" << content.left(200);
    
    // Пробуем различные форматы DEM файлов
    
    // Формат 1: ASCII DEM с метаданными в начале
    QRegularExpression xminRx("XMIN:\\s*([\\-\\d\\.]+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression xmaxRx("XMAX:\\s*([\\-\\d\\.]+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression yminRx("YMIN:\\s*([\\-\\d\\.]+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression ymaxRx("YMAX:\\s*([\\-\\d\\.]+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression rowsRx("(?:NROWS|ROWS):\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression colsRx("(?:NCOLS|COLS):\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression cellSizeRx("(?:CELLSIZE|PIXELSIZE|RESOLUTION):\\s*([\\-\\d\\.]+)", QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match;

    match = xminRx.match(content);
    if (match.hasMatch()) {
        m_header.xmin = match.captured(1).toDouble();
        qDebug() << "DEMReader: Found XMIN:" << m_header.xmin;
    }
    
    match = xmaxRx.match(content);
    if (match.hasMatch()) {
        m_header.xmax = match.captured(1).toDouble();
        qDebug() << "DEMReader: Found XMAX:" << m_header.xmax;
    }
    
    match = yminRx.match(content);
    if (match.hasMatch()) {
        m_header.ymin = match.captured(1).toDouble();
        qDebug() << "DEMReader: Found YMIN:" << m_header.ymin;
    }
    
    match = ymaxRx.match(content);
    if (match.hasMatch()) {
        m_header.ymax = match.captured(1).toDouble();
        qDebug() << "DEMReader: Found YMAX:" << m_header.ymax;
    }
    
    match = rowsRx.match(content);
    if (match.hasMatch()) {
        m_header.rows = match.captured(1).toInt();
        qDebug() << "DEMReader: Found ROWS:" << m_header.rows;
    }
    
    match = colsRx.match(content);
    if (match.hasMatch()) {
        m_header.cols = match.captured(1).toInt();
        qDebug() << "DEMReader: Found COLS:" << m_header.cols;
    }
    
    match = cellSizeRx.match(content);
    if (match.hasMatch()) {
        m_header.cellSize = match.captured(1).toDouble();
        qDebug() << "DEMReader: Found CELLSIZE:" << m_header.cellSize;
    }

    // Если не нашли все параметры, пробуем альтернативный парсинг
    // Некоторые DEM файлы имеют бинарный заголовок USGS
    if (m_header.rows == 0 || m_header.cols == 0) {
        qDebug() << "DEMReader: Standard header not found, trying alternative parsing...";
        // Пробуем распарсить как простой ASCII grid
        QStringList lines = content.split('\n', Qt::SkipEmptyParts);
        
        // Ищем строки с числовыми данными (первая строка данных)
        for (int i = 0; i < lines.size() && i < 20; ++i) {
            QString line = lines[i].trimmed();
            if (line.isEmpty() || line.startsWith("#")) continue;
            
            // Проверяем, содержит ли строка только числа (данные высот)
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            bool allNumbers = true;
            for (const QString &part : parts) {
                bool ok;
                part.toDouble(&ok);
                if (!ok && part != "-9999" && part != "NaN") {
                    allNumbers = false;
                    break;
                }
            }
            
            if (allNumbers && !parts.isEmpty()) {
                // Нашли начало данных, предполагаем что это первая строка сетки
                // Значит заголовок уже закончился
                break;
            }
        }
        
        // Если так и не нашли, пробуем вычислить из данных
        if (m_header.rows == 0 || m_header.cols == 0) {
            qDebug() << "DEMReader: Trying to infer grid size from data...";
            // Для простоты предполагаем стандартный размер ячейки
            m_header.cellSize = 0.000277778; // ~30 arcseconds (SRTM)
            
            // Подсчитываем количество строк данных
            int dataLines = 0;
            int maxCols = 0;
            
            for (int i = 0; i < lines.size(); ++i) {
                QString line = lines[i].trimmed();
                if (line.isEmpty() || line.startsWith("#")) continue;
                
                QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    bool ok;
                    parts[0].toDouble(&ok);
                    if (ok || parts[0] == "-9999") {
                        dataLines++;
                        maxCols = qMax(maxCols, parts.size());
                    }
                }
            }
            
            if (dataLines > 0 && maxCols > 0) {
                m_header.rows = dataLines;
                m_header.cols = maxCols;
                qDebug() << "DEMReader: Inferred grid size:" << m_header.rows << "x" << m_header.cols;
            }
        }
    }

    // Вычисляем размер ячейки если не задан
    if (m_header.cellSize <= 0 && m_header.rows > 1 && m_header.cols > 1) {
        double dx = (m_header.xmax - m_header.xmin) / (m_header.cols - 1);
        double dy = (m_header.ymax - m_header.ymin) / (m_header.rows - 1);
        m_header.cellSize = qAbs(dx) > qAbs(dy) ? qAbs(dx) : qAbs(dy);
        qDebug() << "DEMReader: Calculated cell size:" << m_header.cellSize;
    }

    // Проверка на успешность парсинга
    if (m_header.rows <= 0 || m_header.cols <= 0) {
        qDebug() << "DEMReader: Failed to determine grid size, rows=" << m_header.rows << ", cols=" << m_header.cols;
        return false;
    }

    qDebug() << "DEMReader: Header parsed successfully, rows=" << m_header.rows << ", cols=" << m_header.cols;
    return true;
}

bool DEMReader::parseElevationData(const QByteArray &data)
{
    qDebug() << "DEMReader: parseElevationData called, data size:" << data.size();
    
    QTextStream stream(data);
    stream.setCodec("UTF-8");

    QString content = stream.readAll();
    QStringList lines = content.split('\n', Qt::SkipEmptyParts);

    m_elevationData.resize(m_header.rows * m_header.cols);
    m_minElevation = std::numeric_limits<double>::max();
    m_maxElevation = std::numeric_limits<double>::lowest();

    int row = 0;
    int col = 0;
    bool inDataSection = false;
    int valuesRead = 0;

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        
        // Пропускаем пустые строки и комментарии
        if (trimmedLine.isEmpty() || trimmedLine.startsWith("#")) {
            continue;
        }

        // Проверяем, не является ли строка частью заголовка
        if (!inDataSection) {
            if (trimmedLine.contains(QRegularExpression("[a-zA-Z]"))) {
                // Строка содержит буквы - вероятно это ещё заголовок
                continue;
            }
            inDataSection = true;
        }

        // Разбиваем строку на числа
        QStringList parts = trimmedLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        
        for (const QString &part : parts) {
            bool ok;
            double value = part.toDouble(&ok);
            
            if (ok) {
                // Проверяем на NoData значение
                if (value == -9999 || value == -32768 || std::isnan(value)) {
                    value = -9999.0; // Маркер отсутствия данных
                }

                if (row < m_header.rows && col < m_header.cols) {
                    int index = row * m_header.cols + col;
                    m_elevationData[index] = value;

                    if (value != -9999.0) {
                        m_minElevation = qMin(m_minElevation, value);
                        m_maxElevation = qMax(m_maxElevation, value);
                    }

                    col++;
                    valuesRead++;
                    if (col >= m_header.cols) {
                        col = 0;
                        row++;
                    }
                }
            }
        }
        
        // Если перешли на новую строку и были в середине строки данных
        if (col != 0 && inDataSection) {
            col = 0;
            row++;
        }
    }

    qDebug() << "DEMReader: Values read:" << valuesRead << ", expected:" << (m_header.rows * m_header.cols);

    // Если не удалось прочитать данные
    if (m_elevationData.isEmpty() || (m_minElevation == std::numeric_limits<double>::max())) {
        qDebug() << "DEMReader: No valid elevation data found";
        return false;
    }

    qDebug() << "DEMReader: Elevation data parsed successfully, min=" << m_minElevation << ", max=" << m_maxElevation;
    return true;
}

double DEMReader::interpolateElevation(double lat, double lon) const
{
    if (m_elevationData.isEmpty() || m_header.rows < 2 || m_header.cols < 2) {
        return 0.0;
    }

    // Нормализация координат в индексы сетки
    double xNorm = (lon - m_header.xmin) / (m_header.xmax - m_header.xmin);
    double yNorm = (lat - m_header.ymin) / (m_header.ymax - m_header.ymin);

    // Вычисляем индексы ячеек
    double colF = xNorm * (m_header.cols - 1);
    double rowF = yNorm * (m_header.rows - 1);

    int col0 = static_cast<int>(std::floor(colF));
    int row0 = static_cast<int>(std::floor(rowF));
    int col1 = qMin(col0 + 1, m_header.cols - 1);
    int row1 = qMin(row0 + 1, m_header.rows - 1);

    // Ограничиваем значения
    col0 = qBound(0, col0, m_header.cols - 1);
    row0 = qBound(0, row0, m_header.rows - 1);

    // Дробные части для интерполяции
    double fx = colF - col0;
    double fy = rowF - row0;

    // Получаем значения в четырех углах ячейки
    auto getValue = [this](int row, int col) -> double {
        int index = row * m_header.cols + col;
        if (index >= 0 && index < m_elevationData.size()) {
            double val = m_elevationData[index];
            if (val == -9999.0) return 0.0; // Заменяем NoData на 0
            return val;
        }
        return 0.0;
    };

    double v00 = getValue(row0, col0);
    double v10 = getValue(row0, col1);
    double v01 = getValue(row1, col0);
    double v11 = getValue(row1, col1);

    // Билинейная интерполяция
    double v0 = v00 * (1.0 - fx) + v10 * fx;
    double v1 = v01 * (1.0 - fx) + v11 * fx;
    double result = v0 * (1.0 - fy) + v1 * fy;

    return result;
}
