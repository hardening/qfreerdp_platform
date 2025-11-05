/**
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

#ifndef __QFREERDPSCREEN_H__
#define __QFREERDPSCREEN_H__

#include <qpa/qplatformscreen.h>

QT_BEGIN_NAMESPACE

class QFreeRdpPlatform;
class QFreeRdpWindowManager;
class QFreeRdpCursor;

class QFreeRdpScreen : public QObject, public QPlatformScreen
{
	friend class QFreeRdpWindowManager;
	friend class QFreeRdpPlatform;

    Q_OBJECT
public:
    QFreeRdpScreen(QFreeRdpPlatform *platform, int width, int height);
    virtual ~QFreeRdpScreen();

    QRect geometry() const override;
    int depth() const override;
    QImage::Format format() const override;
    qreal refreshRate() const override;
    QPlatformCursor *cursor() const override;

    QImage *getScreenBits() { return mScreenBits; }
public slots:
    void setGeometry(const QRect &geometry);

protected:
    QRect mGeometry;
    QFreeRdpPlatform *mPlatform;
    QImage *mScreenBits;
};

QT_END_NAMESPACE

#endif
