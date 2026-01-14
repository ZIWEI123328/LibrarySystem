#ifndef OVERDUETHREAD_H
#define OVERDUETHREAD_H

#include <QThread>

class OverdueThread : public QThread
{
    Q_OBJECT
public:
    explicit OverdueThread(QObject *parent = nullptr);
    void stop();

protected:
    void run() override;

signals:
    void overdueDetected(QString message); // 发送信号给主界面

private:
    bool m_running;
};

#endif // OVERDUETHREAD_H
