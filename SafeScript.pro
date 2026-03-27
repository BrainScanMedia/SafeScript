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

# Install rules
target.path = /usr/local/bin
INSTALLS += target

desktop.path = /usr/local/share/applications
desktop.files = safescript.desktop
INSTALLS += desktop

icon.path = /usr/local/share/icons/hicolor/256x256/apps
icon.files = safescript.png
INSTALLS += icon

metainfo.path = /usr/local/share/metainfo
metainfo.files = com.brainscanmedia.SafeScript.metainfo.xml
INSTALLS += metainfo
