#include "demreader.h"
#include <QFileInfo>
#include <QIODevice>
#include <cmath>
#include <QDataStream>
#include <QtGlobal>
#include <QRegExp>

DEMReader::DEMReader()
    : m_isLoaded(false)
    , m_xMin(0), m_yMin(0), m_xMax(0), m_yMax(0)
    , m_cellSize(0), m_rows(0), m_cols(0)
    , m_noDataValue(-9999.0)
    , m_minElevation(0), m_maxElevation(0)
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
    m_isLoaded = false;
    m_elevations.clear();

    QFileInfo fi(filePath);
    QString name = fi.fileName();

    // 1. Пробуем прочитать как SRTM Raw (данные в конце или начале)
    if (tryReadSrtmRaw(file, name)) {
        m_isLoaded = true;
        finalizeLoad();
        return true;
    }

    // 2. Пробуем ASCII Grid
    file.seek(0);
    if (parseAsciiGrid(file)) {
        m_isLoaded = true;
        finalizeLoad();
        return true;
    }

    m_lastError = "Failed to parse: Unknown or unsupported DEM format.";
    qWarning() << "DEMReader:" << m_lastError;
    return false;
}

// Попытка прочитать как SRTM (binary short), перебирая смещения
bool DEMReader::tryReadSrtmRaw(QFile &file, const QString &fileName)
{
    // Извлекаем координаты из имени (n55e037 -> lat=55, lon=37)
    QRegExp rx("([ns])(\\d+)([ew])(\\d+)", Qt::CaseInsensitive);
    if (rx.indexIn(fileName) == -1) {
        // Если имя не стандартное, пробуем угадать или используем дефолт (не рекомендуется)
        // Для примера оставим ошибку, если имя не подходит
        // Но для вашего файла n055e037 оно подойдет
        return false;
    }

    double startLat = rx.cap(2).toDouble();
    double startLon = rx.cap(4).toDouble();
    if (rx.cap(1).toLower() == "s") startLat = -startLat;
    if (rx.cap(3).toLower() == "w") startLon = -startLon;

    int rows = 1201;
    int cols = 1201;
    long long expectedPoints = (long long)rows * cols;
    long long expectedBytes = expectedPoints * 2; // short = 2 bytes

    // Возможные смещения: 0, конец файла, или кратные 1024 (для USGS заголовков)
    QVector<long long> offsets;
    offsets.append(0);

    if (file.size() > expectedBytes) {
        offsets.append(file.size() - expectedBytes);
        // Добавляем популярные смещения для USGS DEM
        for (long long off = 1024; off < file.size() - expectedBytes; off += 1024) {
            offsets.append(off);
        }
        // Ограничим количество попыток, чтобы не зависать
        if (offsets.size() > 50) offsets.resize(50);
    }

    for (long long offset : offsets) {
        if (tryReadBlock(file, offset, rows, cols, startLon, startLat)) {
            return true;
        }
    }

    return false;
}

bool DEMReader::tryReadBlock(QFile &file, long long offset, int rows, int cols, double lonStart, double latStart)
{
    file.seek(offset);

    // Читаем небольшой чанк для проверки валидности данных
    QByteArray testBuffer;
    testBuffer.resize(200);
    if (file.read(testBuffer.data(), testBuffer.size()) != testBuffer.size()) {
        return false;
    }

    // Проверяем первые 10 значений как Big Endian и Little Endian
    bool beValid = false;
    bool leValid = false;

    auto checkRange = [](short val) {
        return (val > -100 && val < 6000); // Реалистичный диапазон высот
    };

    int validCountBE = 0;
    int validCountLE = 0;

    for (int i = 0; i < 10; ++i) {
        unsigned char b1 = testBuffer[i*2];
        unsigned char b2 = testBuffer[i*2+1];

        short valBE = (b1 << 8) | b2;
        short valLE = (b2 << 8) | b1;

        if (checkRange(valBE)) validCountBE++;
        if (checkRange(valLE)) validCountLE++;
    }

    bool useBigEndian = (validCountBE >= validCountLE);

    // Если ни один вариант не дал валидных высот, пропускаем это смещение
    if (validCountBE < 5 && validCountLE < 5) {
        return false;
    }

    // Читаем весь блок
    file.seek(offset);
    long long totalPoints = (long long)rows * cols;
    m_elevations.clear();
    m_elevations.reserve(totalPoints);

    float minV = std::numeric_limits<float>::max();
    float maxV = std::numeric_limits<float>::lowest();

    QByteArray buffer;
    buffer.resize(4096);
    long long pointsRead = 0;

    while (pointsRead < totalPoints && !file.atEnd()) {
        qint64 bytesRead = file.read(buffer.data(), buffer.size());
        if (bytesRead <= 0) break;

        int count = bytesRead / 2;
        const unsigned char* raw = reinterpret_cast<const unsigned char*>(buffer.constData());

        for (int i = 0; i < count && pointsRead < totalPoints; ++i) {
            short val;
            if (useBigEndian) {
                val = (static_cast<short>(raw[i*2]) << 8) | static_cast<short>(raw[i*2+1]);
            } else {
                val = (static_cast<short>(raw[i*2+1]) << 8) | static_cast<short>(raw[i*2]);
            }

            float h = static_cast<float>(val);
            if (h < -500) h = -9999.0; // NoData

            m_elevations.append(h);
            if (h > -500) {
                if (h < minV) minV = h;
                if (h > maxV) maxV = h;
            }
            pointsRead++;
        }
    }

    if (pointsRead != totalPoints) {
        return false; // Не дочитали
    }

    // Финальная проверка диапазона
    if (minV > 5000 || maxV < -100) {
        return false; // Нереалистичные высоты
    }

    // Успех! Записываем параметры
    m_xMin = lonStart;
    m_yMin = latStart;
    m_xMax = lonStart + 1.0;
    m_yMax = latStart + 1.0;
    m_cols = cols;
    m_rows = rows;
    m_cellSize = 1.0 / (cols - 1);
    m_minElevation = minV;
    m_maxElevation = maxV;

    qDebug() << "DEMReader: Successfully read block at offset" << offset
             << "Range:" << minV << "-" << maxV
             << "Endian:" << (useBigEndian ? "BE" : "LE");

    return true;
}

