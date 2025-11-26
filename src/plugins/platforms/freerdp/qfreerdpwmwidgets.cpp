/*
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

#include "qfreerdpwmwidgets.h"
#include "qfreerdpplatform.h"
#include "qfreerdpwindow.h"
#include "qfreerdpwindowmanager.h"
#include "xcursors/qfreerdpxcursor.h"

#include <QImage>
#include <QWindow>
#include <QDebug>


WmWindowDecoration::WmWindowDecoration(QFreeRdpWindow *freerdpW, const WmTheme &theme, const IconResource *closeRes, WmWidget *parent)
: WmWidget(theme, parent)
, mWindow(freerdpW)
, mTopContainer(new WmHContainer(theme, this))
, mEnteredWidget(nullptr)
, mDirty(true)
, mContent(nullptr)
, mLastCursor(Qt::ArrowCursor)
{
	mTopSpacer = new WmSpacer(QSize(5, WM_DECORATION_HEIGHT), theme, mTopContainer);
	mTopSpacer2 = new WmSpacer(QSize(5, WM_DECORATION_HEIGHT), theme, mTopContainer);
	mTitle = new WmLabel(freerdpW->window()->title(), theme, mTopContainer);
	mCloseButton = new WmIconButton(closeRes->normalIcon, closeRes->overIcon, theme, mTopContainer);


	/* create the top border :
	 * [  5 px  ][<--     expanding centered   -->][ close  ][  5 px  ]
	 * [ spacer ][<--        title             -->][ button ][ spacer ]
	 */
	mTopContainer->push(mTopSpacer);
	mTopContainer->push(mTitle, WMWIDGET_SIZEPOLICY_EXPAND);
	mTopContainer->push(mCloseButton);
	mTopContainer->push(mTopSpacer2);

	QWindow *w = mWindow->window();
	resizeFromWindow(w);

	connect(mCloseButton, &WmIconButton::clicked, this, &WmWindowDecoration::onCloseClicked);

	QFreeRdpWindowManager *wm = freerdpW->windowManager();
	connect(this, &WmWindowDecoration::startDrag, wm, &QFreeRdpWindowManager::onStartDragging);
}

WmWindowDecoration::~WmWindowDecoration() {
	QFreeRdpWindowManager *wm = mWindow->windowManager();
	disconnect(this, &WmWindowDecoration::startDrag, wm, &QFreeRdpWindowManager::onStartDragging);

	disconnect(mCloseButton, &WmIconButton::clicked, this, &WmWindowDecoration::onCloseClicked);
	delete mContent;
	delete mTopContainer;
}

QRegion WmWindowDecoration::geometryRegion() const {
	return mGeometryRegion;
}

void WmWindowDecoration::setTitle(const QString &title) {
	mTitle->setTitle(title);
}

void WmWindowDecoration::resizeFromWindow(const QWindow *w) {
	QPoint wPos = w->position();
	mPos = QPoint(wPos.x() - WM_BORDERS_SIZE, wPos.y() - WM_DECORATION_HEIGHT);
	mSize = QSize(w->width() + WM_BORDERS_SIZE*2, w->height() + WM_DECORATION_HEIGHT + WM_BORDERS_SIZE);

	mGeometryRegion = mWindow->outerWindowGeometry();
	mGeometryRegion -= w->geometry();

	mResizeRegions.clear();

	/* Define corner regions first to give them priority */

	/* top-left */
	QRegion r = QRegion(QRect(0, 0, WM_CORNER_GRAB_SIZE, WM_CORNER_GRAB_SIZE));
	ResizeAction a;
	a.r = r;
	a.action = DRAGGING_RESIZE_TOP_LEFT;
	a.cursor = Qt::SizeFDiagCursor;
	mResizeRegions.push_back(a);

	/* top-right */
	r = QRegion(QRect(mSize.width()-WM_CORNER_GRAB_SIZE, 0, WM_CORNER_GRAB_SIZE, WM_CORNER_GRAB_SIZE));
	a.r = r;
	a.action = DRAGGING_RESIZE_TOP_RIGHT;
	a.cursor = Qt::SizeBDiagCursor;
	mResizeRegions.push_back(a);

	/* bottom-left */
	r = QRegion( QRect(0, mSize.height() - WM_BORDERS_SIZE, WM_CORNER_GRAB_SIZE, WM_BORDERS_SIZE) );
	r += QRect(0, mSize.height() - WM_CORNER_GRAB_SIZE, WM_BORDERS_SIZE, WM_CORNER_GRAB_SIZE);
	a.r = r;
	a.action = DRAGGING_RESIZE_BOTTOM_LEFT;
	a.cursor = Qt::SizeBDiagCursor;
	mResizeRegions.push_back(a);

	/* bottom-right */
	r = QRegion( QRect(mSize.width() - WM_BORDERS_SIZE, mSize.height() - WM_CORNER_GRAB_SIZE,
					WM_BORDERS_SIZE, WM_CORNER_GRAB_SIZE) );
	r += QRect(mSize.width() - WM_CORNER_GRAB_SIZE, mSize.height() - WM_BORDERS_SIZE,
			WM_CORNER_GRAB_SIZE, WM_BORDERS_SIZE);
	a.r = r;
	a.action = DRAGGING_RESIZE_BOTTOM_RIGHT;
	a.cursor = Qt::SizeFDiagCursor;
	mResizeRegions.push_back(a);

	/* Then side resize handles */

	/* top */
	r = QRegion( QRect(WM_CORNER_GRAB_SIZE, 0, mSize.width() - 2 * WM_CORNER_GRAB_SIZE, 2) );
	a.r = r;
	a.action = DRAGGING_RESIZE_TOP;
	a.cursor = Qt::SizeVerCursor;
	mResizeRegions.push_back(a);

	/* bottom */
	r = QRegion( QRect(WM_CORNER_GRAB_SIZE, mSize.height() - WM_BORDERS_SIZE,
					mSize.width() - 2 * WM_CORNER_GRAB_SIZE, WM_BORDERS_SIZE) );
	a.r = r;
	a.action = DRAGGING_RESIZE_BOTTOM;
	a.cursor = Qt::SizeVerCursor;
	mResizeRegions.push_back(a);

	/* left */
	r = QRegion( QRect(0, WM_CORNER_GRAB_SIZE, WM_BORDERS_SIZE,
					mSize.height() - WM_CORNER_GRAB_SIZE) );
	a.r = r;
	a.action = DRAGGING_RESIZE_LEFT;
	a.cursor = Qt::SizeHorCursor;
	mResizeRegions.push_back(a);

	/* right */
	r = QRegion( QRect(mSize.width() - WM_BORDERS_SIZE, WM_CORNER_GRAB_SIZE,
					WM_BORDERS_SIZE, mSize.height() - WM_CORNER_GRAB_SIZE) );
	a.r = r;
	a.action = DRAGGING_RESIZE_RIGHT;
	a.cursor = Qt::SizeHorCursor;
	mResizeRegions.push_back(a);

	/* And finally the hotspot to grab the window for moving, spanning the
	 * title bar minus borders */

	/* move from top bar */
	r = QRegion(QRect(WM_BORDERS_SIZE, WM_BORDERS_SIZE,
				   mSize.width() - 2 * WM_BORDERS_SIZE, WM_DECORATION_HEIGHT - WM_BORDERS_SIZE));
	a.r = r;
	a.action = DRAGGING_MOVE;
	a.cursor = Qt::SizeAllCursor;
	mResizeRegions.push_back(a);

	handleResize();
}


