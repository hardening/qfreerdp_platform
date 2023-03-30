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

#include <QtFontDatabaseSupport/private/qgenericunixfontdatabase_p.h>
#include <QtEventDispatcherSupport/private/qgenericunixeventdispatcher_p.h>
#include <QtThemeSupport/private/qgenericunixthemes_p.h>

#include <qpa/qplatformnativeinterface.h>
#include <QtGui/private/qguiapplication_p.h>

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformcursor.h>

#include <qpa/qplatforminputcontextfactory_p.h>
#include <qpa/qplatforminputcontext.h>

#include <QtGui/QSurfaceFormat>
#include <QtCore/QSocketNotifier>

QT_BEGIN_NAMESPACE

QFreeRdpIntegration::QFreeRdpIntegration(const QStringList& paramList)
    : mFontDb(new QGenericUnixFontDatabase())
    , mEventDispatcher(createUnixEventDispatcher())
{
	//Disable desktop settings for now (or themes crash)
	QGuiApplicationPrivate::obey_desktop_settings = false;

	mPlatform = new QFreeRdpPlatform(paramList, mEventDispatcher);
	QWindowSystemInterface::handleScreenAdded(mPlatform->getScreen());
	
	mNativeInterface = new QPlatformNativeInterface();

	// set information on platform
	mNativeInterface->setProperty("freerdp_address", QVariant(mPlatform->getListenAddress()));
	mNativeInterface->setProperty("freerdp_port", QVariant(mPlatform->getListenPort()));
}

QFreeRdpIntegration::~QFreeRdpIntegration() {
}

QPlatformWindow *QFreeRdpIntegration::createPlatformWindow(QWindow *window) const {
	return mPlatform->newWindow(window);
}

QPlatformBackingStore *QFreeRdpIntegration::createPlatformBackingStore(QWindow *window) const {
    return new QFreeRdpBackingStore(window, mPlatform);
}

#if QT_VERSION < 0x050200
QAbstractEventDispatcher *QFreeRdpIntegration::guiThreadEventDispatcher() const {
    return mEventDispatcher;
}
#else
QAbstractEventDispatcher *QFreeRdpIntegration::createEventDispatcher() const {
    return mEventDispatcher;
}
#endif

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

QPlatformNativeInterface *QFreeRdpIntegration::nativeInterface() const {
	return mNativeInterface;
}

void QFreeRdpIntegration::initialize() {
	// create Input Context Plugin
	mInputContext.reset(QPlatformInputContextFactory::create());
}

QPlatformInputContext *QFreeRdpIntegration::inputContext() const {
	return mInputContext.data();
}

QT_END_NAMESPACE
