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

#include "qfreerdpplatform.h"
#include "qfreerdpwindowmanager.h"
#include "qfreerdpscreen.h"
#include "qfreerdpwindow.h"
#include "qfreerdpwmwidgets.h"

#include <QtGui/qpa/qwindowsysteminterface.h>
#include <QRegion>
#include <QPainter>
#include <QDebug>

#include <assert.h>

QT_BEGIN_NAMESPACE


QFreeRdpWindowManager::QFreeRdpWindowManager(QFreeRdpPlatform *platform, int fps)
: mPlatform(platform)
, mFocusWindow(0)
, mEnteredWindow(0)
, mEnteredWidget(0)
, mDecoratedWindows(0)
, mDoDecorate(false)
, mFps(fps)
{
	connect(&mFrameTimer, SIGNAL(timeout()), this, SLOT(onGenerateFrame()));
}

void QFreeRdpWindowManager::initialize() {
	mFrameTimer.start((int)(1000 / mFps));
}

static bool isDecorableWindow(QWindow *window) {
	switch(window->type()) {
	case Qt::Dialog:
	case Qt::Window:
		return true;
	default:
		return false;
	}
}

void QFreeRdpWindowManager::addWindow(QFreeRdpWindow *window) {
	mWindows.push_front(window);

	QWindow* qwindow = window->window();
	if(qwindow->type() != Qt::Desktop)
		mFocusWindow = window;

	if (isDecorableWindow(qwindow))
		mDecoratedWindows++;

	QRegion dirtyRegion = window->outerWindowGeometry();
	bool decorate = (mDecoratedWindows > 1);
	if (decorate != mDoDecorate) {
		qDebug("activating windows decorations");
		dirtyRegion = window->screen()->geometry();
	}

	mDoDecorate = decorate;
	mDirtyRegion += dirtyRegion;
}

void QFreeRdpWindowManager::dropWindow(QFreeRdpWindow *window) {
	if (mFocusWindow == window)
		mFocusWindow = nullptr;
	if (mEnteredWindow == window->window())
		mEnteredWindow = nullptr;

	if (!mWindows.removeAll(window))
		return;

	auto deco = window->decorations();
	if (deco == mEnteredWidget)
		mEnteredWidget = nullptr;

	if (isDecorableWindow(window->window()))
		mDecoratedWindows--;

	QRegion dirtyRegion = window->windowFrameGeometry();
	bool decorate = (mDecoratedWindows > 1);
	if (decorate != mDoDecorate) {
		qDebug("desactivating windows decorations");
		dirtyRegion = window->screen()->geometry();
	}
	mDoDecorate = decorate;

	mDirtyRegion += dirtyRegion;
}

void QFreeRdpWindowManager::raise(QFreeRdpWindow *window) {
	if(!mWindows.removeOne(window))
		return;

	mWindows.push_front(window);
	if(window->isExposed()) {
		mDirtyRegion += window->outerWindowGeometry();
	}
}

void QFreeRdpWindowManager::lower(QFreeRdpWindow *window) {
	if(!mWindows.removeOne(window))
		return;

	mWindows.push_back(window);
	if(window->isExposed())
		mDirtyRegion += window->outerWindowGeometry();
}

void qimage_bitblt(const QRect &srcRect, const QImage *srcImg, const QPoint &dst, QImage *destImg) {
	QPainter painter(destImg);
	painter.drawImage(dst, *srcImg, srcRect);
}

void qimage_fillrect(const QRect &rect, QImage *dest, quint32 color) {
	// check dimensions
	QPoint end = rect.bottomRight();
	if (dest->height() < end.y() || dest->width() < end.x()) {
		qCritical() << "qfreerdp: cannot fill " << rect << " into " << dest->width() << "x" << dest->height() << "image";
		return;
	}

	for(int h = rect.top(); h <= rect.bottom(); h++) {
		quint32* begin = ((quint32*)dest->scanLine(h)) + rect.left();
		std::fill(begin, begin+rect.width(), color);
	}
}

void QFreeRdpWindowManager::pushDirtyArea(const QRegion &region) {
	mDirtyRegion += region;
}

