/*
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

#include <freerdp/pointer.h>

#include <QDebug>

#include "qfreerdpxcursor.h"
#include "rdp-cursor.h"
#include "../qfreerdpwindowmanager.h"
#include "../qfreerdppeer.h"
#include "../qfreerdpplatform.h"

QFreeRdpCursor::QFreeRdpCursor(QFreeRdpPlatform *platform)
: mPlatform(platform)
, mWindowManager(platform->mWindowManager)
, mLastCursor(Qt::ArrowCursor)
{
	QByteArray cursorTheme = qgetenv("XCURSOR_THEME");
	if (cursorTheme.isEmpty())
		cursorTheme = QByteArray("default");

	QByteArray cursorSizeFromEnv = qgetenv("XCURSOR_SIZE");
	bool hasCursorSize = false;
	int cursorSize = cursorSizeFromEnv.toInt(&hasCursorSize);
	if (!hasCursorSize || (cursorSize <= 0) || (cursorSize > 96))
		cursorSize = 28;

	mCursorTheme = rdp_cursor_theme_load(cursorTheme.constData(), cursorSize);
	initCursorMap();
}

QFreeRdpCursor::~QFreeRdpCursor() {
	if (mCursorTheme)
		rdp_cursor_theme_destroy(mCursorTheme);
}

static inline void set_rdp_pointer_andmask_bit(char *data, int x, int y, int width, int height, bool on)
{
	/**
	 * MS-RDPBCGR 2.2.9.1.1.4.4:
	 * andMaskData (variable): A variable-length array of bytes.
	 * Contains the 1-bpp, bottom-up AND mask scan-line data.
	 * The AND mask is padded to a 2-byte boundary for each encoded scan-line.
	 */
	int stride, offset;
	char mvalue;

	if (width < 0 || x < 0 || x >= width) {
		return;
	}

	if (height < 0 || y < 0 || y >= height) {
		return;
	}

	stride = ((width + 15) >> 4) * 2;
	offset = stride * (height-1-y) + (x >> 3);
	mvalue = 0x80 >> (x & 7);

	if (on) {
		data[offset] |= mvalue;
	} else {
		data[offset] &= ~mvalue;
	}
}

/*void rdpSetPixel(uint32_t *dst, int x, int y, int width, int height, uint32_t value) {
	dst[]
}*/

void computeMaskAndData(struct rdp_cursor_image *image, uint32_t *data, char *mask) {
	unsigned int p;
	uint32_t *src;

	src = (uint32_t *)(image + 1);

	for (uint32_t y = 0; y < image->height; y++)	{
		for (uint32_t x = 0; x < image->width; x++) {
			p = src[y * image->width + x];
			if (p >> 24) {
				set_rdp_pointer_andmask_bit(mask, x, y, image->width, image->height, false);

				data[(image->height - y - 1) * image->width + x] = p;
			}
		}
	}

}

void QFreeRdpCursor::restoreLastCursor() {
	changeCursor(&mLastCursor, NULL);
}

//static int cursorIndex = 0;

#ifndef QT_NO_CURSOR

void QFreeRdpCursor::changeCursor(QCursor *cursor, QWindow *window) {
	Q_UNUSED(window);

	// cursor is NULL when restoring cursor to default, for example
	// when called by QGuiApplication::unsetCursor()
	mLastCursor = cursor ? *cursor : QCursor();
	const Qt::CursorShape newShape = cursor ? cursor->shape() : Qt::ArrowCursor;

	qDebug() << "changeCursor() to " << newShape;
	if (newShape == Qt::BlankCursor) {
		mPlatform->setBlankCursor();
		return;
	}

	if (newShape < Qt::BitmapCursor) {
		char data[96 * 96 * 4];
		char mask[96 * 96 / 8];

		memset(data, 0x00, 96 * 96 * 4);
		memset(mask, 0xff, 96 * 96 / 8);

		struct rdp_cursor_image *image;

		struct rdp_cursor *rdpCursor = requestCursor((RdpCursor)newShape);

		image = rdpCursor->images[0];
		/*QImage img((const uchar *)(image+1), image->width, image->height, QImage::Format_ARGB32);
		img.save( QString("/tmp/cursor-%1.png").arg(++cursorIndex) );*/


		POINTER_LARGE_UPDATE msg;
		msg.xorBpp = 32;
		msg.width = image->width;
		msg.height = image->height;
		msg.hotSpotX = image->hotspot_x;
		msg.hotSpotY = image->hotspot_y;
		msg.andMaskData = (BYTE *)mask;
		msg.lengthAndMask = ((image->width + 15) >> 4) * 2 * image->height;
		msg.xorMaskData = (BYTE *)data;
		msg.lengthXorMask = 4 * image->width * image->height;

		computeMaskAndData(image, (uint32_t *)data, mask);

		mPlatform->setPointer(&msg, newShape);
	}
}
#endif

/* stolen from qtWayland */
void QFreeRdpCursor::initCursorMap()
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

struct rdp_cursor *QFreeRdpCursor::requestCursor(RdpCursor shape) {
	struct rdp_cursor *cursor = mCursors.value(shape, 0);
	if (!cursor) {
		QList<QByteArray> cursorNames = mCursorNamesMap.values(shape);
		foreach (QByteArray name, cursorNames) {
			cursor = rdp_cursor_theme_get_cursor(mCursorTheme, name.constData());
			if (cursor) {
				mCursors.insert(shape, cursor);
				break;
			}
		}
	}

	//If there still no cursor for a shape, use the default cursor
	if (!cursor && shape != ArrowCursor)
		cursor = requestCursor(ArrowCursor);

	return cursor;
}
