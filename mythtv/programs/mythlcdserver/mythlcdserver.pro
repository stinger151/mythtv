include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythlcdserver
target.path = $${PREFIX}/bin

INSTALLS += target

HEADERS += lcdserver.h  lcdprocclient.h commandlineparser.h

SOURCES += main.cpp lcdserver.cpp lcdprocclient.cpp commandlineparser.cpp

QT += network xml sql

using_opengl:QT += opengl
