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
#ifndef __QFREERDPWINDOWMANAGER_H___
#define __QFREERDPWINDOWMANAGER_H___

#include <QList>
#include <QRect>
#include <QRegion>
#include <QTimer>

QT_BEGIN_NAMESPACE

class QFreeRdpWindow;
class QFreeRdpPlatform;
class WmWidget;


/**
 * @brief component handling windows (placement, decorations, events)
 */
class QFreeRdpWindowManager : public QObject {
	Q_OBJECT
public:
	QFreeRdpWindowManager(QFreeRdpPlatform *platform, int fps);

	void initialize();

	void addWindow(QFreeRdpWindow *window);

	void dropWindow(QFreeRdpWindow *window);

	void raise(QFreeRdpWindow *window);

	void lower(QFreeRdpWindow *window);

	void pushDirtyArea(const QRegion &region);

	void repaint(const QRegion &region);

	/** retrieve the window visible at the given position
	 * @param pos the position
	 * @return the computed window, NULL otherwise
	 */
	QFreeRdpWindow *getWindowAt(const QPoint pos) const;

	void setFocusWindow(QFreeRdpWindow *w);

	QFreeRdpWindow *getFocusWindow() const { return mFocusWindow; }

	bool handleMouseEvent(const QPoint &pos, Qt::MouseButtons buttons);
	bool handleWheelEvent(const QPoint &pos, int wheelDelta);

	typedef QList<QFreeRdpWindow *> QFreeRdpWindowList;
    QFreeRdpWindowList const *getAllWindows() const { return &mWindows; }

protected slots:
	void onGenerateFrame();

protected:
	QFreeRdpPlatform *mPlatform;
	QFreeRdpWindowList mWindows;
	QFreeRdpWindow *mFocusWindow;
	QWindow *mEnteredWindow;
	WmWidget *mEnteredWidget;
	int mDecoratedWindows;
	bool mDoDecorate;
	int mFps;

	QTimer mFrameTimer;
	QRegion mDirtyRegion;
};


QT_END_NAMESPACE



#endif /* __QFREERDPWINDOWMANAGER_H___ */