void WmWindowDecoration::handleResize() {
	delete mContent;
	mContent = new QImage(mSize, QImage::Format_ARGB32);
	mContent->fill(mTheme.backColor);
	mTopContainer->setSize(QSize(mSize.width(), WM_DECORATION_HEIGHT));
}

void WmWindowDecoration::handleMouse(const QPoint &pos, Qt::MouseButtons buttons) {
	Qt::CursorShape newCursor = Qt::ArrowCursor;

	for(auto resizeAction: mResizeRegions)
	{
		if (resizeAction.r.contains(pos)) {
			if (buttons & Qt::LeftButton)
				emit startDrag(resizeAction.action, mWindow);

			newCursor = resizeAction.cursor;
			break;
		}
	}

	if (mLastCursor != newCursor) {
		QFreeRdpCursor *cursorHandler = mWindow->mPlatform->cursorHandler();
		QCursor c(newCursor);

		cursorHandler->changeCursor(&c, mWindow->window());
		mLastCursor = newCursor;
	}

	if (mTopContainer->localGeometry().contains(pos)) {
		if (mEnteredWidget != mTopContainer) {
			mEnteredWidget = mTopContainer;
			mTopContainer->handleEnter(pos);
		}
		mTopContainer->handleMouse(pos, buttons);
	} else {
		if (mEnteredWidget) {
			mEnteredWidget->handleLeave();
			mEnteredWidget = nullptr;
		}
	}
}

void WmWindowDecoration::handleLeave() {
	if (mEnteredWidget) {
		mEnteredWidget->handleLeave();
		mEnteredWidget = nullptr;
	}

	if (mLastCursor != Qt::ArrowCursor) {
		QFreeRdpCursor *cursorHandler = mWindow->mPlatform->cursorHandler();
		QCursor c(Qt::ArrowCursor);

		cursorHandler->changeCursor(&c, mWindow->window());
		mLastCursor = Qt::ArrowCursor;
	}

}

void WmWindowDecoration::repaint(QPainter &painter, const QRegion &dirtyRegion) {
	if (mDirty) {
		QPainter localPainter(mContent);
		mTopContainer->repaint(localPainter, QPoint(0, 0));
		mDirty = false;

#if 0
		static int ndeco = 0;
		QString fname = QString("/tmp/frame-%1.png").arg(ndeco++);
		mContent->save(fname, "PNG", 100);
#endif
	}

	for (auto const rect: dirtyRegion.translated(-mPos)) {
		painter.drawImage(mPos + rect.topLeft(), *mContent, rect);
	}
}

void WmWindowDecoration::repaint(QPainter &painter, const QPoint &pos) {

	mTopContainer->repaint(painter, pos);
}

void WmWindowDecoration::handleChildDirty(WmWidget* child, const QRegion &dirty) {
	mDirty = true;
	mWindow->notifyDirty( dirty.translated(child->position()) );
}

void WmWindowDecoration::onCloseClicked() {
	emit mWindow->close();
}

