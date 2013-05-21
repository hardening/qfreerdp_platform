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

#ifndef QWAYLANDCURSOR_H
#define QWAYLANDCURSOR_H

#include <qpa/qplatformcursor.h>
#include <QMap>

QT_BEGIN_NAMESPACE

class QFreeRdpScreen;


class QFreeRdpCursor : public QPlatformCursor
{
public:
	QFreeRdpCursor(QFreeRdpScreen *screen);
    ~QFreeRdpCursor();

    virtual void changeCursor(QCursor *cursor, QWindow *window);
/*    void pointerEvent(const QMouseEvent &event);
    QPoint pos() const;
    void setPos(const QPoint &pos);*/

private:
    /*enum WaylandCursor {
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

    struct wl_cursor* requestCursor(WaylandCursor shape);
    void initCursorMap();
    QWaylandDisplay *mDisplay;
    struct wl_cursor_theme *mCursorTheme;
    QPoint mLastPos;
    QMap<WaylandCursor, wl_cursor *> mCursors;
    QMultiMap<WaylandCursor, QByteArray> mCursorNamesMap; */

    QFreeRdpScreen *mScreen;
};

QT_END_NAMESPACE

#endif // QWAYLANDCURSOR_H
