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

#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	#include <QtFontDatabaseSupport/private/qgenericunixfontdatabase_p.h>
	#include <QtEventDispatcherSupport/private/qgenericunixeventdispatcher_p.h>
	#include <QtThemeSupport/private/qgenericunixthemes_p.h>
#else
	#include <QtGui/private/qgenericunixfontdatabase_p.h>
	#include <QtGui/private/qgenericunixeventdispatcher_p.h>
	#include <QtGui/private/qgenericunixthemes_p.h>
#endif

#include <qpa/qplatformnativeinterface.h>
#include <QtGui/private/qguiapplication_p.h>

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformcursor.h>

#include <qpa/qplatforminputcontextfactory_p.h>
#include <qpa/qplatforminputcontext.h>

#include <QtGui/QSurfaceFormat>
#include <QtCore/QSocketNotifier>


#include "qfreerdpplatform.h"
#include "qfreerdpbackingstore.h"
#include "qfreerdplistener.h"
#include "qfreerdpscreen.h"
#include "qfreerdppeer.h"
#include "qfreerdpclipboard.h"
#include "qfreerdpwindow.h"
#include "qfreerdpwindowmanager.h"
#include "xcursors/qfreerdpxcursor.h"

#include <sys/socket.h>
#include <netinet/in.h>

#include <QtCore/QtDebug>
#include <winpr/ssl.h>
#include <winpr/wtsapi.h>
#include <freerdp/channels/channels.h>

#define DEFAULT_CERT_FILE 	"cert.crt"
#define DEFAULT_KEY_FILE 	"cert.key"


QFreeRdpPlatformConfig::QFreeRdpPlatformConfig(const QStringList &params) :
	bind_address(0), port(3389), fixed_socket(-1),
	server_cert( strdup(DEFAULT_CERT_FILE) ),
	server_key( strdup(DEFAULT_KEY_FILE) ),
	rdp_key( strdup(DEFAULT_KEY_FILE) ),
	tls_enabled(true),
	fps(24),
	clipboard_enabled(true),
	egfx_enabled(true),
	qtwebengine_compat(false),
	secrets_file(nullptr),
	screenSz(800, 600),
	displayMode(DisplayMode::AUTODETECT),
	theme{Qt::white, Qt::black, QFont("time", 10)}
{
	QString subVal;
	bool ok = true;
	int val;
	for(int i = 0; i < params.size(); i++) {
		QString param = params[i];
		if(param.startsWith(QLatin1String("width="))) {
			subVal = param.mid(strlen("width="));
			val = subVal.toInt(&ok);
			if(!ok) {
				qWarning() << "invalid width" << subVal;
			}

			screenSz.setWidth(val);
		} else if(param.startsWith(QLatin1String("height="))) {
			subVal = param.mid(strlen("height="));
			val = subVal.toInt(&ok);
			if(!ok) {
				qWarning() << "invalid height" << subVal;
			}
			screenSz.setHeight(val);

		} else if (param.startsWith(QLatin1String("address="))) {
			subVal = param.mid(strlen("address="));
			bind_address = strdup(subVal.toLatin1().data());
		} else if(param.startsWith(QLatin1String("port="))) {
			subVal = param.mid(strlen("port="));
			port = subVal.toInt(&ok);
			if(!ok) {
				qWarning() << "invalid port" << subVal;
			}
		} else if(param.startsWith(QLatin1String("fps="))) {
			subVal = param.mid(strlen("fps="));
			fps = subVal.toInt(&ok);
			if(!ok || (fps <= 0) || (fps > 100)) {
				qWarning() << "invalid fps value" << subVal;
			}
		} else if(param.startsWith(QLatin1String("socket="))) {
			subVal = param.mid(strlen("socket="));
			fixed_socket = subVal.toInt(&ok);
			if(!ok) {
				qWarning() << "invalid socket" << subVal;
			}
		} else if(param.startsWith(QLatin1String("cert="))) {
			subVal = param.mid(strlen("cert="));
			server_cert = strdup(subVal.toLatin1().data());
			if(!server_cert) {
				qWarning() << "invalid cert" << subVal;
			}
		} else if(param.startsWith(QLatin1String("key="))) {
			subVal = param.mid(strlen("key="));
			server_key = strdup(subVal.toLatin1().data());
			if(!server_key) {
				qWarning() << "invalid key" << subVal;
			}
		} else if(param.startsWith(QLatin1String("secrets="))) {
			subVal = param.mid(strlen("secrets="));
			secrets_file = strdup(subVal.toLatin1().data());
			if(!secrets_file) {
				qWarning() << "invalid secrets key" << subVal;
			}
		} else if(param.startsWith(QLatin1String("fg-color="))) {
			subVal = param.mid(strlen("fg-color="));
			if(QColor::isValidColor(subVal)) {
				theme.frontColor.setNamedColor(subVal);
			} else {
				qWarning() << "invalid foreground color" << subVal;
			}
		} else if(param.startsWith(QLatin1String("bg-color="))) {
			subVal = param.mid(strlen("bg-color="));
			if(QColor::isValidColor(subVal)) {
				theme.backColor.setNamedColor(subVal);
			} else {
				qWarning() << "invalid background color" << subVal;
			}
		} else if(param.startsWith(QLatin1String("font="))) {
			subVal = param.mid(strlen("font="));
			// There is no error to catch here, Qt just falls back to another
			// font if it cannot load the one requested
			theme.font.setFamily(subVal);
		} else if(param == "noegfx") {
			qDebug("disabling egfx");
			egfx_enabled = false;
		} else if(param == "noclipboard") {
			qDebug("disabling clipboard");
			clipboard_enabled = false;
		} else if(param.startsWith(QLatin1String("mode="))) {
			subVal = param.mid(strlen("mode="));
			QString mode = subVal;
			if (mode == "legacy") {
				displayMode = DisplayMode::LEGACY;
			} else if (mode == "autodetect") {
				displayMode = DisplayMode::AUTODETECT;
			} else if (mode == "optimize") {
				displayMode = DisplayMode::OPTIMIZE;
			} else {
				displayMode = DisplayMode::AUTODETECT;
				qWarning() << "invalid display mode" << mode << ", falling back to autodetect";
			}
		} else if(param == "qtwebengineKbdCompat") {
			qDebug("Enabling qtWebEngine keyboard compatibility mode");
			qtwebengine_compat = true;
		}
	}
}

