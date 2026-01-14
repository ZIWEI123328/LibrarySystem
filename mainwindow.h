#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlTableModel>
#include <QSqlRelationalTableModel>
#include <QTableView>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>

class OverdueThread;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // --- 按钮功能槽函数 ---
    void onAddBook();          // 添加图书
    void onDeleteBook();       // 删除图书
    void onAddReader();        // 添加读者
    void onDeleteReader();     // 删除读者
    void onBorrowBook();       // 借书动作
    void onReturnBook();       // 还书动作

    void onExportData();       // 导出报表
    void showOverdueAlert(QString msg); // 逾期弹窗

private:
    void setupModels();        // 初始化数据库模型
    void setupUI();            // 初始化界面布局

    // --- 辅助构建界面的函数 ---
    QWidget* createBookTab();
    QWidget* createReaderTab();
    QWidget* createBorrowTab();

    // --- 核心 Model ---
    QSqlTableModel *bookModel;
    QSqlTableModel *readerModel;
    QSqlRelationalTableModel *recordModel;

    // --- UI 控件指针 (为了在槽函数中获取输入值) ---
    // 图书管理页控件
    QTableView *bookView;
    QLineEdit *editBookTitle;
    QLineEdit *editBookAuthor;
    QSpinBox *spinBookCount;

    // 读者管理页控件
    QTableView *readerView;
    QLineEdit *editReaderName;
    QLineEdit *editReaderPhone;

    // 借阅管理页控件
    QTableView *recordView;
    QComboBox *comboBorrowReader; // 选择读者的下拉框
    QComboBox *comboBorrowBook;   // 选择图书的下拉框

    OverdueThread *m_thread;
};

#endif // MAINWINDOW_H
