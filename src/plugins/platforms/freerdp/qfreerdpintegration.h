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

#ifndef __QPLATFORMINTEGRATION_FREERDP_H__
#define __QPLATFORMINTEGRATION_FREERDP_H__

#include <qpa/qplatformintegration.h>
#include <QtGui/qpa/qplatforminputcontextfactory_p.h>


QT_BEGIN_NAMESPACE

class QAbstractEventDispatcher;
class QPlatformTheme;
class QPlatformNativeInterface;

class QFreeRdpScreen;
class QFreeRdpPlatform;

/**
 *
 */
class QFreeRdpIntegration : public QPlatformIntegration
{
public:
	QFreeRdpIntegration(const QStringList& paramList);
    ~QFreeRdpIntegration();

    /** @overload QPlatformIntegration
     * @{ */
    virtual bool hasCapability(QPlatformIntegration::Capability cap) const;
    virtual QPlatformWindow *createPlatformWindow(QWindow *window) const;
    virtual QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const;
    virtual QPlatformFontDatabase *fontDatabase() const;
    virtual QStringList themeNames() const;
    virtual QPlatformTheme *createPlatformTheme(const QString &name) const;
    virtual QPlatformNativeInterface *nativeInterface()const;

    virtual QPlatformInputContext *inputContext() const;

    virtual void initialize();


#if QT_VERSION < 0x050200
    virtual QAbstractEventDispatcher *guiThreadEventDispatcher() const;
#else
    virtual QAbstractEventDispatcher *createEventDispatcher() const;
#endif
    /** @} */

protected:
    QPlatformFontDatabase *mFontDb;
    QAbstractEventDispatcher *mEventDispatcher;
    QFreeRdpPlatform *mPlatform;
    QPlatformNativeInterface *mNativeInterface;
    QScopedPointer<QPlatformInputContext> mInputContext;
};

QT_END_NAMESPACE

#endif