QFreeRdpPlatformConfig::~QFreeRdpPlatformConfig() {
	free(bind_address);
	free(server_cert);
	free(server_key);
	free(rdp_key);
	free(secrets_file);
}



QFreeRdpPlatform::QFreeRdpPlatform(const QString& system, const QStringList& paramList)
: mFontDb(new QGenericUnixFontDatabase())
, mEventDispatcher(createUnixEventDispatcher())
, mNativeInterface(new QPlatformNativeInterface())
, mClipboard(new QFreeRdpClipboard())
, mConfig(new QFreeRdpPlatformConfig(paramList))
, mScreen(new QFreeRdpScreen(this, mConfig->screenSz.width(), mConfig->screenSz.height()))
, mCursor(new QFreeRdpCursor(this))
, mWindowManager(new QFreeRdpWindowManager(this, mConfig->fps))
, mListener(new QFreeRdpListener(this))
, mResourcesLoaded(false)
, mPlatformName(system)
{
	//Disable desktop settings for now (or themes crash)
	QGuiApplicationPrivate::obey_desktop_settings = false;
	QWindowSystemInterface::handleScreenAdded(mScreen);

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
}

char* QFreeRdpPlatform::getListenAddress() const {
	return mConfig->bind_address;
}

int QFreeRdpPlatform::getListenPort() const {
	return mConfig->port;
}


QFreeRdpPlatform::~QFreeRdpPlatform() {
	foreach(QFreeRdpPeer* peer, mPeers) {
		delete peer;
	}

	delete mListener;
	delete mCursor;
	delete mConfig;
	delete mClipboard;
	delete mNativeInterface;
	delete mFontDb;
	delete mWindowManager;
	QWindowSystemInterface::handleScreenRemoved(mScreen->screen()->handle());
}

QPlatformWindow *QFreeRdpPlatform::createPlatformWindow(QWindow *window) const {
	qDebug() << "QFreeRdpPlatform::createPlatformWindow(window=" << (void*)window << " modality=" << window->modality()
			<< " flags=" << window->flags() << ")";

	QFreeRdpWindow *ret = new QFreeRdpWindow(window, const_cast<QFreeRdpPlatform*>(this));
	mWindowManager->addWindow(ret);
	auto it = mbackingStores.find(window);
	if (it != mbackingStores.end())
		ret->setBackingStore(*it);

	return ret;
}

QPlatformBackingStore *QFreeRdpPlatform::createPlatformBackingStore(QWindow *window) const {
    return new QFreeRdpBackingStore(window, const_cast<QFreeRdpPlatform*>(this));
}

