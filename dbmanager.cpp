#include "dbmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDate>
#include <QCoreApplication>
#include <QDir>

DbManager::DbManager() {}

DbManager& DbManager::instance()
{
    static DbManager instance;
    return instance;
}

bool DbManager::connectToDb()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    // 将数据库文件放在运行目录下
    QString path = QCoreApplication::applicationDirPath() + "/library.db";
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        qDebug() << "Error: connection with database failed";
        return false;
    } else {
        qDebug() << "Database: connection ok. Path:" << path;
        initTables();
        return true;
    }
}

void DbManager::initTables()
{
    QSqlQuery query;

    // 1. 创建图书表
    query.exec("CREATE TABLE IF NOT EXISTS books ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "title TEXT NOT NULL, "
               "author TEXT, "
               "total_count INTEGER, "
               "current_count INTEGER)");

    // 2. 创建读者表
    query.exec("CREATE TABLE IF NOT EXISTS readers ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "name TEXT NOT NULL, "
               "phone TEXT)");

    // 3. 创建借阅记录表
    query.exec("CREATE TABLE IF NOT EXISTS records ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "book_id INTEGER, "
               "reader_id INTEGER, "
               "borrow_date TEXT, "
               "return_date TEXT, "
               "is_returned INTEGER DEFAULT 0)");

    // --- 插入一些测试数据（如果表是空的） ---
    query.exec("SELECT count(*) FROM books");
    if (query.next() && query.value(0).toInt() == 0) {
        qDebug() << "Inserting mock data...";
        query.exec("INSERT INTO books (title, author, total_count, current_count) VALUES ('Qt C++编程', '张三', 5, 5)");
        query.exec("INSERT INTO books (title, author, total_count, current_count) VALUES ('深入理解计算机系统', 'Bryant', 3, 3)");
        query.exec("INSERT INTO readers (name, phone) VALUES ('李四', '13800000001')");
        query.exec("INSERT INTO readers (name, phone) VALUES ('王五', '13800000002')");

        // 插入一条故意过期的记录用于测试线程功能 (日期设为 10 天前)
        QString pastDate = QDate::currentDate().addDays(-10).toString("yyyy-MM-dd");
        query.exec(QString("INSERT INTO records (book_id, reader_id, borrow_date, return_date, is_returned) "
                           "VALUES (1, 1, '%1', '%1', 0)").arg(pastDate));
    }
}

bool DbManager::isOpen() const
{
    return m_db.isOpen();
}
