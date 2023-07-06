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

#include "qfreerdpwindow.h"
#include "qfreerdpscreen.h"
#include "qfreerdpbackingstore.h"
#include "qfreerdpplatform.h"
#include "qfreerdpwindowmanager.h"

#include <QtCore/QtDebug>
#include <QPainter>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformwindow_p.h>

QT_BEGIN_NAMESPACE
static volatile int globalWinId = 1;

QFreeRdpWindow::QFreeRdpWindow(QWindow *window, QFreeRdpPlatform *platform) :
	QPlatformWindow(window),
    mPlatform(platform),
    mBackingStore(0),
    mWinId( WId(globalWinId++) ),
    mVisible(false),
    mSentInitialResize(false),
	mDecorate(false)
{
	mScreen = QPlatformScreen::platformScreenForWindow(window);
	qDebug() << "QFreeRdpWindow ctor(" << mWinId << ", type=" << window->type() << ")";

	// adapt window position
	if (window->type() == Qt::Dialog) {
		this->center();
		mDecorate = true;
	}
}

QFreeRdpWindow::~QFreeRdpWindow() {
	qDebug("QFreeRdpWindow::%s(%llu)", __func__, mWinId);
	mPlatform->mWindowManager->dropWindow(this);
}


void QFreeRdpWindow::setBackingStore(QFreeRdpBackingStore *b) {
	mBackingStore = b;
}


void QFreeRdpWindow::setWindowState(Qt::WindowState state) {
    qDebug("QFreeRdpWindow::setWindowState(0x%x)", __func__, (int)state); // << state;

    switch(state) {
    case Qt::WindowActive:
		mPlatform->mWindowManager->setFocusWindow(this);
		break;
    case Qt::WindowMaximized:
    case Qt::WindowFullScreen: {
		QRect r = mPlatform->getScreen()->geometry();
		setGeometry(r);
		break;
    }
    default:
    	break;
	}

	QWindowSystemInterface::handleWindowStateChanged(window(), state);
	QWindowSystemInterface::flushWindowSystemEvents(); // Required for oldState to work on WindowStateChanged
}

void QFreeRdpWindow::raise() {
    qDebug("QFreeRdpWindow::%s(%llu)", __func__, mWinId);
	mPlatform->mWindowManager->raise(this);
}

void QFreeRdpWindow::lower() {
    qDebug("QFreeRdpWindow::%s(%llu)", __func__, mWinId);
	mPlatform->mWindowManager->lower(this);
}

void QFreeRdpWindow::setVisible(bool visible) {
    qDebug("QFreeRdpWindow::%s(%llu,visible=%d)", __func__, mWinId, visible);
	mVisible = visible;
	QPlatformWindow::setVisible(visible);

	if (visible) {
		if (!mSentInitialResize) {
			QWindowSystemInterface::handleGeometryChange(window(), geometry());
			mSentInitialResize = true;
		}
		QWindowSystemInterface::handleWindowActivated(window(), Qt::ActiveWindowFocusReason);
	}

	mPlatform->mWindowManager->repaint(geometry());
}

void QFreeRdpWindow::setGeometry(const QRect &rect) {
	qDebug("QFreeRdpWindow::%s(%llu, %d,%d - %dx%d)", __func__, mWinId, rect.left(),
			rect.top(), rect.width(), rect.height());
	QRegion updateRegion(geometry());

	QPlatformWindow::setGeometry(rect);
	updateRegion += rect;
	mPlatform->mWindowManager->repaint(updateRegion);

	QWindowSystemInterface::handleGeometryChange(window(), rect);
	QWindowSystemInterface::handleExposeEvent(window(), QRegion(rect));
}

void QFreeRdpWindow::propagateSizeHints() {
}

#define TOP_BAR_SIZE 30
#define BORDERS_SIZE 2
#define BORDER_COLOR Qt::black
#define TITLE_COLOR Qt::white

QMargins QFreeRdpWindow::frameMargins() const
{
	switch (window()->type()) {
	case Qt::Dialog:
		return QMargins(BORDERS_SIZE, TOP_BAR_SIZE, BORDERS_SIZE, BORDERS_SIZE);
	default:
		return QMargins();
	}
}

void QFreeRdpWindow::setWindowTitle(const QString &title)
{
	QPlatformWindow::setWindowTitle(title);
}

QRect QFreeRdpWindow::outerWindowGeometry()const
{
	QRect geometry = windowGeometry();
	QMargins m = frameMargins();

	return geometry.adjusted(-m.left(), -m.top(), m.right(), m.bottom());
}

#if 0
WmWidget *QFreeRdpWindow::decorations() const {
	return mDecorations;
}
#endif

#if 0
void QFreeRdpWindow::drawDecorations() {
	QRect decoGeom = outerWindowGeometry();
	decoGeom = QRect(0, 0, decoGeom.width(), decoGeom.height());
	if (!mBorders)
		mBorders = new QImage(decoGeom.size(), QImage::Format_ARGB32_Premultiplied);

	QString title = window()->title();
	QPainter painter(mBorders);

	painter.fillRect(0, 0, decoGeom.width(), decoGeom.height(), Qt::black);

	QFont font("times", 15);
	QFontMetrics fm(font);

	painter.setFont(font);
	painter.setPen(TITLE_COLOR);
	painter.drawText(QPoint((decoGeom.width() - fm.horizontalAdvance(title)) / 2, 5 + fm.height() / 2), title);
	painter.drawLine(QPoint(0, TOP_BAR_SIZE-1), QPoint(decoGeom.width()-1, TOP_BAR_SIZE-1));
	mBordersDirty = false;

	auto closeIcons = mPlatform->getIconResource(ICON_RESOURCE_CLOSE_BUTTON);
	const QImage *closeIcon = closeIcons->normalIcon;

	mCloseButtonArea = QRect(QPoint(decoGeom.width() - closeIcon->width() - 5, 5), closeIcon->size());
	painter.drawImage(mCloseButtonArea.topLeft(), *closeIcon);

	static int counter = 0;
	QString destFile = QString("/tmp/borders_%1_%2.png").arg(mWinId).arg(counter++);
	mBorders->save(destFile, "PNG", 100);
}
#endif

const QImage *QFreeRdpWindow::getContent() {
	if (!mBackingStore) {
		qWarning("QFreeRdpWindow::%s: window %p has no backing store", __func__, (void*)this);
		return 0;
	}
	return (const QImage*) mBackingStore->paintDevice();
}

void QFreeRdpWindow::center() {

	QRect screenGeometry = mScreen->geometry();
	QRect windowGeometry = this->geometry();

	int x = (screenGeometry.width() - windowGeometry.width()) / 2;
	int y = (screenGeometry.height() - windowGeometry.height()) / 2;

	this->setGeometry(QRect(x, y, windowGeometry.width(), windowGeometry.height()));
}



QT_END_NAMESPACE
