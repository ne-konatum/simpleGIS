#include "demreader.h"
#include "hgtmanager.h"
#include <QFileInfo>
#include <QDataStream>
#include <QRegularExpression>
#include <QRegExp>
#include <limits>
#include <QtGlobal>
#include <QDebug>

DEMReader::DEMReader(HgtManager *hgtManager, QObject *parent)
    : QObject(parent)
    , m_hgtManager(hgtManager)
    , m_isLoaded(false)
    , m_xMin(0), m_yMin(0), m_xMax(0), m_yMax(0)
    , m_cellSize(0), m_rows(0), m_cols(0)
    , m_noDataValue(-9999.0)
    , m_minElevation(0), m_maxElevation(0)
{
}

DEMReader::~DEMReader() {}

bool DEMReader::updateForLocation(double lat, double lon)
{
    if (!m_hgtManager) {
        // qDebug() << "DEMReader: HgtManager not set yet.";
        return false;
    }

    QString filePath = m_hgtManager->findFileForLocation(lat, lon);

    if (filePath.isEmpty()) {
        // Файл для этой локации не найден в индексе
        return false;
    }

    // Если файл уже загружен и это тот же самый, ничего не делаем
    if (m_isLoaded && filePath == m_currentFile) {
        return true;
    }

    // Загружаем новый файл
    return loadFile(filePath);
}

bool DEMReader::loadFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "DEMReader: Cannot open" << filePath;
        return false;
    }

    m_isLoaded = false;
    m_elevations.clear();

    QFileInfo fi(filePath);
    QString name = fi.fileName();
    QString suffix = fi.suffix().toLower();

    bool loaded = false;
    if (suffix == "hgt") {
        loaded = tryReadHgt(file, name);
    }

    if (!loaded) {
        file.seek(0);
        loaded = parseAsciiGrid(file);
    }

    if (loaded) {
        m_currentFile = filePath;
        m_isLoaded = true;
        finalizeLoad();
        return true;
    } else {
        qWarning() << "DEMReader: Failed to parse file" << filePath;
        return false;
    }
}