#if QT_VERSION < QT_VERSION_CHECK(5, 2, 0)
QAbstractEventDispatcher *QFreeRdpPlatform::guiThreadEventDispatcher() const {
    return mEventDispatcher;
}
#else
QAbstractEventDispatcher *QFreeRdpPlatform::createEventDispatcher() const {
    return mEventDispatcher;
}
#endif

QPlatformFontDatabase *QFreeRdpPlatform::fontDatabase() const {
    return mFontDb;
}

QStringList QFreeRdpPlatform::themeNames() const {
    return QGenericUnixTheme::themeNames();
}

QPlatformTheme *QFreeRdpPlatform::createPlatformTheme(const QString &name) const {
    return QGenericUnixTheme::createUnixTheme(name);
}

bool QFreeRdpPlatform::hasCapability(QPlatformIntegration::Capability cap) const
{
	switch (cap) {
	case ThreadedPixmaps:
		return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
	case RhiBasedRendering:
		return false;
#endif
	default:
		return QPlatformIntegration::hasCapability(cap);
	}
}

QPlatformNativeInterface *QFreeRdpPlatform::nativeInterface() const {
	return mNativeInterface;
}

void QFreeRdpPlatform::initialize() {
	if (mConfig->fixed_socket != -1) {
		freerdp_peer *client = freerdp_peer_new(mConfig->fixed_socket);
		if (!client) {
			qCritical() << "freerdp_peer_new failed, exiting";
			return;
		}

		QFreeRdpPeer *peer = new QFreeRdpPeer(this, client);
		if(!peer->init()) {
			delete peer;
		}

		registerPeer(peer);
	} else {
		mListener->initialize();
	}

	// Hack to improve support for low level keyboard events in qtWebEngine.
	// As qtWebEngine, when it is run using a platform unknown to it like
	// freerdp, it falls back to a lackluster method of guessing keyboard event
	// codes as if they were generated by a QWERTY layout.
	//
	// See:
	// https://github.com/qt/qtwebengine/blob/603a1809481eb4d4ca972f0f64915d29fb99f53b/src/core/web_event_factory.cpp#L95
	// https://github.com/qt/qtwebengine/blob/603a1809481eb4d4ca972f0f64915d29fb99f53b/src/core/web_event_factory.cpp#L1661
	//
	// To work around this, qfreerdp makes available two platform names:
	// * "freerdp": normal behavior
	// * "freerdp_xcb": qfreerdp reports its QGuiApplication::platformName() as
	//                  "xcb", which will cause qtWebEngine to correctly
	//                  interpret our QKeyEvents.
	//
	// This attribute should be set _after_ the constructor or Qt's plugin
	// loader will overwrite our change.
	if (mPlatformName.toLower() == "freerdp_xcb")
		mPlatformName = "xcb";
	QGuiApplicationPrivate::platform_name = new QString(mPlatformName);
	qDebug("Initializing qfreerdp platform, and setting platform_name to %s",
			mPlatformName.toStdString().c_str());

	// set information on platform
	mNativeInterface->setProperty("freerdp_address", QString(mConfig->bind_address));
	mNativeInterface->setProperty("freerdp_port", QVariant(mConfig->port));

	// create Input Context Plugin
	mInputContext.reset(QPlatformInputContextFactory::create());
	mWindowManager->initialize();
}

QPlatformInputContext *QFreeRdpPlatform::inputContext() const {
	return mInputContext.data();
}

QPlatformClipboard *QFreeRdpPlatform::clipboard() const {
	return mClipboard;
}


void QFreeRdpPlatform::registerPeer(QFreeRdpPeer *peer) {
	mPeers.push_back(peer);

	mNativeInterface->setProperty("freerdp_instance", QVariant::fromValue((void*)peer->freerdpPeer()));
}

void QFreeRdpPlatform::unregisterPeer(QFreeRdpPeer *peer) {
	mPeers.removeAll(peer);
	mNativeInterface->setProperty("freerdp_instance", QVariant::fromValue((void*)nullptr));
}

void QFreeRdpPlatform::registerBackingStore(QWindow *w, QFreeRdpBackingStore *back) {
	mbackingStores.insert(w, back);

	foreach(QFreeRdpWindow *rdpWindow, *mWindowManager->getAllWindows()) {
		if(rdpWindow->window() == w) {
			rdpWindow->setBackingStore(back);
			return;
		}
	}
	qWarning("did not find window %p in window manager to register its backing store", (void*)w);
}

void QFreeRdpPlatform::dropBackingStore(QFreeRdpBackingStore *back)
{
	for (auto it = mbackingStores.begin(); it != mbackingStores.end(); ++it)
		if (it.value() == back) {
			mbackingStores.erase(it);
			break;
		}
}

