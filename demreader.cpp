#include "demreader.h"
#include <QFileInfo>
#include <QIODevice>
#include <cmath>
#include <limits>

DEMReader::DEMReader()
    : m_isLoaded(false)
    , m_xMin(0)
    , m_yMin(0)
    , m_cellSize(0)
    , m_rows(0)
    , m_cols(0)
    , m_noDataValue(-9999.0)
    , m_minElevation(0)
    , m_maxElevation(0)
{
}

DEMReader::~DEMReader()
{
}

bool DEMReader::openFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = "Cannot open file: " + file.errorString();
        qWarning() << "DEMReader:" << m_lastError;
        return false;
    }

    qDebug() << "DEMReader: File size:" << file.size() << "bytes";

    // Сброс состояния
    m_isLoaded = false;
    m_elevations.clear();
    m_minElevation = std::numeric_limits<double>::max();
    m_maxElevation = std::numeric_limits<double>::lowest();

    // Читаем первые байты для определения формата
    QByteArray header = file.read(1024);

    // Проверка на сигнатуру USGS DEM ("DEMP" или другие маркеры)
    if (parseUSGSDem(file)) {
        m_isLoaded = true;
        qDebug() << "DEMReader: Successfully loaded USGS DEM format.";
        qDebug() << "DEMReader: Bounds:" << m_xMin << m_yMin << m_xMin + m_cols * m_cellSize << m_yMin + m_rows * m_cellSize;
        qDebug() << "DEMReader: Grid:" << m_cols << "x" << m_rows << "CellSize:" << m_cellSize;
        qDebug() << "DEMReader: Elevation range:" << m_minElevation << "-" << m_maxElevation << "m";
        return true;
    }

    // Если не USGS, пробуем вернуть поток в начало и проверить ASCII
    file.seek(0);
    if (parseAsciiGrid(file)) {
        m_isLoaded = true;
        qDebug() << "DEMReader: Successfully loaded ASCII Grid format.";
        return true;
    }

    m_lastError = "Failed to parse header: Unknown DEM format";
    qWarning() << "DEMReader:" << m_lastError;
    return false;
}

// Парсер для бинарного формата USGS DEM
bool DEMReader::parseUSGSDem(QFile &file)
{
    file.seek(0);

    // Считаем первые 200 строк как текст для поиска метаданных
    QTextStream in(&file);
    in.setCodec("UTF-8");

    double xmin = 0, xmax = 0, ymin = 0, ymax = 0;
    int rows = 0, cols = 0;
    double cellSize = 0;

    // Многие DEM файлы имеют текстовый заголовок с ключевыми словами
    for (int i = 0; i < 200; ++i) {
        if (in.atEnd()) break;
        QString line = in.readLine().trimmed();
        QString lowerLine = line.toLower();

        // Поиск паттернов для координат
        QRegExp rxX("MINIMUM X[:\\s]*([\\-\\d\\.]+)");
        QRegExp rxY("MINIMUM Y[:\\s]*([\\-\\d\\.]+)");
        QRegExp rxMaxX("MAXIMUM X[:\\s]*([\\-\\d\\.]+)");
        QRegExp rxMaxY("MAXIMUM Y[:\\s]*([\\-\\d\\.]+)");
        QRegExp rxDims("DIMENSION X[:\\s]*(\\d+)");
        QRegExp ryDims("DIMENSION Y[:\\s]*(\\d+)");
        QRegExp rxCell("CELL SIZE[:\\s]*([\\d\\.]+)");

        if (rxX.indexIn(line) != -1) xmin = rxX.cap(1).toDouble();
        if (rxY.indexIn(line) != -1) ymin = rxY.cap(1).toDouble();
        if (rxMaxX.indexIn(line) != -1) xmax = rxMaxX.cap(1).toDouble();
        if (rxMaxY.indexIn(line) != -1) ymax = rxMaxY.cap(1).toDouble();
        if (rxDims.indexIn(line) != -1) cols = rxDims.cap(1).toInt();
        if (ryDims.indexIn(line) != -1) rows = ryDims.cap(1).toInt();
        if (rxCell.indexIn(line) != -1) cellSize = rxCell.cap(1).toDouble();
    }

    // Проверяем, нашли ли координаты
    if ((xmax > xmin) && (ymax > ymin) && rows > 0 && cols > 0) {
        // Вычисляем размер ячейки если не задан
        if (cellSize <= 0) {
            cellSize = (xmax - xmin) / cols;
        }

        m_xMin = xmin;
        m_yMin = ymin;
        m_cellSize = cellSize;
        m_cols = cols;
        m_rows = rows;

        // Теперь читаем бинарные данные
        // Находим позицию конца текстового заголовка
        file.seek(0);
        QByteArray allData = file.readAll();

        // Ищем конец текстового блока
        int headerEnd = 0;
        for (int i = 0; i < std::min((int)allData.size(), 10000); ++i) {
            char c = allData[i];
            if (c >= 32 && c <= 126 || c == '\n' || c == '\r' || c == '\t') {
                headerEnd = i;
            } else {
                bool isBinary = true;
                for(int k=0; k<10; ++k) {
                    if (i+k >= allData.size()) break;
                    char ck = allData[i+k];
                    if (ck >= 32 && ck <= 126) {
                        isBinary = false;
                        break;
                    }
                }
                if (isBinary) {
                    headerEnd = i;
                    break;
                }
            }
        }

        qDebug() << "DEMReader: Estimated header end at:" << headerEnd;

        QByteArray dataPart = allData.mid(headerEnd);
        long long expectedPoints = (long long)m_rows * m_cols;

        if (dataPart.size() >= expectedPoints * 2) {
            const short* shortData = reinterpret_cast<const short*>(dataPart.constData());
            m_elevations.reserve(expectedPoints);

            for (long long i = 0; i < expectedPoints; ++i) {
                float val = static_cast<float>(shortData[i]);
                m_elevations.append(val);
                if (val != m_noDataValue) {
                    m_minElevation = std::min(m_minElevation, (double)val);
                    m_maxElevation = std::max(m_maxElevation, (double)val);
                }
            }
            m_noDataValue = -32768.0;
            return true;
        } else if (dataPart.size() >= expectedPoints * 4) {
            const float* floatData = reinterpret_cast<const float*>(dataPart.constData());
            m_elevations.reserve(expectedPoints);

            for (long long i = 0; i < expectedPoints; ++i) {
                float val = floatData[i];
                m_elevations.append(val);
                if (val != m_noDataValue) {
                    m_minElevation = std::min(m_minElevation, (double)val);
                    m_maxElevation = std::max(m_maxElevation, (double)val);
                }
            }
            m_noDataValue = -9999.0;
            return true;
        } else {
            qWarning() << "DEMReader: Data size mismatch. Expected:" << expectedPoints << "Available bytes:" << dataPart.size();
        }
    }

    return false;
}

