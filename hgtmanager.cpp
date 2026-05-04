#include "hgtmanager.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

HgtManager::HgtManager(const QString &rootPath, QObject *parent)
    : QObject(parent)
{
    scanDirectory(rootPath);
}

void HgtManager::scanDirectory(const QString &path)
{
    QDir rootDir(path);
    if (!rootDir.exists()) {
        qWarning() << "HGT root path does not exist:" << path;
        return;
    }

    QFileInfoList subDirs = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subDirInfo : subDirs) {
        QDir tileDir(subDirInfo.absoluteFilePath());
        QFileInfoList files = tileDir.entryInfoList(QStringList() << "*.hgt", QDir::Files);
        for (const QFileInfo &fileInfo : files) {
            double lat, lon;
            if (parseHgtFileName(fileInfo.fileName(), lat, lon)) {
                int latIdx = static_cast<int>(std::floor(lat));
                int lonIdx = static_cast<int>(std::floor(lon));
                QString key = QString("%1_%2").arg(latIdx).arg(lonIdx);
                m_tileMap.insert(key, fileInfo.absoluteFilePath());
            }
        }
    }
    qDebug() << "HgtManager: Indexed" << m_tileMap.size() << "tiles from" << path;
}

bool HgtManager::parseHgtFileName(const QString &fileName, double &lat, double &lon) const
{
    QRegularExpression re("([ns])(\\d+)([ew])(\\d+)\\.hgt", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = re.match(fileName);
    if (!match.hasMatch())
        return false;

    bool okLat, okLon;
    lat = match.captured(2).toInt(&okLat);
    lon = match.captured(4).toInt(&okLon);
    if (!okLat || !okLon) return false;

    if (match.captured(1).toLower() == 's') lat = -lat;
    if (match.captured(3).toLower() == 'w') lon = -lon;

    return true;
}

QString HgtManager::findFileForLocation(double lat, double lon) const
{
    int latIdx = static_cast<int>(std::floor(lat));
    int lonIdx = static_cast<int>(std::floor(lon));
    QString key = QString("%1_%2").arg(latIdx).arg(lonIdx);
    return m_tileMap.value(key, QString());
}
