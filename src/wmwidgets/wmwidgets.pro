TARGET = wmwidgets
TEMPLATE = lib
CONFIG += debug nostrip force_debug_info create_prl staticlib

INCLUDEPATH += ..

SOURCES += \ 	
	wmwidget.cpp \
	wmspacer.cpp \
	wmlabel.cpp	\
	wmiconbutton.cpp \
	wmcontainer.cpp
	
	
HEADERS += \
	wmwidget.h \
	wmspacer.h \
	wmlabel.h \
	wmiconbutton.h \
	wmcontainer.h

