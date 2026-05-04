#ifndef HGTMANAGER_H
#define HGTMANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <cmath>

class HgtManager : public QObject
{
    Q_OBJECT
public:
    explicit HgtManager(const QString &rootPath, QObject *parent = nullptr);
    QString findFileForLocation(double lat, double lon) const;
    int tileCount() const { return m_tileMap.size(); }

private:
    void scanDirectory(const QString &path);
    bool parseHgtFileName(const QString &fileName, double &lat, double &lon) const;
    QMap<QString, QString> m_tileMap;
};

#endif // HGTMANAGER_H
