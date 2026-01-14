#include "overduethread.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDate>
#include <QDebug>

OverdueThread::OverdueThread(QObject *parent) : QThread(parent), m_running(true)
{
}

void OverdueThread::stop()
{
    m_running = false;
    wait();
}

void OverdueThread::run()
{
    // 注意：多线程中使用数据库，必须建立新的连接名，不能共用主线程连接
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "ThreadConnection");
        db.setDatabaseName("library.db"); // 需与主程序路径一致（这里简化写相对路径，实际建议传参）

        if (!db.open()) {
            qDebug() << "Thread DB Connect Failed";
            return;
        }

        while (m_running) {
            // 查询未归还且 预计归还日期 < 当前日期 的记录
            QSqlQuery query(db);
            QString today = QDate::currentDate().toString("yyyy-MM-dd");

            // 简单的 SQL 查询逾期
            query.prepare("SELECT count(*) FROM records WHERE is_returned = 0 AND return_date < :today");
            query.bindValue(":today", today);

            if (query.exec() && query.next()) {
                int count = query.value(0).toInt();
                if (count > 0) {
                    emit overdueDetected(QString("警告：检测到 %1 本图书已逾期！请及时处理。").arg(count));
                }
            }

            // 模拟长任务，每 10 秒检查一次，避免并在循环过快
            for(int i=0; i<10; ++i) {
                if(!m_running) break;
                QThread::sleep(1);
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase("ThreadConnection");
}
