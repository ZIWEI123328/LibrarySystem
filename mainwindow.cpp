#include "mainwindow.h"
#include "dbmanager.h"
#include "overduethread.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QTabWidget>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QSqlRelationalDelegate>
#include <QSqlRecord>
#include <QDate>
#include <QLabel>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 1. 确保数据库已连接
    if (!DbManager::instance().isOpen()) {
        DbManager::instance().connectToDb();
    }

    // 2. 初始化模型和界面
    setupModels();
    setupUI();

    // 3. 启动后台检测线程
    m_thread = new OverdueThread(this);
    connect(m_thread, &OverdueThread::overdueDetected, this, &MainWindow::showOverdueAlert);
    m_thread->start();

    // 设置窗口属性
    resize(900, 600);
    setWindowTitle("图书与借阅管理系统 (Course Design)");
}

MainWindow::~MainWindow()
{
    if (m_thread) {
        m_thread->stop();
        m_thread->wait();
    }
}

void MainWindow::setupModels()
{
    // --- 1. 图书模型 ---
    bookModel = new QSqlTableModel(this);
    bookModel->setTable("books");
    bookModel->setEditStrategy(QSqlTableModel::OnManualSubmit); // 改为手动提交，防止误操作
    bookModel->select();
    bookModel->setHeaderData(1, Qt::Horizontal, "书名");
    bookModel->setHeaderData(2, Qt::Horizontal, "作者");
    bookModel->setHeaderData(3, Qt::Horizontal, "总库存");
    bookModel->setHeaderData(4, Qt::Horizontal, "当前在馆");

    // --- 2. 读者模型 ---
    readerModel = new QSqlTableModel(this);
    readerModel->setTable("readers");
    readerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    readerModel->select();
    readerModel->setHeaderData(1, Qt::Horizontal, "姓名");
    readerModel->setHeaderData(2, Qt::Horizontal, "电话");

    // --- 3. 借阅记录模型 (关联模型) ---
    recordModel = new QSqlRelationalTableModel(this);
    recordModel->setTable("records");
    // 设置外键: records.book_id 显示 books.title
    recordModel->setRelation(1, QSqlRelation("books", "id", "title"));
    // 设置外键: records.reader_id 显示 readers.name
    recordModel->setRelation(2, QSqlRelation("readers", "id", "name"));

    recordModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    recordModel->select();
    recordModel->setHeaderData(1, Qt::Horizontal, "图书名称");
    recordModel->setHeaderData(2, Qt::Horizontal, "读者姓名");
    recordModel->setHeaderData(3, Qt::Horizontal, "借出日期");
    recordModel->setHeaderData(4, Qt::Horizontal, "应还日期");
    recordModel->setHeaderData(5, Qt::Horizontal, "是否归还(1=是)");
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // 顶部标题
    QLabel *titleLabel = new QLabel("图书借阅管理系统");
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #333; margin: 10px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // 核心 Tab 组件
    QTabWidget *tabWidget = new QTabWidget();
    tabWidget->addTab(createBookTab(), "📚 图书管理");
    tabWidget->addTab(createReaderTab(), "👤 读者管理");
    tabWidget->addTab(createBorrowTab(), "🔄 借还处理");

    mainLayout->addWidget(tabWidget);

    // 底部状态栏区域
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    QPushButton *btnExport = new QPushButton("导出借阅报表");
    connect(btnExport, &QPushButton::clicked, this, &MainWindow::onExportData);
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnExport);
    mainLayout->addLayout(bottomLayout);
}

