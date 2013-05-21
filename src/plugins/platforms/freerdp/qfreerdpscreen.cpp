/*
 * Copyright © 2013 Hardening <rdp.effort@gmail.com>
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

#include "qfreerdpscreen.h"
#include "qfreerdpplatform.h"
#include "qfreerdpcursor.h"
#include "qfreerdpwindowmanager.h"

#include <QtCore/QtDebug>
#include <QDebug>

#include <qpa/qwindowsysteminterface.h>

QT_BEGIN_NAMESPACE

QFreeRdpScreen::QFreeRdpScreen(QFreeRdpPlatform *platform, int width, int height):
	mPlatform(platform), mCursor(new QFreeRdpCursor(this))
{
	qDebug() << "QFreeRdpScreen::" << __func__ << "()";
    mGeometry = QRect(0, 0, width, height);
    mScreenBits = new QImage(width, height, QImage::Format_ARGB32_Premultiplied);
    mScreenBits->fill(Qt::white);
}

QFreeRdpScreen::~QFreeRdpScreen() {
	qDebug() << "QFreeRdpScreen::" << __func__ << "()";
	delete mScreenBits;
}

QRect QFreeRdpScreen::geometry() const {
    return mGeometry;
}

int QFreeRdpScreen::depth() const {
    return 32;
}

QImage::Format QFreeRdpScreen::format() const {
    return QImage::Format_ARGB32_Premultiplied;
}

qreal QFreeRdpScreen::refreshRate() const {

	return 60000.0 / 1000.f;
}

QPlatformCursor *QFreeRdpScreen::cursor() const {
	return mCursor;
}

void QFreeRdpScreen::setGeometry(int x, int y, int width, int height) {
	QRect newGeometry(x, y, width, height);
	if(newGeometry == mGeometry)
		return;

	qDebug() << "QFreeRdpScreen::" << __func__ << "()";
	mGeometry = newGeometry;
    mScreenBits = new QImage(width, height, QImage::Format_ARGB32_Premultiplied);
    mScreenBits->fill(Qt::white);

    QWindowSystemInterface::handleScreenGeometryChange(screen(), mGeometry);
    mPlatform->mWindowManager->repaint( QRegion(mGeometry) );
}


QT_END_NAMESPACE
