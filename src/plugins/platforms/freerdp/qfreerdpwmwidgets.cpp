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

WmWindowDecoration::WmWindowDecoration(QFreeRdpWindow *freerdpW, const WmColorScheme &theme, const IconResource *closeRes, WmWidget *parent)
: WmWidget(parent)
, mWindow(freerdpW)
, mTopSpacer(new WmSpacer(QSize(5, DECORATION_HEIGHT)))
, mTopSpacer2(new WmSpacer(QSize(5, DECORATION_HEIGHT)))
, mTitle(new WmLabel(freerdpW->window()->title(), QFont("time", 10)))
, mCloseButton(new WmIconButton(closeRes->normalIcon, closeRes->overIcon))
, mTopContainer(new WmHContainer(this))
, mEnteredWidget(nullptr)
, mDirty(true)
, mContent(nullptr)
{
	mColors = theme;
	mTopContainer->setColors(theme);
	mTopSpacer->setColors(theme);
	mTopSpacer2->setColors(theme);
	mTitle->setColors(theme);


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

	handleResize();
}


void WmWindowDecoration::handleResize() {
	delete mContent;
	mContent = new QImage(mSize, QImage::Format_ARGB32);
	mTopContainer->setSize(QSize(mSize.width(), DECORATION_HEIGHT));
}

void WmWindowDecoration::handleMouse(const QPoint &pos, Qt::MouseButtons buttons) {
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

void WmWindowDecoration::onCloseClicked() {
	emit mWindow->close();
}

