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

#include "qfreerdpintegration.h"
#include "qfreerdpscreen.h"
#include "qfreerdpbackingstore.h"
#include "qfreerdpwindow.h"
#include "qfreerdpplatform.h"


#include <QtPlatformSupport/private/qgenericunixfontdatabase_p.h>
#include <QtPlatformSupport/private/qgenericunixeventdispatcher_p.h>
#include <QtPlatformSupport/private/qgenericunixthemes_p.h>

#include <QtGui/private/qguiapplication_p.h>

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformcursor.h>

#include <QtGui/QSurfaceFormat>
#include <QtCore/QSocketNotifier>


QT_BEGIN_NAMESPACE


QFreeRdpIntegration::QFreeRdpIntegration(const QStringList& paramList)
    : mFontDb(new QGenericUnixFontDatabase())
    , mEventDispatcher(createUnixEventDispatcher())
{
	QGuiApplicationPrivate::instance()->setEventDispatcher(mEventDispatcher);

	//Disable desktop settings for now (or themes crash)
	QGuiApplicationPrivate::obey_desktop_settings = false;

	mPlatform = new QFreeRdpPlatform(paramList, mEventDispatcher);
	screenAdded(mPlatform->getScreen());
    mFontDb = new QGenericUnixFontDatabase();
}

QFreeRdpIntegration::~QFreeRdpIntegration() {
}


QPlatformWindow *QFreeRdpIntegration::createPlatformWindow(QWindow *window) const {
	return mPlatform->newWindow(window);
}


QPlatformBackingStore *QFreeRdpIntegration::createPlatformBackingStore(QWindow *window) const {
    return new QFreeRdpBackingStore(window, mPlatform);
}

QAbstractEventDispatcher *QFreeRdpIntegration::guiThreadEventDispatcher() const {
    return mEventDispatcher;
}

QPlatformFontDatabase *QFreeRdpIntegration::fontDatabase() const {
    return mFontDb;
}

QStringList QFreeRdpIntegration::themeNames() const {
    return QGenericUnixTheme::themeNames();
}

QPlatformTheme *QFreeRdpIntegration::createPlatformTheme(const QString &name) const {
    return QGenericUnixTheme::createUnixTheme(name);
}

bool QFreeRdpIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps:
    	return true;
    default:
    	return QPlatformIntegration::hasCapability(cap);
    }
}

QT_END_NAMESPACE
