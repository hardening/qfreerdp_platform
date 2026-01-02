/**
 * Copyright © 2013-2023 David Fort <contact@hardening-consulting.com>
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
#include "qfreerdpwindowmanager.h"

#include <QtCore/QtDebug>
#include <QDebug>

#include <qpa/qwindowsysteminterface.h>
#include "xcursors/qfreerdpxcursor.h"

QT_BEGIN_NAMESPACE

QFreeRdpScreen::QFreeRdpScreen(QFreeRdpPlatform *platform, const QRect &geom, const QRect &rdpGeom)
: mGeometry(geom)
, mRdpGeometry(rdpGeom)
, mPlatform(platform)
{
	QSize sz = geom.size();
	qDebug("QFreeRdpScreen::%s(%d x %d)", __func__, sz.width(), sz.height());
}

QFreeRdpScreen::~QFreeRdpScreen() {
	qDebug() << "QFreeRdpScreen::" << __func__ << "()";
}

QRect QFreeRdpScreen::geometry() const {
    return mGeometry;
}

QRect QFreeRdpScreen::rdpGeometry() const {
	return mRdpGeometry;
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
	return mPlatform->mCursor;
}

void QFreeRdpScreen::setGeometry(const QRect &geometry) {
	qDebug("QFreeRdpScreen::%s(%d,%d, %dx%d)", __func__, geometry.left(), geometry.top(),
			geometry.width(), geometry.height());
	if(geometry == mGeometry)
		return;

	mGeometry = geometry;

    QWindowSystemInterface::handleScreenGeometryChange(screen(), mGeometry, mGeometry);
	resizeMaximizedWindows();

    mPlatform->mWindowManager->pushDirtyArea(mGeometry);
}

void QFreeRdpScreen::setRdpGeometry(const QRect &rdpGeometry)
{
	mRdpGeometry = rdpGeometry;
}

void QFreeRdpScreen::setFullGeometry(const QRect &geometry, const QRect &rdpGeometry)
{
	mRdpGeometry = rdpGeometry;
	setGeometry(geometry);
}

QT_END_NAMESPACE