bool DEMReader::tryReadHgt(QFile &file, const QString &fileName)
{
    QRegularExpression re("([ns])(\\d+)([ew])(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = re.match(fileName);
    if (!match.hasMatch()) return false;

    bool okLat, okLon;
    double startLat = match.captured(2).toInt(&okLat);
    double startLon = match.captured(4).toInt(&okLon);
    if (!okLat || !okLon) return false;

    if (match.captured(1).toLower() == 's') startLat = -startLat;
    if (match.captured(3).toLower() == 'w') startLon = -startLon;

    long long fileSize = file.size();
    int rows, cols;

    if (fileSize == 3601LL * 3601LL * 2) {
        rows = 3601; cols = 3601;
    } else if (fileSize == 1201LL * 1201LL * 2) {
        rows = 1201; cols = 1201;
    } else {
        long long points = fileSize / 2;
        int sqrtPoints = static_cast<int>(std::sqrt(points));
        if (sqrtPoints * sqrtPoints == points && sqrtPoints > 100) {
            rows = sqrtPoints; cols = sqrtPoints;
        } else {
            return false;
        }
    }

    return tryReadBlock(file, 0, rows, cols, startLon, startLat);
}

bool DEMReader::tryReadBlock(QFile &file, long long offset, int rows, int cols, double lonStart, double latStart)
{
    file.seek(offset);
    QByteArray testBuffer(200, 0);
    if (file.read(testBuffer.data(), testBuffer.size()) != testBuffer.size()) return false;

    bool useBigEndian = true;
    int validBE = 0, validLE = 0;

    for (int i = 0; i < 10; ++i) {
        unsigned char b1 = testBuffer[i*2];
        unsigned char b2 = testBuffer[i*2+1];
        short valBE = (b1 << 8) | b2;
        short valLE = (b2 << 8) | b1;
        if (valBE > -100 && valBE < 6000) validBE++;
        if (valLE > -100 && valLE < 6000) validLE++;
    }
    useBigEndian = (validBE >= validLE);
    if (validBE < 5 && validLE < 5) return false;

    file.seek(offset);
    long long totalPoints = (long long)rows * cols;
    m_elevations.reserve(totalPoints);

    float minV = std::numeric_limits<float>::max();
    float maxV = std::numeric_limits<float>::lowest();
    QByteArray buffer(4096, 0);
    long long pointsRead = 0;

    while (pointsRead < totalPoints && !file.atEnd()) {
        qint64 bytesRead = file.read(buffer.data(), buffer.size());
        if (bytesRead <= 0) break;
        int count = bytesRead / 2;
        const unsigned char* raw = reinterpret_cast<const unsigned char*>(buffer.constData());

        for (int i = 0; i < count && pointsRead < totalPoints; ++i) {
            short val;
            if (useBigEndian) val = (static_cast<short>(raw[i*2]) << 8) | static_cast<short>(raw[i*2+1]);
            else val = (static_cast<short>(raw[i*2+1]) << 8) | static_cast<short>(raw[i*2]);

            float h = static_cast<float>(val);
            if (h < -500) h = -9999.0;
            m_elevations.append(h);
            if (h > -500) {
                if (h < minV) minV = h;
                if (h > maxV) maxV = h;
            }
            pointsRead++;
        }
    }

    if (pointsRead != totalPoints || minV > 5000 || maxV < -100) return false;

    m_xMin = lonStart;
    m_yMin = latStart;
    m_xMax = lonStart + 1.0;
    m_yMax = latStart + 1.0;
    m_cols = cols;
    m_rows = rows;
    m_cellSize = 1.0 / (cols - 1);
    m_minElevation = minV;
    m_maxElevation = maxV;

    return true;
}

void DEMReader::finalizeLoad()
{
    qDebug() << "DEMReader: Loaded" << m_currentFile;
    qDebug() << "DEMReader: Bounds:" << m_yMin << m_xMin << "-" << m_yMax << m_xMax;
    qDebug() << "DEMReader: Range:" << m_minElevation << "-" << m_maxElevation << "m";
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

    m_cols = ncols; m_rows = nrows;
    m_xMin = xll; m_yMin = yll; m_cellSize = cell; m_noDataValue = nodata;
    m_elevations.resize(ncols * nrows);

    float minV = std::numeric_limits<float>::max();
    float maxV = std::numeric_limits<float>::lowest();

    for (int i = 0; i < ncols * nrows; ++i) {
        double v; in >> v;
        float f = static_cast<float>(v);
        m_elevations[i] = f;
        if (f != nodata) {
            if (f < minV) minV = f;
            if (f > maxV) maxV = f;
        }
    }
    m_minElevation = minV; m_maxElevation = maxV;
    return true;
}

bool DEMReader::getElevation(double lat, double lon, double &height) const
{
    if (!m_isLoaded || m_cols == 0 || m_rows == 0) {
        height = 0;
        return false;
    }

    const double epsilon = 1e-5; // Чуть увеличил допуск

    // Строгая проверка границ
    if (lon < m_xMin - epsilon || lon > m_xMax + epsilon ||
        lat < m_yMin - epsilon || lat > m_yMax + epsilon) {
        return false;
    }

    // Ограничиваем координаты внутри диапазона [min, max]
    double clampedLon = qBound(m_xMin, lon, m_xMax);
    double clampedLat = qBound(m_yMin, lat, m_yMax);

    int col = static_cast<int>((clampedLon - m_xMin) / m_cellSize);
    int rowFromBottom = static_cast<int>((clampedLat - m_yMin) / m_cellSize);

    // Защита от выхода за границы массива из-за округления
    if (col >= m_cols) col = m_cols - 1;
    if (rowFromBottom >= m_rows) rowFromBottom = m_rows - 1;
    if (col < 0) col = 0;
    if (rowFromBottom < 0) rowFromBottom = 0;

    // Инверсия строки: данные в HGT идут с Севера на Юг
    int row = m_rows - 1 - rowFromBottom;

    int index = row * m_cols + col;

    if (index < 0 || index >= m_elevations.size()) {
        height = 0;
        return false;
    }

    height = m_elevations[index];

    // Проверка на NoData
    if (height == m_noDataValue || height < -500) {
        height = 0;
        return false;
    }

    return true;
}
