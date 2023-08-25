CONFIG += force_debug_info debug nostrip link_prl
QT += core widgets

SOURCES += wmwidgetsTests.cpp
INCLUDEPATH += ..
LIBS += -L../wmwidgets -lwmwidgets
RESOURCES += wmwidgetsTests.qrc