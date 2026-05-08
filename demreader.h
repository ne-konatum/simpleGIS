#ifndef DEMREADER_H
#define DEMREADER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QFile>
#include <QMap>
#include <cmath>

class HgtManager;

class DEMReader : public QObject
{
    Q_OBJECT
public:
    explicit DEMReader(HgtManager *hgtManager, QObject *parent = nullptr);
    ~DEMReader();

    void setHgtManager(HgtManager *manager) { m_hgtManager = manager; }

    bool updateForLocation(double lat, double lon);
    bool updateForBounds(double minLat, double maxLat, double minLon, double maxLon);
    bool getElevation(double lat, double lon, double &height) const;

    double getMinElevation() const { return m_minElevation; }
    double getMaxElevation() const { return m_maxElevation; }
    bool isLoaded() const { return m_isLoaded; }
    QString getCurrentFile() const { return m_currentFile; }

private:
    bool loadFile(const QString &filePath);
    bool tryReadHgt(QFile &file, const QString &fileName);
    bool tryReadHgtForCache(QFile &file, const QString &fileName, QVector<float> &elevations, 
                            double &xMin, double &yMin, double &cellSize, int &rows, int &cols);
    bool tryReadBlock(QFile &file, long long offset, int rows, int cols, double lonStart, double latStart);
    bool tryReadBlockForCache(QFile &file, long long offset, int rows, int cols, 
                              double lonStart, double latStart, QVector<float> &elevations,
                              double &xMin, double &yMin, double &cellSize);
    bool parseAsciiGrid(QFile &file);
    bool parseAsciiGridForCache(QFile &file, QVector<float> &elevations, 
                                double &xMin, double &yMin, double &cellSize, 
                                int &rows, int &cols);
    void finalizeLoad();
    
    // Вспомогательный метод для получения высоты из кэша
    bool getElevationFromCache(double lat, double lon, double &height) const;

    HgtManager *m_hgtManager;
    QString m_currentFile;
    bool m_isLoaded;

    double m_xMin, m_yMin, m_xMax, m_yMax;
    double m_cellSize;
    int m_rows, m_cols;
    double m_noDataValue;
    double m_minElevation, m_maxElevation;
    QVector<float> m_elevations;
    
    // Кэш загруженных файлов по ключу (latIdx_lonIdx)
    QMap<QString, QVector<float>> m_elevationCache;
    QMap<QString, double> m_cacheXMin, m_cacheYMin, m_cacheCellSize;
    QMap<QString, int> m_cacheRows, m_cacheCols;
};

#endif // DEMREADER_H