// === 构建“图书管理”页面的代码 ===
QWidget* MainWindow::createBookTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // 1. 表格视图
    bookView = new QTableView();
    bookView->setModel(bookModel);
    bookView->setSelectionBehavior(QAbstractItemView::SelectRows);
    bookView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    bookView->hideColumn(0); // 隐藏ID列
    layout->addWidget(bookView);

    // 2. 操作区 (添加图书)
    QGroupBox *groupBox = new QGroupBox("新书入库");
    QHBoxLayout *inputLayout = new QHBoxLayout(groupBox);

    editBookTitle = new QLineEdit();
    editBookTitle->setPlaceholderText("请输入书名");

    editBookAuthor = new QLineEdit();
    editBookAuthor->setPlaceholderText("作者");

    spinBookCount = new QSpinBox();
    spinBookCount->setRange(1, 100);
    spinBookCount->setSuffix(" 本");

    QPushButton *btnAdd = new QPushButton("添加");
    QPushButton *btnDel = new QPushButton("删除选中");

    // 简单的 QSS 美化
    btnAdd->setStyleSheet("background-color: #4CAF50; color: white;");
    btnDel->setStyleSheet("background-color: #F44336; color: white;");

    inputLayout->addWidget(new QLabel("书名:"));
    inputLayout->addWidget(editBookTitle);
    inputLayout->addWidget(new QLabel("作者:"));
    inputLayout->addWidget(editBookAuthor);
    inputLayout->addWidget(new QLabel("数量:"));
    inputLayout->addWidget(spinBookCount);
    inputLayout->addWidget(btnAdd);
    inputLayout->addWidget(btnDel);

    layout->addWidget(groupBox);

    // 连接信号槽
    connect(btnAdd, &QPushButton::clicked, this, &MainWindow::onAddBook);
    connect(btnDel, &QPushButton::clicked, this, &MainWindow::onDeleteBook);

    return tab;
}

// === 构建“读者管理”页面的代码 ===
QWidget* MainWindow::createReaderTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // 1. 表格
    readerView = new QTableView();
    readerView->setModel(readerModel);
    readerView->setSelectionBehavior(QAbstractItemView::SelectRows);
    readerView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    readerView->hideColumn(0);
    layout->addWidget(readerView);

    // 2. 操作区
    QGroupBox *groupBox = new QGroupBox("注册新读者");
    QHBoxLayout *inputLayout = new QHBoxLayout(groupBox);

    editReaderName = new QLineEdit();
    editReaderName->setPlaceholderText("姓名");
    editReaderPhone = new QLineEdit();
    editReaderPhone->setPlaceholderText("电话号码");

    QPushButton *btnAdd = new QPushButton("添加读者");
    QPushButton *btnDel = new QPushButton("删除选中");
    btnAdd->setStyleSheet("background-color: #2196F3; color: white;");

    inputLayout->addWidget(new QLabel("姓名:"));
    inputLayout->addWidget(editReaderName);
    inputLayout->addWidget(new QLabel("电话:"));
    inputLayout->addWidget(editReaderPhone);
    inputLayout->addWidget(btnAdd);
    inputLayout->addWidget(btnDel);

    layout->addWidget(groupBox);

    connect(btnAdd, &QPushButton::clicked, this, &MainWindow::onAddReader);
    connect(btnDel, &QPushButton::clicked, this, &MainWindow::onDeleteReader);

    return tab;
}

// === 构建“借还处理”页面的代码 ===
QWidget* MainWindow::createBorrowTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // 1. 借阅操作区 (Form Layout)
    QGroupBox *borrowGroup = new QGroupBox("办理借阅");
    QHBoxLayout *borrowLayout = new QHBoxLayout(borrowGroup);

    comboBorrowReader = new QComboBox();
    comboBorrowReader->setModel(readerModel);
    comboBorrowReader->setModelColumn(1); // 显示姓名列

    comboBorrowBook = new QComboBox();
    comboBorrowBook->setModel(bookModel);
    comboBorrowBook->setModelColumn(1); // 显示书名列

    QPushButton *btnBorrow = new QPushButton("确认借阅");
    btnBorrow->setStyleSheet("background-color: #FF9800; color: white; font-weight: bold;");

    borrowLayout->addWidget(new QLabel("选择读者:"));
    borrowLayout->addWidget(comboBorrowReader);
    borrowLayout->addWidget(new QLabel("选择图书:"));
    borrowLayout->addWidget(comboBorrowBook);
    borrowLayout->addWidget(btnBorrow);

    layout->addWidget(borrowGroup);

    // 2. 借阅记录列表
    layout->addWidget(new QLabel("当前借阅记录 (选中记录点击归还):"));
    recordView = new QTableView();
    recordView->setModel(recordModel);
    recordView->setItemDelegate(new QSqlRelationalDelegate(recordView));
    recordView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    recordView->setSelectionBehavior(QAbstractItemView::SelectRows);
    recordView->hideColumn(0);
    layout->addWidget(recordView);

    // 3. 归还按钮
    QPushButton *btnReturn = new QPushButton("归还选中的图书");
    btnReturn->setStyleSheet("height: 40px; font-size: 14px;");
    layout->addWidget(btnReturn);

    connect(btnBorrow, &QPushButton::clicked, this, &MainWindow::onBorrowBook);
    connect(btnReturn, &QPushButton::clicked, this, &MainWindow::onReturnBook);

    return tab;
}

