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

#ifndef __QFREERDPBACKINGSTORE_H__
#define __QFREERDPBACKINGSTORE_H__

#include <qpa/qplatformbackingstore.h>
#include <QtGui/QImage>

QT_BEGIN_NAMESPACE

class QFreeRdpPlatform;

class QFreeRdpBackingStore : public QObject, public QPlatformBackingStore
{
    Q_OBJECT
public:
    QFreeRdpBackingStore(QWindow *window, QFreeRdpPlatform *platform);

    ~QFreeRdpBackingStore();

    QPaintDevice *paintDevice();

    void flush(QWindow *window, const QRegion &region, const QPoint &offset);

    void resize(const QSize &size, const QRegion &staticContents);


protected:
    QImage mImage;
    QFreeRdpPlatform *mPlatform;
};

QT_END_NAMESPACE

#endif
