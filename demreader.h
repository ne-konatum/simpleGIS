#ifndef DEMREADER_H
#define DEMREADER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QVector>
#include <QDebug>
#include <limits>

class DEMReader : public QObject
{
    Q_OBJECT
public:
    explicit DEMReader();
    ~DEMReader();

    bool openFile(const QString &filePath);
    bool getElevation(double x, double y, double &height) const;
    double getMinElevation() const { return m_minElevation; }
    double getMaxElevation() const { return m_maxElevation; }
    bool isLoaded() const { return m_isLoaded; }
    QString getLastError() const { return m_lastError; }

private:
    bool tryReadSrtmRaw(QFile &file, const QString &fileName);
    bool tryReadBlock(QFile &file, long long offset, int rows, int cols, double lonStart, double latStart);
    bool parseAsciiGrid(QFile &file);
    void finalizeLoad();

    QString m_lastError;
    bool m_isLoaded;

    double m_xMin;
    double m_yMin;
    double m_xMax;
    double m_yMax;
    double m_cellSize;
    int m_rows;
    int m_cols;
    double m_noDataValue;

    double m_minElevation;
    double m_maxElevation;

    QVector<float> m_elevations;
};

#endif // DEMREADER_H