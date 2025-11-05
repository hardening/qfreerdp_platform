/**
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

#ifndef __QFREERDPWINDOW_H__
#define __QFREERDPWINDOW_H__

#include <qpa/qplatformwindow.h>

#include <wmwidgets/wmwidget.h>

QT_BEGIN_NAMESPACE

class QFreeRdpPlatform;
class QFreeRdpBackingStore;
class WmWindowDecoration;
class QImage;

/**
 * @brief a window
 */
class QFreeRdpWindow : public QPlatformWindow
{
	Q_DECLARE_PRIVATE(QPlatformWindow)

public:
    QFreeRdpWindow(QWindow *window, QFreeRdpPlatform *platform);
    virtual ~QFreeRdpWindow();

    void setBackingStore(QFreeRdpBackingStore *b);

    /** @overload QPlatformWindow
     * @{*/
    WId winId() const override { return mWinId; }
    void setWindowState(Qt::WindowStates state) override;
    void raise() override;
    void lower() override;
    void setVisible(bool visible) override;
    void setGeometry(const QRect &rect) override;
    void propagateSizeHints() override;
    QMargins frameMargins() const override;
    void setWindowTitle(const QString &title) override;
    /** @} */

    virtual QRect outerWindowGeometry() const;

    bool isVisible() const { return mVisible; }

    WmWindowDecoration *decorations() const;
    QRegion decorationGeometry() const;
    void setDecorate(bool active);

    const QImage *windowContent();
    void center();
    void notifyDirty(const QRegion &dirty);

protected:
    QFreeRdpPlatform *mPlatform;
    QFreeRdpBackingStore *mBackingStore;
    WId mWinId;
    bool mVisible;
    bool mSentInitialResize;

    bool mDecorate;
    WmWindowDecoration *mDecorations;
};

QT_END_NAMESPACE

#endif
