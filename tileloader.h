#ifndef TILELOADER_H
#define TILELOADER_H

#include <QObject>
#include <QSqlDatabase>
#include <QPixmap>
#include <QRunnable>
#include <QThreadPool>

class TileLoader : public QObject
{
    Q_OBJECT

public:
    explicit TileLoader(QObject *parent = nullptr);
    void loadTileAsync(const QSqlDatabase &database, int z, int x, int y);
    ~TileLoader();

signals:
    void tileLoaded(int zoom, int x, int y, const QImage &tileImage);

private slots:
    void onTileLoadFinished(int z, int x, int y, const QPixmap &tile); // Новый слот

private:
    QPixmap loadTileFromDatabase(const QSqlDatabase &database, int z, int x, int y);
    QThreadPool *m_threadPool;
};


class TileLoadTask : public QRunnable
{
public:
    TileLoadTask(const QSqlDatabase &database, int z, int x, int y, TileLoader *loader)
        : m_database(database)
        , m_z(z)
        , m_x(x)
        , m_y(y)
        , m_loader(loader) {}

    void run() override;

private:
    QSqlDatabase m_database;
    int m_z, m_x, m_y;
    TileLoader *m_loader;  // Указатель на TileLoader

    QPixmap loadTileFromDatabase();
    QPixmap createPlaceholderTile(int x, int y);
};

#endif // TILELOADER_H
