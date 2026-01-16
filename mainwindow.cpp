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
    // 1. 获取 ComboBox 选中的行索引
    int readerRow = comboBorrowReader->currentIndex();
    int bookRow = comboBorrowBook->currentIndex();

    if (readerRow < 0 || bookRow < 0) {
        QMessageBox::warning(this, "提示", "请先选择读者和图书");
        return;
    }

    // 2. 【核心修复】从 Model 的原始记录中提取 ID
    // 必须使用 record(row).value("id")，确保拿到的是数据库中的主键数字
    int readerId = readerModel->record(readerRow).value("id").toInt();
    int bookId   = bookModel->record(bookRow).value("id").toInt();

    // 获取当前在馆数量
    int currentCount = bookModel->record(bookRow).value("current_count").toInt();

    // 诊断：如果这里输出 0，说明 bookModel 的列名不是 "id"
    qDebug() << "Debug -> ReaderID:" << readerId << "BookID:" << bookId << "Stock:" << currentCount;

    if (bookId <= 0 || readerId <= 0) {
        QMessageBox::critical(this, "错误", "无法从表格模型中提取有效的 ID，请检查数据库初始化。");
        return;
    }

    // 3. 库存检查
    if (currentCount <= 0) {
        QMessageBox::warning(this, "失败", "该书库存不足，无法借阅！");
        return;
    }

    // 4. 执行数据库事务
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery query;

    // 步骤 A：插入借阅记录
    query.prepare("INSERT INTO records (book_id, reader_id, borrow_date, return_date, is_returned) "
                  "VALUES (:bid, :rid, :bdate, :rdate, 0)");
    query.bindValue(":bid", bookId);
    query.bindValue(":rid", readerId);
    query.bindValue(":bdate", QDate::currentDate().toString("yyyy-MM-dd"));
    query.bindValue(":rdate", QDate::currentDate().addDays(30).toString("yyyy-MM-dd"));

    if (!query.exec()) {
        db.rollback();
        QMessageBox::critical(this, "错误", "写入记录失败: " + query.lastError().text());
        return;
    }

    // 步骤 B：扣减图书表库存
    query.prepare("UPDATE books SET current_count = current_count - 1 WHERE id = :bid");
    query.bindValue(":bid", bookId);

    if (query.exec()) {
        db.commit();

        // 5. 同步刷新所有模型
        recordModel->select();
        bookModel->select();

        QMessageBox::information(this, "成功", "借阅办理成功！");
    } else {
        db.rollback();
        QMessageBox::critical(this, "错误", "扣减库存失败: " + query.lastError().text());
    }
}
void MainWindow::onReturnBook()
{
    int row = recordView->currentIndex().row();
    if (row < 0) return;

    // 1. 強制從 EditRole 獲取原始整數
    QVariant varBookId = recordModel->index(row, 1).data(Qt::EditRole);
    int bookId = varBookId.toInt();

    // 2. 增加這段報警，幫助你精確定位問題
    if (varBookId.isNull() || bookId <= 0) {
        QString rawData = recordModel->index(row, 1).data().toString();
        QMessageBox::critical(this, "數據錯誤",
                              QString("數據庫中的 BookID 為空或 0！\n當前單元格顯示內容為: %1\n這說明借閱時數據就沒存對。").arg(rawData));
        return;
    }

    // 3. 執行 SQL 更新 (同前...)
    QSqlQuery query;
    query.prepare("UPDATE books SET current_count = current_count + 1 WHERE id = ?");
    query.addBindValue(bookId);

    if (query.exec() && query.numRowsAffected() > 0) {
        QSqlDatabase::database().commit();
        recordModel->select();
        bookModel->select();
        QMessageBox::information(this, "成功", "歸還成功！");
    } else {
        QMessageBox::critical(this, "失敗", "找不到 ID 為 " + QString::number(bookId) + " 的書");
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
