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
#include "xcursors/qfreerdpxcursor.h"

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
, mFps(fps)
, mDraggingType(WmWidget::DRAGGING_NONE)
, mDraggedWindow(nullptr)
{
	connect(&mFrameTimer, &QTimer::timeout, this, &QFreeRdpWindowManager::onGenerateFrame);
}

QFreeRdpWindowManager::~QFreeRdpWindowManager() {
}

void QFreeRdpWindowManager::initialize() {
	mFrameTimer.start((int)(1000 / mFps));
}

static bool isDecorableWindow(QWindow *window) {
	switch(window->type()) {
	case Qt::Dialog:
	case Qt::Window:
		return !(window->flags() & Qt::FramelessWindowHint);
	default:
		return false;
	}
}

void QFreeRdpWindowManager::addWindow(QFreeRdpWindow *window) {
	mWindows.push_front(window);

	QWindow* qwindow = window->window();
	if(qwindow->type() != Qt::Desktop)
		mFocusWindow = window;

	QRegion dirtyRegion = window->outerWindowGeometry();

	// mWinId 1 is our original page, where we don't want any decorations
	// Any other window is fair game.
	if (window->winId() > 1 && isDecorableWindow(qwindow)) {
		qDebug("WM activating windows decorations");
		mDecoratedWindows++;
		dirtyRegion = window->screen()->geometry();
		window->setDecorate(true);
	}

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

	if (window->winId() > 1 && isDecorableWindow(window->window())) {
		qDebug("WM desactivating windows decorations");
		mDecoratedWindows--;
	}

	QRegion dirtyRegion = window->windowFrameGeometry();

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
		if(!window->isVisible() || !window->windowContent())
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
// 		qDebug("%s: window=%llu windowRectLeft=%d windowRectTop=%d windowRectWidth=%d windowRectHeight=%d", __func__, window->winId(),
// 				windowRect.left(), windowRect.top(), windowRect.width(), windowRect.height());
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
	return nullptr;
}

void QFreeRdpWindowManager::setFocusWindow(QFreeRdpWindow *w) {
	if (w != mFocusWindow) {
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
		QWindowSystemInterface::handleWindowActivated(w->window());
#else
		QWindowSystemInterface::handleFocusWindowChanged(w->window());
#endif // QT_VERSION

		raise(w);
	}

	mFocusWindow = w;
}


void QFreeRdpWindowManager::onStartDragging(WmWidget::DraggingType dragType, QFreeRdpWindow *window)
{
	mDraggingType = dragType;
	mDraggedWindow = window;

	switch (dragType) {
	case WmWidget::DRAGGING_NONE:
		return;
	case WmWidget::DRAGGING_MOVE: {
		QCursor newCursor(Qt::SizeAllCursor);
		mPlatform->cursorHandler()->changeCursor(&newCursor, nullptr);
		break;
	}
	default:
		break;;
	}
}

bool QFreeRdpWindowManager::handleWindowMove(const QPoint &pos, Qt::MouseButtons buttons, Qt::MouseButton button, bool down)
{
	if (!(buttons & Qt::LeftButton)) {
		qDebug() << "end of dragging";
		mDraggingType = WmWidget::DRAGGING_NONE;
		mDraggedWindow = nullptr;

		QCursor moveCursor(Qt::ArrowCursor);
		mPlatform->cursorHandler()->changeCursor(&moveCursor, nullptr);
		return true;
	}

	QPoint delta = (pos - mLastMousePos);
	mLastMousePos = pos;

	auto window = mDraggedWindow->window();
	QPoint newPosition = window->position() + delta;

	if (isValidWindowGeometry(window, QRect(newPosition, window->size())))
		window->setPosition(newPosition);
	return true;
}

bool QFreeRdpWindowManager::isValidWindowGeometry(const QWindow *window, const QRect &newGeometry)
{
	QPoint newPosition = newGeometry.topLeft();
	auto screenGeom = window->screen()->geometry();

	if (!newGeometry.isValid())
		return false;

	/* ensure that the window is still accessible */
	QSize newSize = newGeometry.size();
	if (!(newPosition.x() + newSize.width() - 10 > 0 && newPosition.y() > WM_DECORATION_HEIGHT &&
			newPosition.y() < screenGeom.bottom() && newPosition.x() < screenGeom.right() - 5))
		return false;

	QSize minimumSize(
		5 * 2 /* resize anchors */ + 20, /* minimum size for the title / close button / inner window */
		WM_DECORATION_HEIGHT /* top */ + WM_BORDERS_SIZE /* bottom */ + 5 /*content */
	);

	return newSize.width() >= minimumSize.width() && newSize.height() >= minimumSize.height();
}

