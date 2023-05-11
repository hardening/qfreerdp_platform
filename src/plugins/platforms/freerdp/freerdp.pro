PLUGIN_TYPE=platforms
PLUGIN_CLASS_NAME = QFreeRdpIntegrationPlugin
load(qt_plugin)

TARGET=qfreerdp

isEmpty(PREFIX) {
    PREFIX = /usr
}

DESTDIR = $$PREFIX/lib/platforms

unix {
    CONFIG += link_pkgconfig # debug nostrip
    PKGCONFIG += freerdp3 freerdp-server3 winpr3 winpr-tools3 xkbcommon glib-2.0 xcursor
}

*-g++* {
    QMAKE_CXXFLAGS += -Wformat -Wformat-security -Werror=format-security
    QMAKE_CXXFLAGS += -D_FORTIFY_SOURCE=2 -fPIC
    QMAKE_CXXFLAGS += -fstack-protector-strong --param ssp-buffer-size=4
    QMAKE_CXXFLAGS_RELEASE += -O2
    QMAKE_CXXFLAGS_DEBUG += -O0 -g
    QMAKE_LFLAGS_RELEASE += -pie -Wl,-z,relro -Wl,-z,now -Wl,-strip-all
}

*-clang* {
	isEmpty(asan) {
		asan = false
	}

	# enable ASAN
	if($$asan) {
		message( "Use Address Sanitizer" )
    	QMAKE_CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer -O0 -g3
		QMAKE_CFLAGS += -fsanitize=address -fno-omit-frame-pointer
		QMAKE_LFLAGS = -fsanitize=address
    } else {
    	QMAKE_CXXFLAGS += -O0 -g3
    }
}

CONFIG += link_pkgconfig qpa/genericunixfontdatabase c++14

QT += core-private gui-private

equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION, 8): {
	QT += platformsupport-private
} else {
	QT += fontdatabase_support_private eventdispatcher_support_private theme_support_private
}

SOURCES += main.cpp 				\
		qfreerdpcompositor.cpp      \
		qfreerdpclipboard.cpp       \
		qfreerdpplatform.cpp 		\
		qfreerdpscreen.cpp			\
		qfreerdpbackingstore.cpp	\
		qfreerdpwindow.cpp			\
		qfreerdppeer.cpp			\
		qfreerdppeerclipboard.cpp	\
		qfreerdpwindowmanager.cpp	\
		xcursors/xcursor.cpp        \
		xcursors/rdp-cursor.cpp     \
		xcursors/qfreerdpxcursor.cpp


HEADERS += qfreerdpcompositor.h \
	qfreerdpplatform.h \
	qfreerdpclipboard.h \
	qfreerdpscreen.h \
	qfreerdpcursor.h			\
	qfreerdpbackingstore.h \
	qfreerdpwindow.h \
	qfreerdppeer.h \
	qfreerdppeerclipboard.h	\
	qfreerdplistener.h \
	qfreerdpwindowmanager.h \
	xcursors/cursor-data.h \
	xcursors/xcursor.h \
	xcursors/rdp-cursor.h \
	xcursors/qfreerdpxcursor.h
