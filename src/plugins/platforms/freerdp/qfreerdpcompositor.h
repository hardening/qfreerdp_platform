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
#ifndef __QFREERDPCOMPOSITOR_H__
#define __QFREERDPCOMPOSITOR_H__

#include <memory>

#include <QImage>

#include "qfreerdpscreen.h"

QT_BEGIN_NAMESPACE

/**
 * @brief a compositor to convert Qt to RDP screen updates
 * 
 * Qt is not optimized for refreshing a partial part of a
 * screen. This compositor minimizes the regions impacted
 * by changes.
 * 
 * For instance, on Qt 5.15 any change in a Qt Window triggers
 * a full update of the screen. This compositor will only update
 * the impacted region on a 64x64 square basis. That means if one
 * pixel the region impacted is 64x64 and not the entire screen.
 */
class QFreeRdpCompositor : public QObject {
public:
    explicit QFreeRdpCompositor(QFreeRdpScreen *screen);

    /**
     * Reset compositor
     */
    void reset(size_t width, size_t height);

	/** 
	 * Given a dirty region announced by Qt computes the effectively dirty
	 * region (also updating the shadow image during the operation).
	 *
	 * @param region input dirty region
	 * @return the real dirty region
	 */
	QRegion qtToRdpDirtyRegion(const QRegion &region);

private:

    /** 
	 * Compares a tile in the current image and the previous one and returns
     * if it has been effectively modified. The shadow image is updated
     * during the process.
	 *
	 * @param rect the tile
	 * @return if the tile is modified in the new image
	 */
	bool compareTileAndUpdate(const QRect &rect);

	/**
	 * Given a dirty rect announced by Qt computes the effectively dirty
	 * sub-rectangles as a dirty region.
	 */
	QRegion dirtyRegion(const QRect &rect);

    std::unique_ptr<QImage> mShadowImage;
    QFreeRdpScreen *mScreen;
};

QT_END_NAMESPACE

#endif // __QFREERDPCOMPOSITOR_H__