void QFreeRdpWindowManager::repaint(const QRegion &region) {
	QFreeRdpScreen *screen = mPlatform->getScreen();
	QImage *dest = screen->getScreenBits();
	QRect screenGeometry = screen->geometry();

	QRegion toRepaint = region.intersected(screenGeometry); // clip to screen size
	if(toRepaint.isEmpty())
		return;

	QRegion dirtyRegion = toRepaint;

	//qDebug() << "dirtyRegion=" << dirtyRegion;

	foreach(QFreeRdpWindow *window, mWindows) {
		if(!window->isVisible())
			continue;

		QRect windowRect = window->geometry();

		/* first draw the decorations if any */
		WmWindowDecoration *decorations = window->decorations();
		if (decorations) {
			QRegion decoRegion = window->decorationGeometry();
			QRegion decoRepaint = toRepaint.intersected(decoRegion);

			toRepaint -= decoRepaint;

			if (!decoRepaint.isEmpty()) {
				//qDebug() << "decoRepaint region=" << decoRepaint;

				QPainter painter(dest);
				decorations->repaint(painter, decoRepaint);
			}
		}

		/*  then draw the window content itself	 */
//		qDebug("%s: window=%llu windowRectLeft=%d windowRectTop=%d windowRectWidth=%d windowRectHeight=%d", __func__, window->winId(),
//				windowRect.left(), windowRect.top(), windowRect.width(), windowRect.height());
		QRegion inter = toRepaint.intersected(windowRect);
		for (const QRect& repaintRect : inter) {
			QPoint topLeft = windowRect.topLeft();
			QRect localCoord = repaintRect.translated(-topLeft);

			assert(window->windowContent() != NULL);
			assert(dest != NULL);
			qimage_bitblt(localCoord, window->windowContent(), repaintRect.topLeft(), dest);
		}

		toRepaint -= inter;
	}

	for (const QRect& repaintRect: toRepaint) {
		qimage_fillrect(repaintRect, dest, 0);
	}

	mPlatform->repaint(dirtyRegion);
}

void QFreeRdpWindowManager::onGenerateFrame() {
	if (!mDirtyRegion.isEmpty()) {
		repaint(mDirtyRegion);
		mDirtyRegion = QRegion();
	}
}

QFreeRdpWindow *QFreeRdpWindowManager::getWindowAt(const QPoint pos) const {
	foreach(QFreeRdpWindow *window, mWindows) {
		if(!window->isVisible())
			continue;

		if(window->outerWindowGeometry().contains(pos))
			return window;
	}
	return 0;
}

void QFreeRdpWindowManager::setFocusWindow(QFreeRdpWindow *w) {
	if (w != mFocusWindow)
		QWindowSystemInterface::handleWindowActivated(w->window());

	mFocusWindow = w;
}

bool QFreeRdpWindowManager::handleMouseEvent(const QPoint &pos, Qt::MouseButtons buttons) {
	QFreeRdpWindow *rdpWindow = getWindowAt(pos);
	QWindow *window = nullptr;
	WmWidget *targetWmWidget = nullptr;
	QPoint localPos;

	bool wmEvent = false;

	if (rdpWindow) {
		window = rdpWindow->window();
		wmEvent = !window->geometry().contains(pos);
		if (wmEvent) {
			targetWmWidget = rdpWindow->decorations();
			localPos = (pos - rdpWindow->outerWindowGeometry().topLeft());
		} else {
			localPos = (pos - window->geometry().topLeft());
			targetWmWidget = nullptr;
		}
	}

	if (window != mEnteredWindow) {
		if (mEnteredWindow)
			QWindowSystemInterface::handleLeaveEvent(mEnteredWindow);
	}

	if (targetWmWidget != mEnteredWidget) {
		if (mEnteredWidget)
			mEnteredWidget->handleLeave();
	}

	if (window != mEnteredWindow) {
		if (window)
			QWindowSystemInterface::handleEnterEvent(window, localPos, pos);

		mEnteredWindow = window;
	}

	if (targetWmWidget != mEnteredWidget) {
		if (targetWmWidget)
			targetWmWidget->handleEnter(pos - targetWmWidget->localPosition());

		mEnteredWidget = targetWmWidget;
	}

	if(buttons && rdpWindow)
		setFocusWindow(rdpWindow);

	if (wmEvent) {
		if (targetWmWidget) {
			targetWmWidget->handleMouse(localPos, buttons);
			/*if (targetWmWidget->isDirty()) {
				rdpWindow->repaintDecorations();
				pushDirtyArea(rdpWindow->outerWindowGeometry());
			}*/
		}

	} else {
		if(window) {
			//qDebug("%s: dest=%d flags=0x%x buttons=0x%x", __func__, window->winId(), flags, peer->mLastButtons);
			QWindowSystemInterface::handleMouseEvent(window, localPos, pos, buttons);
		}
	}

	return true;
}

bool QFreeRdpWindowManager::handleWheelEvent(const QPoint &pos, int wheelDelta)
{
	QFreeRdpWindow *rdpWindow = getWindowAt(pos);
	QWindow *window = nullptr;

	bool wmEvent = false;

	if (rdpWindow) {
		window = rdpWindow->window();
		wmEvent = !window->geometry().contains(pos);
	}

	if (wmEvent) {
		//qDebug("WM wheel event");

	} else {
		QPoint localCoord = (pos - window->geometry().topLeft());
		QPoint angleDelta;
		angleDelta.setY(wheelDelta);
		QWindowSystemInterface::handleWheelEvent(window, localCoord, pos, QPoint(), angleDelta);
	}

	return true;
}

QT_END_NAMESPACE

