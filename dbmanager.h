#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QString>
#include <QSqlDatabase>

class DbManager
{
public:
    static DbManager& instance();
    bool connectToDb();
    void initTables(); // 创建表
    bool isOpen() const;

private:
    DbManager();
    QSqlDatabase m_db;
};

#endif // DBMANAGER_H
