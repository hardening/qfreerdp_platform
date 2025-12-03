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
#include <QMargins>
#include <QRegion>
#include <QPainter>
#include <QDebug>

#include <assert.h>

QT_BEGIN_NAMESPACE


// Returns std::nullopt if the new geometry is invalid, otherwize return a
// QPoint representing a 2D vector offset to be used to correct the provided
// `newGeometry` (handling of this offset will be different depending on the
// type of initiating event)
static std::optional<QPoint> validateWindowGeometry(const QRect &screenGeometry, const QRect &newGeometry)
{
	QMargins antiLossMargins(5, 0 /* top, unused */, 10, 0 /* bottom, unused */);
	// QRect's bottom() and right() unfortunately give off by one results
	// https://doc.qt.io/qt-6/qrect.html#bottom
	int screenBottom = screenGeometry.top() + screenGeometry.height();
	int screenRight = screenGeometry.left() + screenGeometry.width();
	int geomRight = newGeometry.left() + newGeometry.width();
	QPoint offset(0, 0);

	if (!newGeometry.isValid())
		return std::nullopt;

	/* ensure that the user cannot lose their window out of the viewport */
	if (geomRight < antiLossMargins.right())
		offset.setX(antiLossMargins.right() - geomRight);
	else if (newGeometry.left() > screenRight - antiLossMargins.left())
		offset.setX(screenRight - antiLossMargins.left() - newGeometry.left());

	if (newGeometry.top() < 0)
		offset.setY(-newGeometry.top());
	else if (newGeometry.top() > screenBottom - WM_DECORATION_HEIGHT)
		offset.setY(screenBottom - WM_DECORATION_HEIGHT - newGeometry.top());

	return offset;
}


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

	// rootWindow(mWinId==1) is our original page, where we don't want any decorations
	// Any other window is fair game.
	if (window->winId() != mPlatform->config()->rootWindow && isDecorableWindow(qwindow)) {
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

		if (w->winId() != mPlatform->config()->rootWindow && !(w->window()->flags() & Qt::WindowStaysOnBottomHint))
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

	QPoint delta = (pos - mLastValidMousePos);

	auto window = mDraggedWindow->window();
	QPoint newPosition = window->position() + delta;
	QRect newGeometry = QRect(newPosition, window->size()) + mDraggedWindow->frameMargins();

	if (auto offset = validateWindowGeometry(window->screen()->geometry(), newGeometry)) {
		window->setPosition(newPosition + offset.value());
		mLastValidMousePos = pos;
	}
	return true;
}