bool QFreeRdpWindowManager::handleWindowResize(const QPoint &pos, Qt::MouseButtons buttons, Qt::MouseButton button, bool down)
{
	if (!(buttons & Qt::LeftButton)) {
		qDebug() << "end of resizing";
		mDraggingType = WmWidget::DRAGGING_NONE;
		mDraggedWindow = nullptr;

		QCursor moveCursor(Qt::ArrowCursor);
		mPlatform->cursorHandler()->changeCursor(&moveCursor, nullptr);
		return true;
	}

	QPoint delta = (pos - mLastMousePos);
	mLastMousePos = pos;

	auto window = mDraggedWindow->window();
	QRect newGeometry = window->geometry();
	switch (mDraggingType) {
	case WmWidget::DRAGGING_RESIZE_TOP:
		newGeometry.adjust(0, delta.y(), 0, 0);
		break;
	case WmWidget::DRAGGING_RESIZE_TOP_LEFT:
		newGeometry.adjust(delta.x(), delta.y(), 0, 0);
		break;
	case WmWidget::DRAGGING_RESIZE_TOP_RIGHT:
		newGeometry.adjust(0, delta.y(), delta.x(), 0);
		break;
	case WmWidget::DRAGGING_RESIZE_BOTTOM:
		newGeometry.adjust(0, 0, 0, delta.y());
		break;
	case WmWidget::DRAGGING_RESIZE_BOTTOM_LEFT:
		newGeometry.adjust(delta.x(), 0, 0, delta.y());
		break;
	case WmWidget::DRAGGING_RESIZE_BOTTOM_RIGHT:
		newGeometry.adjust(0, 0, delta.x(), delta.y());
		break;
	case WmWidget::DRAGGING_RESIZE_LEFT:
		newGeometry.adjust(delta.x(), 0, 0, 0);
		break;
	case WmWidget::DRAGGING_RESIZE_RIGHT:
		newGeometry.adjust(0, 0, delta.x(), 0);
		break;
	default:
		return true;
	}

	if (isValidWindowGeometry(window, newGeometry))
		window->setGeometry(newGeometry);
	return true;
}

bool QFreeRdpWindowManager::handleMouseEvent(const QPoint &pos, Qt::MouseButtons buttons, Qt::MouseButton button, bool down) {
	switch(mDraggingType) {
	case WmWidget::DRAGGING_MOVE:
		return handleWindowMove(pos, buttons, button, down);
	case WmWidget::DRAGGING_RESIZE_TOP:
	case WmWidget::DRAGGING_RESIZE_TOP_LEFT:
	case WmWidget::DRAGGING_RESIZE_TOP_RIGHT:
	case WmWidget::DRAGGING_RESIZE_BOTTOM:
	case WmWidget::DRAGGING_RESIZE_BOTTOM_LEFT:
	case WmWidget::DRAGGING_RESIZE_BOTTOM_RIGHT:
	case WmWidget::DRAGGING_RESIZE_LEFT:
	case WmWidget::DRAGGING_RESIZE_RIGHT:
		return handleWindowResize(pos, buttons, button, down);
	case WmWidget::DRAGGING_NONE:
		break;
	}

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

	if(button && down && rdpWindow) {
		/* we don't want a background window to do anything if the current window is modal */
		if (!mFocusWindow || !mFocusWindow->window()->isModal())
			setFocusWindow(rdpWindow);
	}

	if (wmEvent) {
		if (targetWmWidget) {
			targetWmWidget->handleMouse(localPos, buttons);
		}

	} else {
		if(window) {
			//qDebug("%s: dest=%d flags=0x%x buttons=0x%x", __func__, window->winId(), flags, peer->mLastButtons);
			QEvent::Type eventType;
			if (button)
				eventType = down ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease;
			else
				eventType = QEvent::MouseMove;
			QWindowSystemInterface::handleMouseEvent(window, localPos, pos, buttons, button, eventType);
		}
	}

	mLastMousePos = pos;
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

