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
    ~QFreeRdpWindow();

    void setBackingStore(QFreeRdpBackingStore *b);

    /** @overload QPlatformWindow
     * @{*/
    virtual WId winId() const { return mWinId; }
    virtual void setWindowState(Qt::WindowState state);
    virtual void raise();
    virtual void lower();
    virtual void setVisible(bool visible);
    virtual void setGeometry(const QRect &rect);
    virtual void propagateSizeHints();
    virtual QMargins frameMargins() const;
    virtual void setWindowTitle(const QString &title);
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
    QPlatformScreen *mScreen;
    WId mWinId;
    bool mVisible;
    bool mSentInitialResize;

    bool mDecorate;
    WmWindowDecoration *mDecorations;
    WmColorScheme mColorTheme;
};

QT_END_NAMESPACE

#endif
