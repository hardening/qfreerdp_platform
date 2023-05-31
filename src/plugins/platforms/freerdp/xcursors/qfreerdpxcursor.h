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


#ifndef QFREERDP_XCURSOR_H__
#define QFREERDP_XCURSOR_H__

#include <QString>
#include <QMultiMap>
#include <qpa/qplatformcursor.h>

class QFreeRdpWindowManager;
class QFreeRdpPlatform;
struct rdp_cursor_theme;
struct rdp_cursor;

/**
 * @brief
 */
class QFreeRdpCursor : public QPlatformCursor {
public:
	QFreeRdpCursor(QFreeRdpPlatform *platform);

	~QFreeRdpCursor();

#ifndef QT_NO_CURSOR
    virtual void changeCursor(QCursor *windowCursor, QWindow *window);
#endif

    void restoreLastCursor();

protected:
    /** @brief type of cursor */
    enum RdpCursor {
		ArrowCursor = Qt::ArrowCursor,
		UpArrowCursor,
		CrossCursor,
		WaitCursor,
		IBeamCursor,
		SizeVerCursor,
		SizeHorCursor,
		SizeBDiagCursor,
		SizeFDiagCursor,
		SizeAllCursor,
		BlankCursor,
		SplitVCursor,
		SplitHCursor,
		PointingHandCursor,
		ForbiddenCursor,
		WhatsThisCursor,
		BusyCursor,
		OpenHandCursor,
		ClosedHandCursor,
		DragCopyCursor,
		DragMoveCursor,
		DragLinkCursor,
		ResizeNorthCursor = Qt::CustomCursor + 1,
		ResizeSouthCursor,
		ResizeEastCursor,
		ResizeWestCursor,
		ResizeNorthWestCursor,
		ResizeSouthEastCursor,
		ResizeNorthEastCursor,
		ResizeSouthWestCursor
	};

protected:
    void initCursorMap();
    struct rdp_cursor *requestCursor(RdpCursor shape);

protected:
    QFreeRdpPlatform *mPlatform;
    QFreeRdpWindowManager *mWindowManager;
    QMultiMap<RdpCursor, QByteArray> mCursorNamesMap;
    QMap<RdpCursor, struct rdp_cursor *> mCursors;
    struct rdp_cursor_theme *mCursorTheme;
    QCursor mLastCursor;
};


#endif // QFREERDP_XCURSOR_H__
