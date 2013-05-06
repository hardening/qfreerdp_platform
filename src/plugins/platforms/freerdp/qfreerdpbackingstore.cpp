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

#include "qfreerdpbackingstore.h"
#include "qfreerdpwindow.h"
#include "qfreerdpplatform.h"
#include "qfreerdpwindowmanager.h"

#include <qpa/qplatformwindow.h>
#include <QtCore/QtDebug>

QT_BEGIN_NAMESPACE

QFreeRdpBackingStore::QFreeRdpBackingStore(QWindow *window, QFreeRdpPlatform *platform)
    : QPlatformBackingStore(window),
      mPlatform(platform)
{
	mPlatform->registerBackingStore(window, this);
}

QFreeRdpBackingStore::~QFreeRdpBackingStore() {
}

QPaintDevice *QFreeRdpBackingStore::paintDevice() {
    return &mImage;
}

void QFreeRdpBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    Q_UNUSED(window);
    Q_UNUSED(offset);
    //qDebug() << "QFreeRdpBackingStore::" << __func__ << "()";
    foreach (const QRect &rect, (region & mDirtyRegion).rects())
        flush(rect);
    mDirtyRegion -= region;
}

void QFreeRdpBackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    Q_UNUSED(staticContents);
    qDebug("QFreeRdpBackingStore::%s(%dx%d)", __func__, size.width(), size.height());

    if (mImage.size() != size)
        mImage = QImage(size, QImage::Format_ARGB32_Premultiplied);
    mDirtyRegion = QRegion();
}


#if 0
extern void qt_scrollRectInImage(QImage &img, const QRect &rect, const QPoint &offset);
bool QFreeRdpBackingStore::scroll(const QRegion &area, int dx, int dy)
{
    qDebug() << "QFreeRdpBackingStore::" << __func__ << "()";
    const QPoint offset(dx, dy);
    foreach (const QRect &rect, area.rects()) {
        /*QMetaObject::invokeMethod(mHtmlService, "scroll",
                                  Q_ARG(int, static_cast<int>(window()->winId())),
                                  Q_ARG(int, rect.x()),
                                  Q_ARG(int, rect.y()),
                                  Q_ARG(int, rect.width()),
                                  Q_ARG(int, rect.height()),
                                  Q_ARG(int, dx),
                                  Q_ARG(int, dy));*/
        qt_scrollRectInImage(mImage, rect, offset);
    }
    return true;
}
#endif

#if 0
void QFreeRdpBackingStore::onFlush()
{
    qDebug() << "QFreeRdpBackingStore::" << __func__ << "()";
    window()->handle()->setGeometry(window()->geometry());
    window()->handle()->setVisible( window()->isVisible() );
    flush(QRect(QPoint(0, 0), window()->geometry().size()));
}
#endif

void QFreeRdpBackingStore::flush(const QRect &rect) {
	//qDebug() << "QFreeRdpBackingStore::" << __func__ << rect;

	QRect globalRect = rect.translated(window()->geometry().topLeft());

	mPlatform->mWindowManager->repaint(globalRect);
}

void QFreeRdpBackingStore::beginPaint(const QRegion &region)
{
    mDirtyRegion += region;
    QPlatformBackingStore::beginPaint(region);
}

void QFreeRdpBackingStore::endPaint()
{
    QPlatformBackingStore::endPaint();
}

QT_END_NAMESPACE
