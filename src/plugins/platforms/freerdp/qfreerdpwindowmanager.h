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
#ifndef __QFREERDPWINDOWMANAGER_H___
#define __QFREERDPWINDOWMANAGER_H___

#include <QList>
#include <QRect>
#include <QRegion>

QT_BEGIN_NAMESPACE

class QFreeRdpWindow;
class QFreeRdpPlatform;

/**
 *
 */
class QFreeRdpWindowManager {
public:
	QFreeRdpWindowManager(QFreeRdpPlatform *platform);

	void addWindow(QFreeRdpWindow *window);

	void dropWindow(QFreeRdpWindow *window);

	void raise(QFreeRdpWindow *window);

	void lower(QFreeRdpWindow *window);

	void repaint(const QRegion &region);

	/** retrieve the window visible at the given position
	 * @param pos the position
	 * @return the computed window, NULL otherwise
	 */
	QWindow *getWindowAt(const QPoint pos) const;

	void setActiveWindow(QFreeRdpWindow *w) {  mActiveWindow = w; }

	QFreeRdpWindow *getActiveWindow() const { return mActiveWindow; }

	void handleMouseEvent(const QPoint &pos, Qt::MouseButtons buttons, Qt::MouseButton button, QEvent::Type eventtype);
protected:
	QFreeRdpPlatform *mPlatform;
	typedef QList<QFreeRdpWindow *> QFreeRdpWindowList;
	QFreeRdpWindowList mWindows;
	QFreeRdpWindow *mActiveWindow;
	QWindow *mEnteredWindow;
};


QT_END_NAMESPACE



#endif /* __QFREERDPWINDOWMANAGER_H___ */