void DEMReader::finalizeLoad()
{
    qDebug() << "DEMReader: Bounds:" << m_xMin << m_yMin << m_xMax << m_yMax;
    qDebug() << "DEMReader: Grid:" << m_cols << "x" << m_rows;
    qDebug() << "DEMReader: Elevation range:" << m_minElevation << "-" << m_maxElevation << "m";
}

bool DEMReader::parseAsciiGrid(QFile &file)
{
    QTextStream in(&file);
    int ncols = 0, nrows = 0;
    double xll = 0, yll = 0, cell = 0, nodata = -9999;

    for (int i = 0; i < 6; ++i) {
        if (in.atEnd()) break;
        QString line = in.readLine().trimmed();
        QStringList parts = line.split(QRegExp("\\s+"), QString::SkipEmptyParts);
        if (parts.size() < 2) continue;

        QString key = parts[0].toLower();
        double val = parts[1].toDouble();

        if (key == "ncols") ncols = (int)val;
        else if (key == "nrows") nrows = (int)val;
        else if (key.startsWith("xll")) xll = val;
        else if (key.startsWith("yll")) yll = val;
        else if (key.contains("cell")) cell = val;
        else if (key.contains("nodata")) nodata = val;
    }

    if (ncols <= 0 || nrows <= 0) return false;

    m_cols = ncols;
    m_rows = nrows;
    m_xMin = xll;
    m_yMin = yll;
    m_cellSize = cell;
    m_noDataValue = nodata;

    m_elevations.resize(ncols * nrows);
    float minV = std::numeric_limits<float>::max();
    float maxV = std::numeric_limits<float>::lowest();

    for (int i = 0; i < ncols * nrows; ++i) {
        double v;
        in >> v;
        float f = static_cast<float>(v);
        m_elevations[i] = f;
        if (f != nodata) {
            if (f < minV) minV = f;
            if (f > maxV) maxV = f;
        }
    }

    m_minElevation = minV;
    m_maxElevation = maxV;
    return true;
}

bool DEMReader::getElevation(double x, double y, double &height) const
{
    if (!m_isLoaded || m_cols == 0 || m_rows == 0) {
        height = 0;
        return false;
    }

    // Проверка границ
    if (x < m_xMin || x > m_xMax || y < m_yMin || y > m_yMax) {
        height = 0;
        return false;
    }

    int col = static_cast<int>((x - m_xMin) / m_cellSize);
    int rowFromBottom = static_cast<int>((y - m_yMin) / m_cellSize);

    if (col < 0) col = 0;
    if (col >= m_cols) col = m_cols - 1;
    if (rowFromBottom < 0) rowFromBottom = 0;
    if (rowFromBottom >= m_rows) rowFromBottom = m_rows - 1;

    // Инверсия строки: данные хранятся с севера на юг (строка 0 - это верх/Ymax)
    int row = m_rows - 1 - rowFromBottom;

    int index = row * m_cols + col;
    if (index < 0 || index >= m_elevations.size()) {
        height = 0;
        return false;
    }

    height = m_elevations[index];
    if (height == m_noDataValue) {
        height = 0;
        return false;
    }

    return true;
}