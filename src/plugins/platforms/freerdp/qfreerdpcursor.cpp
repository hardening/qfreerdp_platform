#include "qfreerdpcursor.h"
#include "qfreerdpscreen.h"

#include <QtGui/QImageReader>
#include <QDebug>


QT_USE_NAMESPACE

QFreeRdpCursor::QFreeRdpCursor(QFreeRdpScreen *screen) : mScreen(screen)
{
}

QFreeRdpCursor::~QFreeRdpCursor()
{
}

void QFreeRdpCursor::changeCursor(QCursor *cursor, QWindow *window)
{
	const Qt::CursorShape newShape = cursor ? cursor->shape() : Qt::ArrowCursor;
	qDebug("QFreeRdpCursor::%s(%p, %p) = %d", __func__, (void *)cursor, (void *)window, newShape);

    //struct wl_cursor *waylandCursor = 0;

    if (newShape < Qt::BitmapCursor) {
        //waylandCursor = requestCursor((WaylandCursor)newShape);
    } else if (newShape == Qt::BitmapCursor) {
        //TODO: Bitmap cursor logic
    } else {
        //TODO: Custom cursor logic (for resize arrows)
    }

#if 0
    if (!waylandCursor) {
        qDebug("Could not find cursor for shape %d", newShape);
        return;
    }

    struct wl_cursor_image *image = waylandCursor->images[0];

    struct wl_buffer *buffer = wl_cursor_image_get_buffer(image);
    if (!buffer) {
        qDebug("Could not find buffer for cursor");
        return;
    }

    mDisplay->setCursor(buffer, image);
#endif
}

#if 0
void QWaylandDisplay::setCursor(struct wl_buffer *buffer, struct wl_cursor_image *image)
{
    /* Qt doesn't tell us which input device we should set the cursor
     * for, so set it for all devices. */
    for (int i = 0; i < mInputDevices.count(); i++) {
        QWaylandInputDevice *inputDevice = mInputDevices.at(i);
        inputDevice->setCursor(buffer, image);
    }
}

void QWaylandCursor::pointerEvent(const QMouseEvent &event)
{
    mLastPos = event.globalPos();
}

QPoint QWaylandCursor::pos() const
{
    return mLastPos;
}

void QWaylandCursor::setPos(const QPoint &pos)
{
    Q_UNUSED(pos);
    qWarning() << "QWaylandCursor::setPos: not implemented";
}

wl_cursor *QWaylandCursor::requestCursor(WaylandCursor shape)
{
    if (shape == BlankCursor)
        return 0;

    struct wl_cursor *cursor = mCursors.value(shape, 0);

    //If the cursor has not been loaded already, load it
    if (!cursor) {
        QList<QByteArray> cursorNames = mCursorNamesMap.values(shape);
        foreach (QByteArray name, cursorNames) {
            cursor = wl_cursor_theme_get_cursor(mCursorTheme, name.constData());
            if (cursor) {
                mCursors.insert(shape, cursor);
                break;
            }
        }
    }

    //If there still no cursor for a shape, use the default cursor
    if (!cursor && shape != ArrowCursor) {
        cursor = requestCursor(ArrowCursor);
    }

    return cursor;
}


