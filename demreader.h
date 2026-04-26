#ifndef DEMREADER_H
#define DEMREADER_H

#include <QString>
#include <QFile>
#include <QDataStream>
#include <QMap>
#include <QPointF>
#include <QDebug>
#include <QVector>
#include <limits>

/**
 * @brief Класс для чтения и работы с DEM файлами (Digital Elevation Model)
 * 
 * Поддерживает формат USGS DEM (стандартный формат карт высот).
 * Позволяет получать высоту в указанной точке по координатам широты и долготы.
 */
class DEMReader
{
public:
    DEMReader();
    ~DEMReader();

    /**
     * @brief Открыть DEM файл
     * @param filePath Путь к файлу .dem
     * @return true если файл успешно открыт и загружен
     */
    bool openFile(const QString &filePath);

    /**
     * @brief Закрыть текущий DEM файл и освободить память
     */
    void closeFile();

    /**
     * @brief Проверка, загружена ли карта высот
     * @return true если карта высот загружена
     */
    bool isLoaded() const { return m_isLoaded; }

    /**
     * @brief Получить высоту в указанной точке
     * @param latitude Широта в градусах (WGS-84)
     * @param longitude Долгота в градусах (WGS-84)
     * @param height Выходной параметр - высота в метрах
     * @return true если высота успешно получена, false если точка вне диапазона или файл не загружен
     */
    bool getElevation(double latitude, double longitude, double &height) const;

    /**
     * @brief Получить путь к текущему загруженному файлу
     * @return Путь к файлу или пустая строка если файл не загружен
     */
    QString getFilePath() const { return m_filePath; }

    /**
     * @brief Получить минимальную высоту в загруженной модели
     * @return Минимальная высота в метрах
     */
    double getMinElevation() const { return m_minElevation; }

    /**
     * @brief Получить максимальную высоту в загруженной модели
     * @return Максимальная высота в метрах
     */
    double getMaxElevation() const { return m_maxElevation; }

private:
    /**
     * @brief Структура заголовка DEM файла (упрощенная)
     */
    struct DEMHeader {
        double xmin;      // Минимальная долгота
        double xmax;      // Максимальная долгота
        double ymin;      // Минимальная широта
        double ymax;      // Максимальная широта
        int rows;         // Количество строк (по широте)
        int cols;         // Количество столбцов (по долготе)
        double cellSize;  // Размер ячейки в градусах
    };

    /**
     * @brief Парсинг заголовка DEM файла
     * @param data Сырые данные файла
     * @return true если заголовок успешно распарсен
     */
    bool parseHeader(const QByteArray &data);

    /**
     * @brief Парсинг данных высот из DEM файла
     * @param data Сырые данные файла
     * @return true если данные успешно распаршены
     */
    bool parseElevationData(const QByteArray &data);

    /**
     * @brief Билинейная интерполяция высоты между узлами сетки
     */
    double interpolateElevation(double lat, double lon) const;

    // Состояние объекта
    bool m_isLoaded;
    QString m_filePath;
    
    // Параметры сетки высот
    DEMHeader m_header;
    
    // Данные высот (плоский массив, индексация: row * cols + col)
    QVector<double> m_elevationData;
    
    // Статистика
    double m_minElevation;
    double m_maxElevation;
};

#endif // DEMREADER_H
