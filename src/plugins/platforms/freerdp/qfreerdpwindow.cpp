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

#include "qfreerdpwindow.h"
#include "qfreerdpscreen.h"
#include "qfreerdpbackingstore.h"
#include "qfreerdpplatform.h"
#include "qfreerdpwindowmanager.h"
#include "qfreerdpwmwidgets.h"

#include <QtCore/QtDebug>
#include <QPainter>
#include <QImage>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformwindow_p.h>

QT_BEGIN_NAMESPACE
static volatile int globalWinId = 1;

QFreeRdpWindow::QFreeRdpWindow(QWindow *window, QFreeRdpPlatform *platform) :
	QPlatformWindow(window),
    mPlatform(platform),
    mBackingStore(nullptr),
    mWinId( WId(globalWinId++) ),
    mVisible(false),
    mSentInitialResize(false),
	mDecorate(false),
	mDecorations(nullptr)
{
	qDebug() << "QFreeRdpWindow ctor(" << mWinId << ", type=" << window->type() << ")";

	// adapt window position
	switch (window->type()) {
	case Qt::Dialog:
		this->center();
		setDecorate(true);
		break;
	default:
		break;
	}
}

#define TOP_BAR_SIZE 30
#define BORDERS_SIZE 2

void QFreeRdpWindow::setDecorate(bool active)
{
	if (active == mDecorate)
		return;

	if (active) {
		mDecorations = new WmWindowDecoration(this, mPlatform->getTheme(), mPlatform->getIconResource(ICON_RESOURCE_CLOSE_BUTTON));

		setGeometry(
				geometry().adjusted(BORDERS_SIZE, TOP_BAR_SIZE, -BORDERS_SIZE, -BORDERS_SIZE)
		);
	} else {
		delete mDecorations;
		mDecorations = nullptr;

		setGeometry(
				geometry().adjusted(-BORDERS_SIZE, -TOP_BAR_SIZE, BORDERS_SIZE, BORDERS_SIZE)
		);
	}

	mDecorate = active;
}

QFreeRdpWindow::~QFreeRdpWindow() {
	qDebug("QFreeRdpWindow::%s(%llu)", __func__, mWinId);
	mPlatform->mWindowManager->dropWindow(this);
}


void QFreeRdpWindow::setBackingStore(QFreeRdpBackingStore *b) {
	mBackingStore = b;
}


void QFreeRdpWindow::setWindowState(Qt::WindowStates state) {
	qDebug() << "QFreeRdpWindow::setWindowState(" << state << ")";

	if (state & Qt::WindowActive)
		mPlatform->mWindowManager->setFocusWindow(this);

	if (state & (Qt::WindowMaximized | Qt::WindowFullScreen))
		setGeometry(mPlatform->getScreen()->geometry());

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
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
		QWindowSystemInterface::handleWindowActivated(window(), Qt::ActiveWindowFocusReason);
#else
		QWindowSystemInterface::handleFocusWindowChanged(window(), Qt::ActiveWindowFocusReason);
#endif // QT_VERSION
	}

	mPlatform->mWindowManager->pushDirtyArea(outerWindowGeometry());
}

void QFreeRdpWindow::setGeometry(const QRect &rect) {
	qDebug("QFreeRdpWindow::%s(%llu, %d,%d - %dx%d)", __func__, mWinId, rect.left(),
			rect.top(), rect.width(), rect.height());
	QRegion updateRegion(outerWindowGeometry());

	QPlatformWindow::setGeometry(rect);
	updateRegion += outerWindowGeometry();

	QWindowSystemInterface::handleGeometryChange(window(), rect);
	QWindowSystemInterface::handleExposeEvent(window(), QRegion(rect));

	if (mDecorations) {
		mDecorations->resizeFromWindow(window());
	}

	notifyDirty(updateRegion);
}

void QFreeRdpWindow::notifyDirty(const QRegion &dirty) {
	mPlatform->mWindowManager->pushDirtyArea(dirty);
}


void QFreeRdpWindow::propagateSizeHints() {
}


QMargins QFreeRdpWindow::frameMargins() const
{
	if (mDecorate)
		return QMargins(BORDERS_SIZE, TOP_BAR_SIZE, BORDERS_SIZE, BORDERS_SIZE);

	return QMargins();
}

void QFreeRdpWindow::setWindowTitle(const QString &title)
{
	QPlatformWindow::setWindowTitle(title);
	if (mDecorations)
		mDecorations->setTitle(title);
}

QRect QFreeRdpWindow::outerWindowGeometry()const
{
	QRect geometry = windowGeometry();
	QMargins m = frameMargins();

	return geometry.adjusted(-m.left(), -m.top(), m.right(), m.bottom());
}


WmWindowDecoration *QFreeRdpWindow::decorations() const {
	return mDecorations;
}

QFreeRdpWindowManager *QFreeRdpWindow::windowManager() const {
	return mPlatform->mWindowManager;
}

QRegion QFreeRdpWindow::decorationGeometry() const {
	return mDecorations->geometryRegion();
}

const QImage *QFreeRdpWindow::windowContent() {
	if (!mBackingStore) {
		qWarning("QFreeRdpWindow::%s: window %p(%lld) has no backing store", __func__, (void*)this, mWinId);
		return nullptr;
	}
	return (const QImage*)mBackingStore->paintDevice();
}

void QFreeRdpWindow::center() {
	QPlatformScreen *screen = QPlatformScreen::platformScreenForWindow(window());
	QRect screenGeometry = screen->geometry();
	QRect windowGeometry = geometry();

	int x = (screenGeometry.width() - windowGeometry.width()) / 2;
	int y = (screenGeometry.height() - windowGeometry.height()) / 2;

	this->setGeometry(QRect(x, y, windowGeometry.width(), windowGeometry.height()));
}

QT_END_NAMESPACE