void QFreeRdpPlatform::repaint(const QRegion &region) {
	foreach(QFreeRdpPeer *peer, mPeers) {
		peer->repaint(region);
	}
}

void QFreeRdpPlatform::configureClient(rdpSettings *settings) {
	if(mConfig->tls_enabled) {
		settings->TLSMinVersion = 0x0303; //TLS1.2 number registered to the IANA
		rdpPrivateKey* key = freerdp_key_new_from_file(mConfig->server_key);
		if (!key) {
			qCritical() << "failed to open private key" << mConfig->server_key;
		} else {
			if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerRsaKey, key, 1)) {
				qCritical() << "failed to set FreeRDP_RdpServerRsaKey from" << mConfig->server_key;
			}
		}
		rdpCertificate* cert = freerdp_certificate_new_from_file(mConfig->server_cert);
		if (!cert) {
			qCritical() << "failed to open cert" << mConfig->server_cert;
		} else {
			if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerCertificate, cert, 1)) {
				qCritical() << "failed to set FreeRDP_RdpServerCertificate from" << mConfig->server_cert;
			}
		}
	} else {
		settings->TlsSecurity = FALSE;
		rdpPrivateKey* key = freerdp_key_new_from_file(mConfig->rdp_key);
		if (!key) {
			qCritical() << "failed to open RDP key" << mConfig->rdp_key;
		} else {
			if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerRsaKey, key, 1)) {
				qCritical() << "failed to set FreeRDP_RdpServerRsaKey from" << mConfig->rdp_key;
			}
		}
	}

	if (mConfig->secrets_file) {
		if (!freerdp_settings_set_string(settings, FreeRDP_TlsSecretsFile, mConfig->secrets_file)) {
			qCritical() << "failed to set secrets file";
		}
	}

	// FIPS mode is currently disabled
	settings->FIPSMode = FALSE;

	// make sure SSL is initialize for earlier enough for crypto, by taking advantage of winpr SSL FIPS flag for openssl initialization
	DWORD flags = WINPR_SSL_INIT_DEFAULT;
	if (settings->FIPSMode) {
		flags |= WINPR_SSL_INIT_ENABLE_FIPS;
	}

	// init SSL
	winpr_InitializeSSL(flags);

	/* FIPS Mode forces the following and overrides the following(by happening later */
	/* in the command line processing): */
	/* 1. Disables NLA Security since NLA in freerdp uses NTLM(no Kerberos support yet) which uses algorithms */
	/*      not allowed in FIPS for sensitive data. So, we disallow NLA when FIPS is required. */
	/* 2. Forces the only supported RDP encryption method to be FIPS. */
	if (settings->FIPSMode || winpr_FIPSMode())
	{
		settings->NlaSecurity = FALSE;
		settings->EncryptionMethods = ENCRYPTION_METHOD_FIPS;
	}
}

DisplayMode QFreeRdpPlatform::getDisplayMode() {
	return mConfig->displayMode;
}

void QFreeRdpPlatform::setBlankCursor()
{
	foreach(QFreeRdpPeer *peer, mPeers) {
		peer->setBlankCursor();
	}
}

void QFreeRdpPlatform::setPointer(const POINTER_LARGE_UPDATE *pointer, Qt::CursorShape newShape)
{
	foreach(QFreeRdpPeer *peer, mPeers) {
		peer->setPointer(pointer, newShape);
	}
}

IconResource::~IconResource() {
	delete normalIcon;
	delete overIcon;
}

bool QFreeRdpPlatform::loadResources() {
	struct ResItem {
		IconResourceType rtype;
		const char *normal;
		const char *hover;
	};

	ResItem items[] = {
		{ICON_RESOURCE_CLOSE_BUTTON, ":/qfreerdp/window-close.png", ":/qfreerdp/window-close#hover.png" }
	};

	for (size_t i = 0; i < ARRAYSIZE(items); i++) {
		auto item = new IconResource();
		item->normalIcon = new QImage(items[i].normal, "PNG");
		item->overIcon = new QImage(items[i].hover, "PNG");

		mResources[items[i].rtype] = item;
	}

	mResourcesLoaded = true;
	return true;
}

const IconResource *QFreeRdpPlatform::getIconResource(IconResourceType rtype) {
	if (!mResourcesLoaded && !loadResources())
		return nullptr;

	auto it = mResources.find(rtype);
	if (it == mResources.end())
		return nullptr;

	return *it;
}

const WmTheme& QFreeRdpPlatform::getTheme() {
	return mConfig->theme;
}

QFreeRdpCursor *QFreeRdpPlatform::cursorHandler() const {
	return mCursor;

}
