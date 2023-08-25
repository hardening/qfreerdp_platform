/**
 * Copyright © 2023 Hardening <rdp.effort@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

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