void QWaylandCursor::initCursorMap()
{
    //Fill the cursor name map will the table of xcursor names
    mCursorNamesMap.insert(ArrowCursor, "left_ptr");
    mCursorNamesMap.insert(ArrowCursor, "default");
    mCursorNamesMap.insert(ArrowCursor, "top_left_arrow");
    mCursorNamesMap.insert(ArrowCursor, "left_arrow");

    mCursorNamesMap.insert(UpArrowCursor, "up_arrow");

    mCursorNamesMap.insert(CrossCursor, "cross");

    mCursorNamesMap.insert(WaitCursor, "wait");
    mCursorNamesMap.insert(WaitCursor, "watch");
    mCursorNamesMap.insert(WaitCursor, "0426c94ea35c87780ff01dc239897213");

    mCursorNamesMap.insert(IBeamCursor, "ibeam");
    mCursorNamesMap.insert(IBeamCursor, "text");
    mCursorNamesMap.insert(IBeamCursor, "xterm");

    mCursorNamesMap.insert(SizeVerCursor, "size_ver");
    mCursorNamesMap.insert(SizeVerCursor, "ns-resize");
    mCursorNamesMap.insert(SizeVerCursor, "v_double_arrow");
    mCursorNamesMap.insert(SizeVerCursor, "00008160000006810000408080010102");

    mCursorNamesMap.insert(SizeHorCursor, "size_hor");
    mCursorNamesMap.insert(SizeHorCursor, "ew-resize");
    mCursorNamesMap.insert(SizeHorCursor, "h_double_arrow");
    mCursorNamesMap.insert(SizeHorCursor, "028006030e0e7ebffc7f7070c0600140");

    mCursorNamesMap.insert(SizeBDiagCursor, "size_bdiag");
    mCursorNamesMap.insert(SizeBDiagCursor, "nesw-resize");
    mCursorNamesMap.insert(SizeBDiagCursor, "50585d75b494802d0151028115016902");
    mCursorNamesMap.insert(SizeBDiagCursor, "fcf1c3c7cd4491d801f1e1c78f100000");

    mCursorNamesMap.insert(SizeFDiagCursor, "size_fdiag");
    mCursorNamesMap.insert(SizeFDiagCursor, "nwse-resize");
    mCursorNamesMap.insert(SizeFDiagCursor, "38c5dff7c7b8962045400281044508d2");
    mCursorNamesMap.insert(SizeFDiagCursor, "c7088f0f3e6c8088236ef8e1e3e70000");

    mCursorNamesMap.insert(SizeAllCursor, "size_all");

    mCursorNamesMap.insert(SplitVCursor, "split_v");
    mCursorNamesMap.insert(SplitVCursor, "row-resize");
    mCursorNamesMap.insert(SplitVCursor, "sb_v_double_arrow");
    mCursorNamesMap.insert(SplitVCursor, "2870a09082c103050810ffdffffe0204");
    mCursorNamesMap.insert(SplitVCursor, "c07385c7190e701020ff7ffffd08103c");

    mCursorNamesMap.insert(SplitHCursor, "split_h");
    mCursorNamesMap.insert(SplitHCursor, "col-resize");
    mCursorNamesMap.insert(SplitHCursor, "sb_h_double_arrow");
    mCursorNamesMap.insert(SplitHCursor, "043a9f68147c53184671403ffa811cc5");
    mCursorNamesMap.insert(SplitHCursor, "14fef782d02440884392942c11205230");

    mCursorNamesMap.insert(PointingHandCursor, "pointing_hand");
    mCursorNamesMap.insert(PointingHandCursor, "pointer");
    mCursorNamesMap.insert(PointingHandCursor, "hand1");
    mCursorNamesMap.insert(PointingHandCursor, "e29285e634086352946a0e7090d73106");

    mCursorNamesMap.insert(ForbiddenCursor, "forbidden");
    mCursorNamesMap.insert(ForbiddenCursor, "not-allowed");
    mCursorNamesMap.insert(ForbiddenCursor, "crossed_circle");
    mCursorNamesMap.insert(ForbiddenCursor, "circle");
    mCursorNamesMap.insert(ForbiddenCursor, "03b6e0fcb3499374a867c041f52298f0");

    mCursorNamesMap.insert(WhatsThisCursor, "whats_this");
    mCursorNamesMap.insert(WhatsThisCursor, "help");
    mCursorNamesMap.insert(WhatsThisCursor, "question_arrow");
    mCursorNamesMap.insert(WhatsThisCursor, "5c6cd98b3f3ebcb1f9c7f1c204630408");
    mCursorNamesMap.insert(WhatsThisCursor, "d9ce0ab605698f320427677b458ad60b");

    mCursorNamesMap.insert(BusyCursor, "left_ptr_watch");
    mCursorNamesMap.insert(BusyCursor, "half-busy");
    mCursorNamesMap.insert(BusyCursor, "progress");
    mCursorNamesMap.insert(BusyCursor, "00000000000000020006000e7e9ffc3f");
    mCursorNamesMap.insert(BusyCursor, "08e8e1c95fe2fc01f976f1e063a24ccd");

    mCursorNamesMap.insert(OpenHandCursor, "openhand");
    mCursorNamesMap.insert(OpenHandCursor, "fleur");
    mCursorNamesMap.insert(OpenHandCursor, "5aca4d189052212118709018842178c0");
    mCursorNamesMap.insert(OpenHandCursor, "9d800788f1b08800ae810202380a0822");

    mCursorNamesMap.insert(ClosedHandCursor, "closedhand");
    mCursorNamesMap.insert(ClosedHandCursor, "grabbing");
    mCursorNamesMap.insert(ClosedHandCursor, "208530c400c041818281048008011002");

    mCursorNamesMap.insert(DragCopyCursor, "dnd-copy");
    mCursorNamesMap.insert(DragCopyCursor, "copy");

    mCursorNamesMap.insert(DragMoveCursor, "dnd-move");
    mCursorNamesMap.insert(DragMoveCursor, "move");

    mCursorNamesMap.insert(DragLinkCursor, "dnd-link");
    mCursorNamesMap.insert(DragLinkCursor, "link");

    mCursorNamesMap.insert(ResizeNorthCursor, "n-resize");
    mCursorNamesMap.insert(ResizeNorthCursor, "top_side");

    mCursorNamesMap.insert(ResizeSouthCursor, "s-resize");
    mCursorNamesMap.insert(ResizeSouthCursor, "bottom_side");

    mCursorNamesMap.insert(ResizeEastCursor, "e-resize");
    mCursorNamesMap.insert(ResizeEastCursor, "right_side");

    mCursorNamesMap.insert(ResizeWestCursor, "w-resize");
    mCursorNamesMap.insert(ResizeWestCursor, "left_side");

    mCursorNamesMap.insert(ResizeNorthWestCursor, "nw-resize");
    mCursorNamesMap.insert(ResizeNorthWestCursor, "top_left_corner");

    mCursorNamesMap.insert(ResizeSouthEastCursor, "se-resize");
    mCursorNamesMap.insert(ResizeSouthEastCursor, "bottom_right_corner");

    mCursorNamesMap.insert(ResizeNorthEastCursor, "ne-resize");
    mCursorNamesMap.insert(ResizeNorthEastCursor, "top_right_corner");

    mCursorNamesMap.insert(ResizeSouthWestCursor, "sw-resize");
    mCursorNamesMap.insert(ResizeSouthWestCursor, "bottom_left_corner");
}
#endif
