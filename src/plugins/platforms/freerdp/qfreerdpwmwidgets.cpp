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

#include <QImage>
#include <QWindow>
#include <QDebug>


#define DECORATION_HEIGHT 30
#define BORDERS_SIZE 2

WmWindowDecoration::WmWindowDecoration(QFreeRdpWindow *freerdpW, const WmTheme &theme, const IconResource *closeRes, WmWidget *parent)
: WmWidget(theme, parent)
, mWindow(freerdpW)
, mTopContainer(new WmHContainer(theme, this))
, mEnteredWidget(nullptr)
, mDirty(true)
, mContent(nullptr)
{
	mTopSpacer = new WmSpacer(QSize(5, DECORATION_HEIGHT), theme, mTopContainer);
	mTopSpacer2 = new WmSpacer(QSize(5, DECORATION_HEIGHT), theme, mTopContainer);
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

	connect(mCloseButton, SIGNAL(clicked()), this, SLOT(onCloseClicked()));
	connect(mTitle, SIGNAL(startDrag(WmWidget::DraggingType )),
			this, SLOT(onStartDragging(WmWidget::DraggingType))
	);

	QFreeRdpWindowManager *wm = freerdpW->windowManager();
	connect(this, SIGNAL(startDrag(WmWidget::DraggingType, QFreeRdpWindow *)),
			(QObject*)wm, SLOT(onStartDragging(WmWidget::DraggingType, QFreeRdpWindow *))
	);
}

WmWindowDecoration::~WmWindowDecoration() {
	disconnect(mCloseButton, SIGNAL(clicked()), this, SLOT(onCloseClicked()));
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
	mPos = QPoint(wPos.x() - BORDERS_SIZE, wPos.y() - DECORATION_HEIGHT);
	mSize = QSize(w->width() + BORDERS_SIZE*2, w->height() + DECORATION_HEIGHT + BORDERS_SIZE);

	mGeometryRegion = mWindow->outerWindowGeometry();
	mGeometryRegion -= w->geometry();

	mResizeRegions.clear();

	/* top */
	QRegion r( QRect(5, 0, mSize.width() - 5, 2) );
	ResizeAction a;
	a.r = r;
	a.action = DRAGGING_RESIZE_TOP;
	mResizeRegions.push_back(a);

	/* top-left */
	r = QRegion(QRect(0, 0, 5, BORDERS_SIZE));
	r += QRect(0, 0, BORDERS_SIZE, 5);
	a.r = r;
	a.action = DRAGGING_RESIZE_TOP_LEFT;
	mResizeRegions.push_back(a);

	/* top-right */
	r = QRegion(QRect(mSize.width()-5, 0, 5, BORDERS_SIZE));
	r += QRect(mSize.width() - BORDERS_SIZE, 0, BORDERS_SIZE, 5);
	a.r = r;
	a.action = DRAGGING_RESIZE_TOP_RIGHT;
	mResizeRegions.push_back(a);

	/* bottom */
	r = QRegion( QRect(5, mSize.height() - BORDERS_SIZE, mSize.width() - 5, BORDERS_SIZE) );
	a.r = r;
	a.action = DRAGGING_RESIZE_BOTTOM;
	mResizeRegions.push_back(a);

	/* bottom-left */
	r = QRegion( QRect(0, mSize.height() - BORDERS_SIZE, 5, BORDERS_SIZE) );
	r += QRect(0, mSize.height() - 5, BORDERS_SIZE, 5);
	a.r = r;
	a.action = DRAGGING_RESIZE_BOTTOM_LEFT;
	mResizeRegions.push_back(a);

	/* bottom-right */
	r = QRegion( QRect(mSize.width() - BORDERS_SIZE, mSize.height() - 5, BORDERS_SIZE, 5) );
	r += QRect(mSize.width() - 5, mSize.height() - BORDERS_SIZE, 5, BORDERS_SIZE);
	a.r = r;
	a.action = DRAGGING_RESIZE_BOTTOM_RIGHT;
	mResizeRegions.push_back(a);

	/* left */
	r = QRegion( QRect(0, 5, BORDERS_SIZE, mSize.height() - 5) );
	a.r = r;
	a.action = DRAGGING_RESIZE_LEFT;
	mResizeRegions.push_back(a);

	/* right */
	r = QRegion( QRect(mSize.width() - BORDERS_SIZE, 5, BORDERS_SIZE, mSize.height() - 5) );
	a.r = r;
	a.action = DRAGGING_RESIZE_RIGHT;
	mResizeRegions.push_back(a);

	handleResize();
}


void WmWindowDecoration::handleResize() {
	delete mContent;
	mContent = new QImage(mSize, QImage::Format_ARGB32);
	mContent->fill(mTheme.backColor);
	mTopContainer->setSize(QSize(mSize.width(), DECORATION_HEIGHT));
}

void WmWindowDecoration::handleMouse(const QPoint &pos, Qt::MouseButtons buttons) {
	for(auto it = mResizeRegions.begin(); it != mResizeRegions.end(); it++)
	{
		if (it->r.contains(pos)) {
			emit startDrag(it->action, mWindow);
			break;
		}
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

void WmWindowDecoration::onStartDragging(WmWidget::DraggingType dragType)
{
	emit startDrag(dragType, mWindow);
}

void WmWindowDecoration::onCloseClicked() {
	emit mWindow->close();
}

