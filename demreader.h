#ifndef DEMREADER_H
#define DEMREADER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QByteArray>
#include <QVector>
#include <QDebug>
#include <QTextStream>
#include <QRegExp>
#include <algorithm>
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
    bool parseUSGSDem(QFile &file);
    bool parseAsciiGrid(QFile &file);

    QString m_lastError;
    bool m_isLoaded;

    // Параметры сетки
    double m_xMin;
    double m_yMin;
    double m_cellSize;
    int m_rows;
    int m_cols;
    double m_noDataValue;
    
    // Статистика
    double m_minElevation;
    double m_maxElevation;

    // Данные высот (плоский массив)
    QVector<float> m_elevations;
};

#endif // DEMREADER_H