// --- 槽函数实现 ---

void MainWindow::onAddBook()
{
    QString title = editBookTitle->text();
    QString author = editBookAuthor->text();
    int count = spinBookCount->value();

    if(title.isEmpty()) return;

    QSqlRecord record = bookModel->record();
    record.setValue("title", title);
    record.setValue("author", author);
    record.setValue("total_count", count);
    record.setValue("current_count", count);

    if(bookModel->insertRecord(-1, record)) {
        bookModel->submitAll(); // 提交到数据库
        editBookTitle->clear();
        editBookAuthor->clear();
        QMessageBox::information(this, "成功", "图书添加成功！");
    } else {
        QMessageBox::warning(this, "失败", bookModel->lastError().text());
    }
}

void MainWindow::onDeleteBook()
{
    int row = bookView->currentIndex().row();
    if(row < 0) return;

    if(QMessageBox::Yes == QMessageBox::question(this, "确认", "确定删除这本书吗？")) {
        bookModel->removeRow(row);
        bookModel->submitAll();
    }
}

void MainWindow::onAddReader()
{
    QString name = editReaderName->text();
    QString phone = editReaderPhone->text();

    if(name.isEmpty()) return;

    QSqlRecord record = readerModel->record();
    record.setValue("name", name);
    record.setValue("phone", phone);

    if(readerModel->insertRecord(-1, record)) {
        readerModel->submitAll();
        editReaderName->clear();
        editReaderPhone->clear();
        QMessageBox::information(this, "成功", "读者添加成功！");
    }
}

void MainWindow::onDeleteReader()
{
    int row = readerView->currentIndex().row();
    if(row < 0) return;
    readerModel->removeRow(row);
    readerModel->submitAll();
}

void MainWindow::onBorrowBook()
{
    // 1. 获取选中的行索引
    int readerRow = comboBorrowReader->currentIndex();
    int bookRow = comboBorrowBook->currentIndex();

    if (readerRow < 0 || bookRow < 0) {
        QMessageBox::warning(this, "提示", "请先选择读者和图书");
        return;
    }

    // 2. 获取实际的 ID 和库存
    QSqlRecord readerRec = readerModel->record(readerRow);
    int readerId = readerRec.value("id").toInt();

    QSqlRecord bookRec = bookModel->record(bookRow);
    int bookId = bookRec.value("id").toInt();
    int currentCount = bookRec.value("current_count").toInt();

    // 3. 检查库存
    if (currentCount <= 0) {
        QMessageBox::warning(this, "失败", "该书库存不足，无法借阅！");
        return;
    }

    // --- 【修改开始：改用 QSqlQuery 直接插入】 ---
    QSqlQuery query;
    query.prepare("INSERT INTO records (book_id, reader_id, borrow_date, return_date, is_returned) "
                  "VALUES (:book, :reader, :borrow, :return, 0)");

    query.bindValue(":book", bookId);
    query.bindValue(":reader", readerId);
    query.bindValue(":borrow", QDate::currentDate().toString("yyyy-MM-dd"));
    query.bindValue(":return", QDate::currentDate().addDays(30).toString("yyyy-MM-dd"));

    if (query.exec()) {
        // 4. 插入成功后，必须刷新 recordModel，界面才会显示刚刚插入的记录
        recordModel->select();

        // 5. 扣减库存 (同理，也可以用 SQL 更新，或者继续用 Model 更新)
        // 这里用 Model 更新库存没问题，因为 books 表没有 setRelation，结构是原始的
        bookRec.setValue("current_count", currentCount - 1);
        bookModel->setRecord(bookRow, bookRec);
        bookModel->submitAll(); // 提交库存变更
        bookModel->select();    // 刷新显示

        QMessageBox::information(this, "成功", "借阅成功！");
    } else {
        QMessageBox::critical(this, "错误", "借阅失败: " + query.lastError().text());
    }
    // --- 【修改结束】 ---
}

