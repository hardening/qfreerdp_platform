/*
 * Copyright © 2023 Rubycat <support@rubycat.eu>
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

#include <memory>

#include <QImage>

#include "qfreerdpcompositor.h"

#define SHADOW_TILE_SIZE 64

#ifdef NDEBUG
#define DEBUG false
#else
#define DEBUG false
#endif

QT_BEGIN_NAMESPACE

// helper function to get region area in pixels
static int area(const QRegion &region) {
	int sz = 0;

	for (const QRect& rect: region)
	{
		sz += rect.width() * rect.height();
	}

	return sz;
}

QFreeRdpCompositor::QFreeRdpCompositor(QFreeRdpScreen *screen) :
        QObject(screen),
        mScreen(screen) {}

void QFreeRdpCompositor::reset(size_t width, size_t height) {
	mShadowImage = std::make_unique<QImage>(
		QSize(width, height),
		QImage::Format_ARGB32_Premultiplied
	);
	mShadowImage->fill(Qt::black);
}

QRegion QFreeRdpCompositor::qtToRdpDirtyRegion(const QRegion &region) {
	QRegion dirty;
	int inSize = 0;

	inSize += area(region);

	// if Qt compositor has a small enough tile size
	// do not try to reduce it.
	if (inSize <= SHADOW_TILE_SIZE) {
		return region;
	}

	for (const QRect& rect: region)	{
		dirty += dirtyRegion(rect);
	}

	if (DEBUG) {
		int outSize = area(dirty);
		qDebug("%s: gain %d%%", __FUNCTION__, (outSize * 100) / inSize);
	}

	return dirty;
}

bool QFreeRdpCompositor::compareTileAndUpdate(const QRect &rect) {
	const QImage *srcImg = mScreen->getScreenBits();
	int SrcStride = srcImg->bytesPerLine();
	const int bytesPerPixel = 4;
	const uchar *src = srcImg->bits() + (rect.top() * SrcStride) + (rect.left() * bytesPerPixel);

	int shadowStride = mShadowImage->bytesPerLine();
	uchar *shadow = mShadowImage->bits() + (rect.top() * shadowStride) + (rect.left() * bytesPerPixel);

	bool ret = false;
	int tileWidth = rect.width() * bytesPerPixel;
	for (int y = rect.top(); y <= rect.bottom(); y++, src += SrcStride, shadow += shadowStride) {
		if (memcmp(src, shadow, tileWidth) != 0) {
			ret = true;
			memcpy(shadow, src, tileWidth);
		}
	}

	return ret;
}

QRegion QFreeRdpCompositor::dirtyRegion(const QRect &rect) {
	int y = rect.top();
	int ymax = y + rect.height();

	QRegion dirty;
	while (y < ymax) {
		int height = std::min(ymax - y, SHADOW_TILE_SIZE);
		int x = rect.left();
		int xmax = x + rect.width();
		while (x < xmax) {
			int width = std::min(xmax - x, SHADOW_TILE_SIZE);

			QRect tile(QPoint(x, y), QSize(width, height));
			if (compareTileAndUpdate(tile)) {
				dirty += tile;
			}
			x += width;
		}

		y += height;
	}

	return dirty;
}

QT_END_NAMESPACE
