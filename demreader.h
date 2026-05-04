#ifndef DEMREADER_H
#define DEMREADER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QFile>
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
    bool getElevation(double lat, double lon, double &height) const;

    double getMinElevation() const { return m_minElevation; }
    double getMaxElevation() const { return m_maxElevation; }
    bool isLoaded() const { return m_isLoaded; }
    QString getCurrentFile() const { return m_currentFile; }

private:
    bool loadFile(const QString &filePath);
    bool tryReadHgt(QFile &file, const QString &fileName);
    bool tryReadBlock(QFile &file, long long offset, int rows, int cols, double lonStart, double latStart);
    bool parseAsciiGrid(QFile &file);
    void finalizeLoad();

    HgtManager *m_hgtManager;
    QString m_currentFile;
    bool m_isLoaded;

    double m_xMin, m_yMin, m_xMax, m_yMax;
    double m_cellSize;
    int m_rows, m_cols;
    double m_noDataValue;
    double m_minElevation, m_maxElevation;
    QVector<float> m_elevations;
};

#endif // DEMREADER_H
