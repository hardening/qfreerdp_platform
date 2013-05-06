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
#include "qfreerdpbackingstore.h"
#include "qfreerdpplatform.h"
#include "qfreerdpwindowmanager.h"

#include <QtCore/QtDebug>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformwindow_p.h>

QT_BEGIN_NAMESPACE
static volatile int globalWinId = 1;

QFreeRdpWindow::QFreeRdpWindow(QWindow *window, QFreeRdpPlatform *platform) :
	QPlatformWindow(window),
    mPlatform(platform),
    mBackingStore(0),
    mWinId( WId(globalWinId++) ),
    mVisible(false)
{
	mScreen = QPlatformScreen::platformScreenForWindow(window);
	qDebug("QFreeRdpWindow ctor(%d, type=0x%x)", mWinId, window->type());
}

QFreeRdpWindow::~QFreeRdpWindow() {
	qDebug("QFreeRdpWindow::%s(%d)", __func__, mWinId);
	mPlatform->mWindowManager->dropWindow(this);
}


void QFreeRdpWindow::setBackingStore(QFreeRdpBackingStore *b) {
	mBackingStore = b;
}

void QFreeRdpWindow::setWindowState(Qt::WindowState state) {
    qDebug() << "QFreeRdpWindow::" << __func__ << "()";
	if(state & Qt::WindowActive)
		mPlatform->mWindowManager->setActiveWindow(this);
	QPlatformWindow::setWindowState(state);
}

void QFreeRdpWindow::raise() {
    qDebug("QFreeRdpWindow::%s(%d)", __func__, mWinId);
	mPlatform->mWindowManager->raise(this);
}

void QFreeRdpWindow::lower() {
    qDebug("QFreeRdpWindow::%s(%d)", __func__, mWinId);
	mPlatform->mWindowManager->lower(this);
}

void QFreeRdpWindow::setVisible(bool visible) {
    qDebug("QFreeRdpWindow::%s(%d,visible=%d)", __func__, mWinId, visible);
	QPlatformWindow::setVisible(visible);

	mVisible = visible;
	mPlatform->mWindowManager->repaint(geometry());
}

void QFreeRdpWindow::setGeometry(const QRect &rect) {
	qDebug("QFreeRdpWindow::%s(%d, %d,%d - %dx%d)", __func__, mWinId, rect.left(),
			rect.top(), rect.width(), rect.height());
	QRegion updateRegion(geometry());

	QPlatformWindow::setGeometry(rect);
	updateRegion += rect;
	mPlatform->mWindowManager->repaint(updateRegion);
}

const QImage *QFreeRdpWindow::getContent() {
	if(!mBackingStore)
		return 0;
	return (const QImage *)mBackingStore->paintDevice();
}
#if 0
QMargins QFreeRdpWindow::frameMargins() const
{
    qDebug() << "QFreeRdpWindow::" << __func__ << "()";
    return QPlatformWindow::frameMargins();
}


WId QFreeRdpWindow::winId() const
{
    qDebug() << "QFreeRdpWindow::" << __func__ << "()";
    return static_cast<WId>(mWinId);
}

void QFreeRdpWindow::setWindowTitle(const QString & /*title*/)
{
    qDebug() << "QFreeRdpWindow::" << __func__ << "()";
}


void QFreeRdpWindow::propagateSizeHints() {
    qDebug() << "QFreeRdpWindow::" << __func__ << "()";
}

void QFreeRdpWindow::onDestroy(int winId)
{
    qDebug() << "QFreeRdpWindow::" << __func__ << "()";
    if (winId == mWinId) {
        QWindowSystemInterface::handleCloseEvent(window());
    }
}

void QFreeRdpWindow::onActivated(int winId)
{
    qDebug() << "QFreeRdpWindow::" << __func__ << "()";
    if (winId == mWinId) {
        QWindowSystemInterface::handleWindowActivated(window());
    }
}

void QFreeRdpWindow::onSetGeometry(int winId, int x, int y, int width, int height)
{
    qDebug() << "QFreeRdpWindow::" << __func__ << "()";
    if (winId == mWinId) {
        const QRect rect(x, y, width, height);
        QPlatformWindow::setGeometry(rect);
        QWindowSystemInterface::handleGeometryChange(window(), rect);
    }
}

void QFreeRdpWindow::onKeyEvent(int winId, int type, int code, int modifiers, const QString &text)
{
    qDebug() << "QFreeRdpWindow::" << __func__ << "()";
    if (winId == mWinId) {
        QWindowSystemInterface::handleKeyEvent(window(), QEvent::Type(type), code, Qt::KeyboardModifiers(modifiers), text);
    }
}

void QFreeRdpWindow::onMouseEvent(int winId, int localX, int localY, int globalX, int globalY, int buttons, int modifiers)
{
    qDebug() << "QFreeRdpWindow::" << __func__ << "()";
    if (winId == mWinId) {
        QWindowSystemInterface::handleMouseEvent(window(), QPoint(localX, localY), QPoint(globalX, globalY),
                                                 Qt::MouseButtons(buttons), Qt::KeyboardModifiers(modifiers));
    }
}

void QFreeRdpWindow::onMouseWheel(int winId, int localX, int localY, int globalX, int globalY, int delta, int modifiers)
{
    qDebug() << "QFreeRdpWindow::" << __func__ << "()";
    if (winId == mWinId) {
        delta *= 120;
        QWindowSystemInterface::handleWheelEvent(window(), QPoint(localX, localY), QPoint(globalX, globalY),
                                                 delta, Qt::Vertical, Qt::KeyboardModifiers(modifiers));
    }
}
#endif

QT_END_NAMESPACE
