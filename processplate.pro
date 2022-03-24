#-------------------------------------------------
#
# Project created by QtCreator 2019-08-10T12:27:53
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += link_pkgconfig
PKGCONFIG += opencv4
TARGET = TutorialOpenCV
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

SOURCES += \
        main.cpp \
        mainwindow.cpp
HEADERS += \
        mainwindow.h

FORMS += \
        mainwindow.ui

# Default rules for deployment.
# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/DarkHelp/build/src-lib/release/ -ldarkhelp
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/DarkHelp/build/src-lib/debug/ -ldarkhelp
else:unix: LIBS += -L$$PWD/DarkHelp/build/src-lib/ -ldarkhelp

INCLUDEPATH += $$PWD/DarkHelp/build/src-lib
DEPENDPATH += $$PWD/DarkHelp/build/src-lib

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/DarkHelp/build/src-lib/release/libdarkhelp.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/DarkHelp/build/src-lib/debug/libdarkhelp.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/DarkHelp/build/src-lib/release/darkhelp.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/DarkHelp/build/src-lib/debug/darkhelp.lib
else:unix: PRE_TARGETDEPS += $$PWD/DarkHelp/build/src-lib/libdarkhelp.a

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/darknet/release/ -ldarknet
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/darknet/debug/ -ldarknet
else:unix: LIBS += -L$$PWD/darknet/ -ldarknet

INCLUDEPATH += $$PWD/darknet
DEPENDPATH += $$PWD/darknet