bool QFreeRdpWindowManager::handleWindowResize(const QPoint &mousePos, Qt::MouseButtons buttons, Qt::MouseButton button, bool down)
{
	if (!(buttons & Qt::LeftButton)) {
		qDebug() << "end of resizing";
		mDraggingType = WmWidget::DRAGGING_NONE;
		mDraggedWindow = nullptr;

		QCursor moveCursor(Qt::ArrowCursor);
		mPlatform->cursorHandler()->changeCursor(&moveCursor, nullptr);
		return true;
	}

	auto window = mDraggedWindow->window();
	QMargins margins = mDraggedWindow->frameMargins();
	QRect geometry = window->geometry() + margins;
	QPoint pos = mousePos;
	QSize minimumSize(
		WM_CORNER_GRAB_SIZE * 2 /* resize anchors */ + 20, /* minimum size for the title / close button / inner window */
		WM_DECORATION_HEIGHT /* top */ + WM_BORDERS_SIZE /* bottom */ + 5 /*content */
	);

	switch (mDraggingType) {
	case WmWidget::DRAGGING_RESIZE_TOP:
		pos.setX(geometry.x());
		break;
	case WmWidget::DRAGGING_RESIZE_BOTTOM:
		pos.setX(geometry.left() + geometry.width());
		break;
	case WmWidget::DRAGGING_RESIZE_LEFT:
		pos.setY(geometry.y());
		break;
	case WmWidget::DRAGGING_RESIZE_RIGHT:
		pos.setY(geometry.top() + geometry.height());
		break;
	default:
		break;
	}

	switch (mDraggingType) {
	case WmWidget::DRAGGING_RESIZE_TOP:
	case WmWidget::DRAGGING_RESIZE_TOP_LEFT:
	case WmWidget::DRAGGING_RESIZE_LEFT:
		geometry.setTopLeft(pos);
		if (geometry.width() < minimumSize.width())
			geometry.adjust(geometry.width() - minimumSize.width(), 0, 0, 0);
		if (geometry.height() < minimumSize.height())
			geometry.adjust(0, geometry.height() - minimumSize.height(), 0, 0);
		if (auto offset = validateWindowGeometry(window->screen()->geometry(), geometry)) {
			geometry.adjust(offset.value().x(), offset.value().y(), 0, 0);
			window->setGeometry(geometry - margins);
		}
		break;
	case WmWidget::DRAGGING_RESIZE_BOTTOM:
	case WmWidget::DRAGGING_RESIZE_BOTTOM_RIGHT:
	case WmWidget::DRAGGING_RESIZE_RIGHT:
		geometry.setBottomRight(pos);
		if (geometry.width() < minimumSize.width())
			geometry.adjust(0, 0, minimumSize.width() - geometry.width(), 0);
		if (geometry.height() < minimumSize.height())
			geometry.adjust(0, 0, 0, minimumSize.height() - geometry.height());
		if (auto offset = validateWindowGeometry(window->screen()->geometry(), geometry)) {
			geometry.adjust(0, 0, offset.value().x(), offset.value().y());
			window->setGeometry(geometry - margins);
		}
		break;
	case WmWidget::DRAGGING_RESIZE_TOP_RIGHT:
		geometry.setTopRight(pos);
		if (geometry.width() < minimumSize.width())
			geometry.adjust(0, 0, minimumSize.width() - geometry.width(), 0);
		if (geometry.height() < minimumSize.height())
			geometry.adjust(0, geometry.height() - minimumSize.height(), 0, 0);
		if (auto offset = validateWindowGeometry(window->screen()->geometry(), geometry)) {
			geometry.adjust(0, offset.value().y(), offset.value().x(), 0);
			window->setGeometry(geometry - margins);
		}
		break;
	case WmWidget::DRAGGING_RESIZE_BOTTOM_LEFT:
		geometry.setBottomLeft(pos);
		if (geometry.width() < minimumSize.width())
			geometry.adjust(geometry.width() - minimumSize.width(), 0, 0, 0);
		if (geometry.height() < minimumSize.height())
			geometry.adjust(0, 0, 0, minimumSize.height() - geometry.height());
		if (auto offset = validateWindowGeometry(window->screen()->geometry(), geometry)) {
			geometry.adjust(offset.value().x(), 0, 0, offset.value().y());
			window->setGeometry(geometry - margins);
		}
		break;
	default:
		return true;
	}
	mLastValidMousePos = mousePos;

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

	mLastValidMousePos = pos;
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


#ifdef BUILD_TESTS
#include "tests/qfreerdptestharness.h"

#include <QTest>

void QFreeRdpTest::windowManagerTestValidateGeometry_data() {
	QTest::addColumn<QRect>("inputGeometry");
	QTest::addColumn<std::optional<QPoint>>("offsetResult");

	// Assuming a 800x600 screen geometry

	QTest::newRow("completely valid window")
		<< QRect(50, 20, 200, 100)
		<< std::optional{QPoint(0, 0)};

	QTest::newRow("invalid window geometry")
		<< QRect(QPoint(100, 100), QPoint(50, 50))
		<< std::optional<QPoint>{std::nullopt};

	QTest::newRow("window disappearing to the left")
		<< QRect(-100, 10, 90, 50)
		<< std::optional{QPoint(20, 0)};

	QTest::newRow("window disappearing to the right")
		<< QRect(810, 10, 90, 50)
		<< std::optional{QPoint(-15, 0)};

	QTest::newRow("window bumping the top of the screen")
		<< QRect(50, -5, 90, 50)
		<< std::optional{QPoint(0, 5)};

	QTest::newRow("window disappearing to the bottom")
		<< QRect(50, 610, 90, 50)
		<< std::optional{QPoint(0, -WM_DECORATION_HEIGHT - 10)};

	QTest::newRow("window disappeared to the top-left")
		<< QRect(-50, -50, 40, 40)
		<< std::optional{QPoint(20, 50)};

	QTest::newRow("window disappeared to the top-right")
		<< QRect(850, -50, 40, 40)
		<< std::optional{QPoint(-55, 50)};

	QTest::newRow("window disappeared to the bottom-left")
		<< QRect(-50, 650, 40, 40)
		<< std::optional{QPoint(20, -50 - WM_DECORATION_HEIGHT)};

	QTest::newRow("window disappeared to the bottom-right")
		<< QRect(850, 650, 40, 40)
		<< std::optional{QPoint(-55, -50 - WM_DECORATION_HEIGHT)};
}

void QFreeRdpTest::windowManagerTestValidateGeometry() {
	QRect screenGeometry(0, 0, 800, 600);
	QFETCH(QRect, inputGeometry);
	QFETCH(std::optional<QPoint>, offsetResult);

	QCOMPARE(validateWindowGeometry(screenGeometry, inputGeometry), offsetResult);
}
#endif // BUILD_TESTS

QT_END_NAMESPACE

