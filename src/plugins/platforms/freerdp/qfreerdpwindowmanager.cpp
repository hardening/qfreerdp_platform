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

#include "qfreerdpplatform.h"
#include "qfreerdpwindowmanager.h"
#include "qfreerdpscreen.h"
#include "qfreerdpwindow.h"

#include <QtGui/qpa/qwindowsysteminterface.h>
#include <QRegion>
#include <QPainter>
#include <QDebug>

#include <assert.h>

QT_BEGIN_NAMESPACE

QFreeRdpWindowManager::QFreeRdpWindowManager(QFreeRdpPlatform *platform) :
	mPlatform(platform),
	mActiveWindow(0),
	mEnteredWindow(0)
{
}

void QFreeRdpWindowManager::addWindow(QFreeRdpWindow *window) {
	mWindows.push_front(window);
	repaint(window->geometry());
	if(window->window()->type() != Qt::Desktop)
		mActiveWindow = window;
}

void QFreeRdpWindowManager::dropWindow(QFreeRdpWindow *window) {
	if(mActiveWindow == window)
		mActiveWindow = 0;
	if(!mWindows.removeAll(window))
		return;
	repaint(window->geometry());
}

void QFreeRdpWindowManager::raise(QFreeRdpWindow *window) {
	if(!mWindows.removeOne(window))
		return;

	mWindows.push_front(window);
	if(window->isExposed())
		repaint(window->geometry());
}

void QFreeRdpWindowManager::lower(QFreeRdpWindow *window) {
	if(!mWindows.removeOne(window))
		return;

	mWindows.push_back(window);
	if(window->isExposed())
		repaint(window->geometry());
}

void qimage_bitblt(const QRect &srcRect, const QImage *srcImg, const QPoint &dst, QImage *destImg) {

	if (srcImg->sizeInBytes() == 0) {
		// no bytes in source image
		return;
	}

	int srcStride = srcImg->bytesPerLine();
	QPoint topLeft = srcRect.topLeft();
	const uchar *srcPtr = srcImg->bits() + (topLeft.y() * srcStride) + (topLeft.x() * 4);

	// qDebug("%s: srcStride=%d srcRectLeft=%d srcRectTop=%d srcRectWidth=%d srcRectHeight=%d", __func__, srcStride,
	// 		srcRect.left(), srcRect.top(), srcRect.width(), srcRect.height());
	// qDebug("%s: src image bits = %d", __func__, srcImg->byteCount());

	int dstStride = destImg->bytesPerLine();
	uchar *destPtr = destImg->bits() + (dst.y() * dstStride) + (dst.x() * 4);

	// qDebug("%s: dstStride=%d dstX=%d dstY=%d", __func__, dstStride, dst.x(), dst.y());
	// qDebug("%s: dst image bits = %d", __func__, destImg->byteCount());

	for(int h = 0; h < srcRect.height(); h++) {
		memcpy(destPtr, srcPtr, srcRect.width() * 4);
		srcPtr += srcStride;
		destPtr += dstStride;
	}
}

void qimage_fillrect(const QRect &rect, QImage *dest, quint32 color) {
	// check dimensions
	QPoint end = rect.bottomRight();
	if (dest->height() < end.y() || dest->width() < end.x()) {
		qCritical() << "qfreerdp: cannot fill " << rect << " into " << dest->width() << "x" << dest->height() << "image";
		return;
	}

	for(int h = rect.top(); h < rect.bottom(); h++) {
		quint32* begin = ((quint32*)dest->scanLine(h)) + rect.left();
		std::fill(begin, begin+rect.width(), color);
	}
}


void QFreeRdpWindowManager::repaint(const QRegion &region) {
	QFreeRdpScreen *screen = mPlatform->getScreen();
	QImage *dest = screen->getScreenBits();
	QRect screenGeometry = screen->geometry();

	QRegion toRepaint = region.intersected(screenGeometry); // clip to screen size
	if(toRepaint.isEmpty()) {
		return;
	}

	foreach(QFreeRdpWindow *window, mWindows) {

//		qDebug("%s: window=%llu isVisible=%d", __func__, window->winId(), window->isVisible());

		if(!window->isVisible())
			continue;

		const QRect &windowRect = window->geometry();

//		qDebug("%s: window=%llu windowRectLeft=%d windowRectTop=%d windowRectWidth=%d windowRectHeight=%d", __func__, window->winId(),
//				windowRect.left(), windowRect.top(), windowRect.width(), windowRect.height());

		QRegion inter = toRepaint.intersected(windowRect);
		for (const QRect& repaintRect : inter) {
			QPoint topLeft = windowRect.topLeft();
			QRect localCoord = repaintRect.translated(-topLeft);


			assert(window->getContent() != NULL);
			assert(dest != NULL);
			qimage_bitblt(localCoord, window->getContent(), repaintRect.topLeft(), dest);
		}

		toRepaint -= inter;
	}

	for (const QRect& repaintRect: toRepaint) {
		qimage_fillrect(repaintRect, dest, 0);
	}

	mPlatform->repaint( region.intersected(screenGeometry) );
}

QWindow *QFreeRdpWindowManager::getWindowAt(const QPoint pos) const {
	foreach(QFreeRdpWindow *window, mWindows) {
		if(!window->isVisible())
			continue;

		if(window->geometry().contains(pos))
			return window->window();
	}
	return 0;
}

void QFreeRdpWindowManager::handleMouseEvent(const QPoint &pos, Qt::MouseButtons buttons, Qt::MouseButton button, QEvent::Type eventtype) {
	QWindow *window = getWindowAt(pos);
	if(window != mEnteredWindow) {
		if(mEnteredWindow)
			QWindowSystemInterface::handleLeaveEvent(mEnteredWindow);
	}

	if(window) {
		//qDebug("%s: dest=%d flags=0x%x buttons=0x%x", __func__, window->winId(), flags, peer->mLastButtons);
		Qt::KeyboardModifiers modifiers = Qt::NoModifier;
		QPoint wTopLeft = window->geometry().topLeft();
		QPoint localCoord = pos - wTopLeft;
		if(window != mEnteredWindow)
			QWindowSystemInterface::handleEnterEvent(mEnteredWindow, localCoord, pos);
	    QWindowSystemInterface::handleMouseEvent(window, localCoord, pos, buttons, button, eventtype, modifiers);
	}

	if(window != mEnteredWindow)
		mEnteredWindow = window;
}

QT_END_NAMESPACE

