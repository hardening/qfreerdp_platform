#include "qfreerdpcursor.h"
#include "qfreerdpscreen.h"

#include <QDebug>


QT_USE_NAMESPACE

class QFreeRdpCursorPrivate {
public :
	QFreeRdpCursorPrivate(QFreeRdpScreen *screen) :
		mScreen(screen){}
public :
    QFreeRdpScreen *mScreen;
};

QFreeRdpCursor::QFreeRdpCursor(QFreeRdpScreen *screen) :
	d(new QFreeRdpCursorPrivate(screen)){}

QFreeRdpCursor::~QFreeRdpCursor()
{
	delete d;
}

void QFreeRdpCursor::changeCursor(QCursor *cursor, QWindow *window)
{
	Q_UNUSED(window)
	const Qt::CursorShape newShape = cursor ? cursor->shape() : Qt::ArrowCursor;
	//qDebug("QFreeRdpCursor::%s(%p, %p) = %d", __func__, (void *)cursor, (void *)window, newShape);

    if (newShape < Qt::BitmapCursor) {
        //waylandCursor = requestCursor((WaylandCursor)newShape);
    } else if (newShape == Qt::BitmapCursor) {
        //TODO: Bitmap cursor logic
    } else {
        //TODO: Custom cursor logic (for resize arrows)
    }
}
