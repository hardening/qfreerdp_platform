TARGET = wmwidgets
TEMPLATE = lib
CONFIG += create_prl staticlib

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

