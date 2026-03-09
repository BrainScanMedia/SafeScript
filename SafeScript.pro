QT += core gui widgets sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = SafeScript
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    databasemanager.cpp

HEADERS += \
    mainwindow.h \
    databasemanager.h

RESOURCES += \
    resources.qrc