// Парсер для ASCII Grid формата
bool DEMReader::parseAsciiGrid(QFile &file)
{
    QTextStream in(&file);
    QString line;

    int ncols = 0, nrows = 0;
    double xll = 0, yll = 0, cell = 0, nodata = -9999;

    int headersRead = 0;
    while (headersRead < 6 && !in.atEnd()) {
        line = in.readLine().trimmed();
        QStringList parts = line.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 2) continue;

        QString key = parts[0].toLower();
        bool ok;
        double val = parts[1].toDouble(&ok);
        if (!ok) continue;

        if (key == "ncols") ncols = (int)val;
        else if (key == "nrows") nrows = (int)val;
        else if (key == "xllcorner" || key == "xll") xll = val;
        else if (key == "yllcorner" || key == "yll") yll = val;
        else if (key == "cellsize" || key == "dx") cell = val;
        else if (key == "nodata_value" || key == "nodata") nodata = val;

        if (ncols > 0 && nrows > 0 && cell > 0) headersRead++;
    }

    if (ncols <= 0 || nrows <= 0 || cell <= 0) return false;

    m_cols = ncols;
    m_rows = nrows;
    m_xMin = xll;
    m_yMin = yll;
    m_cellSize = cell;
    m_noDataValue = nodata;

    m_elevations.resize(ncols * nrows);
    m_minElevation = std::numeric_limits<double>::max();
    m_maxElevation = std::numeric_limits<double>::lowest();

    for (int r = 0; r < nrows; ++r) {
        for (int c = 0; c < ncols; ++c) {
            double val;
            in >> val;
            int idx = r * ncols + c;
            m_elevations[idx] = static_cast<float>(val);
            if (val != nodata) {
                m_minElevation = std::min(m_minElevation, val);
                m_maxElevation = std::max(m_maxElevation, val);
            }
        }
    }

    return true;
}

bool DEMReader::getElevation(double x, double y, double &height) const
{
    if (!m_isLoaded || m_cellSize <= 0) {
        height = m_noDataValue;
        return false;
    }

    // Проверка границ
    double xMax = m_xMin + m_cols * m_cellSize;
    double yMax = m_yMin + m_rows * m_cellSize;
    
    if (x < m_xMin || x > xMax || y < m_yMin || y > yMax) {
        height = m_noDataValue;
        return false;
    }

    // Вычисление индексов
    int col = static_cast<int>((x - m_xMin) / m_cellSize);
    int row = static_cast<int>((y - m_yMin) / m_cellSize);

    // Корректировка границ
    if (col < 0) col = 0;
    if (col >= m_cols) col = m_cols - 1;
    if (row < 0) row = 0;
    if (row >= m_rows) row = m_rows - 1;

    int index = row * m_cols + col;

    if (index < 0 || index >= m_elevations.size()) {
        height = m_noDataValue;
        return false;
    }

    float val = m_elevations[index];
    if (val == m_noDataValue) {
        height = m_noDataValue;
        return false;
    }

    height = static_cast<double>(val);
    return true;
}
