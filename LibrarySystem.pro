QT       += core gui sql widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# 启用控制台输出以便调试
CONFIG += console

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    dbmanager.cpp \
    overduethread.cpp

HEADERS += \
    mainwindow.h \
    dbmanager.h \
    overduethread.h