void MainWindow::onReturnBook()
{
    // 1. 獲取當前選中的行
    QModelIndex currentIndex = recordView->currentIndex();
    if (!currentIndex.isValid()) {
        QMessageBox::warning(this, "提示", "請先在表格中選中一條借閱記錄");
        return;
    }
    int row = currentIndex.row();

    // 2. 【核心修復】使用 Qt::EditRole 強制獲取原始 ID
    // 即使界面顯示的是書名，EditRole 也能穿透關聯，拿到底層數據庫真實的數字 ID
    // 假設你的 records 表列索引為：0:id, 1:book_id, 2:reader_id, 5:is_returned
    int recordId = recordModel->index(row, 0).data(Qt::EditRole).toInt();
    int bookId   = recordModel->index(row, 1).data(Qt::EditRole).toInt();
    int isReturned = recordModel->index(row, 5).data(Qt::EditRole).toInt();

    // 偵錯輸出：你可以在 Qt Creator 的“應用程序輸出”窗口看到這兩個 ID 是否為正整數
    qDebug() << "Debug -> RecordID:" << recordId << "BookID:" << bookId;

    // 3. 狀態檢查
    if (isReturned == 1) {
        QMessageBox::information(this, "提示", "該書已歸還過");
        return;
    }

    if (bookId <= 0) {
        QMessageBox::critical(this, "錯誤", "無法提取正確的圖書 ID，請檢查數據庫列索引");
        return;
    }

    // 4. 執行數據庫事務更新
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction(); // 開啟事務，保證兩個更新同時成功或失敗

    QSqlQuery query;

    // 動作 A：將借閱狀態改為已還 (is_returned = 1)
    query.prepare("UPDATE records SET is_returned = 1 WHERE id = ?");
    query.addBindValue(recordId);
    if (!query.exec()) {
        db.rollback();
        QMessageBox::critical(this, "錯誤", "更新記錄失敗：" + query.lastError().text());
        return;
    }

    // 動作 B：【關鍵】增加 books 表中對應書籍的庫存 (current_count + 1)
    query.prepare("UPDATE books SET current_count = current_count + 1 WHERE id = ?");
    query.addBindValue(bookId);

    if (query.exec() && query.numRowsAffected() > 0) {
        db.commit(); // 提交事務

        // 5. 強制刷新 Model，使界面立即顯示最新狀態
        recordModel->select(); // 借閱列表會顯示“已歸還”
        bookModel->select();   // 圖書管理頁面的庫存數字會增加

        QMessageBox::information(this, "成功", "歸還成功，庫存已自動恢復！");
    } else {
        db.rollback();
        QMessageBox::critical(this, "錯誤", "庫存更新失敗：未能在圖書表中找到 ID 為 " + QString::number(bookId) + " 的書籍");
    }
}

void MainWindow::onExportData()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出记录", "borrow_history.csv", "CSV Files (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&file);
        // 为了解决中文乱码，建议使用 UTF-8 With BOM
        out.setEncoding(QStringConverter::Utf8);
        out << "\uFEFF"; // BOM

        out << "Book,Reader,BorrowDate,ReturnDate,IsReturned\n";

        for (int i = 0; i < recordModel->rowCount(); ++i) {
            QSqlRecord rec = recordModel->record(i);
            out << rec.value(1).toString() << "," // 书名 (因为 Model 已经 setRelation)
                << rec.value(2).toString() << "," // 读者名
                << rec.value("borrow_date").toString() << ","
                << rec.value("return_date").toString() << ","
                << (rec.value("is_returned").toInt() == 1 ? "Yes" : "No")
                << "\n";
        }
        file.close();
        QMessageBox::information(this, "成功", "文件导出成功！");
    }
}

void MainWindow::showOverdueAlert(QString msg)
{
    // 系统托盘气泡或弹窗
    QMessageBox::warning(this, "系统消息", msg);
}
