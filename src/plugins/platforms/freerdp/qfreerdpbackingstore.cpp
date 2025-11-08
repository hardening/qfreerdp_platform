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

#include "qfreerdpbackingstore.h"
#include "qfreerdpwindow.h"
#include "qfreerdpplatform.h"
#include "qfreerdpwindowmanager.h"

#include <qpa/qplatformwindow.h>
#include <QtCore/QtDebug>

QT_BEGIN_NAMESPACE

QFreeRdpBackingStore::QFreeRdpBackingStore(QWindow *window, QFreeRdpPlatform *platform)
: QPlatformBackingStore(window)
, mPlatform(platform)
{
	mPlatform->registerBackingStore(window, this);
}

QFreeRdpBackingStore::~QFreeRdpBackingStore() {
	mPlatform->dropBackingStore(this);
}

QPaintDevice *QFreeRdpBackingStore::paintDevice() {
    return &mImage;
}

void QFreeRdpBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    Q_UNUSED(offset);

    mPlatform->mWindowManager->pushDirtyArea(
    		region.translated(window->geometry().topLeft())
	);
}

void QFreeRdpBackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    Q_UNUSED(staticContents);

    if (mImage.size() != size)
        mImage = QImage(size, QImage::Format_ARGB32_Premultiplied);
}

QT_END_NAMESPACE
