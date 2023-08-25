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


#include "qfreerdpplatform.h"
#include "qfreerdpbackingstore.h"
#include "qfreerdplistener.h"
#include "qfreerdpscreen.h"
#include "qfreerdppeer.h"
#include "qfreerdpclipboard.h"
#include "qfreerdpwindow.h"
#include "qfreerdpwindowmanager.h"
#include <sys/socket.h>
#include <netinet/in.h>

#include <QtCore/QtDebug>
#include <winpr/ssl.h>
#include <winpr/wtsapi.h>
#include <freerdp/channels/channels.h>

#define DEFAULT_CERT_FILE 	"cert.crt"
#define DEFAULT_KEY_FILE 	"cert.key"


/** @brief configuration for FreeRdpPlatform */
struct QFreeRdpPlatformConfig {
	/**
	 * @param params list of parameters
	 */
	QFreeRdpPlatformConfig(const QStringList &params);

	~QFreeRdpPlatformConfig();

	char *bind_address;
	int port;
	int fixed_socket;

	char *server_cert;
	char *server_key;
	char *rdp_key;
	bool tls_enabled;
	int fps;

	QSize screenSz;
	DisplayMode displayMode;
};

QFreeRdpPlatformConfig::QFreeRdpPlatformConfig(const QStringList &params) :
	bind_address(0), port(3389), fixed_socket(-1),
	server_cert( strdup(DEFAULT_CERT_FILE) ),
	server_key( strdup(DEFAULT_KEY_FILE) ),
	rdp_key( strdup(DEFAULT_KEY_FILE) ),
	tls_enabled(true),
	fps(24),
	screenSz(800, 600),
	displayMode(DisplayMode::AUTODETECT)
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
			if(!ok) {
				qWarning() << "invalid cert" << subVal;
			}
		} else if(param.startsWith(QLatin1String("key="))) {
			subVal = param.mid(strlen("key="));
			server_key = strdup(subVal.toLatin1().data());
			if(!ok) {
				qWarning() << "invalid key" << subVal;
			}
		} else if(param.startsWith(QLatin1String("mode="))) {
			subVal = param.mid(strlen("mode="));
			QString mode = strdup(subVal.toLatin1().data());
			if (mode == "legacy") {
				displayMode = DisplayMode::LEGACY;
			} else if (mode == "autodetect") {
				displayMode = DisplayMode::AUTODETECT;
			} else if (mode == "optimize") {
				displayMode = DisplayMode::OPTIMIZE;
			} else {
				displayMode = DisplayMode::AUTODETECT;
			}
		}
	}
}

QFreeRdpPlatformConfig::~QFreeRdpPlatformConfig() {
	free(bind_address);
	free(server_cert);
	free(server_key);
	free(rdp_key);
}

QFreeRdpListener::QFreeRdpListener(QFreeRdpPlatform *platform) :
	listener(0),
	mSocketNotifier(0),
	mPlatform(platform) {
}


BOOL QFreeRdpListener::rdp_incoming_peer(freerdp_listener* instance, freerdp_peer* client)
{
	qDebug() << "got an incoming connection";

	QFreeRdpListener *listener = (QFreeRdpListener *)instance->param4;
	QFreeRdpPeer *peer = new QFreeRdpPeer(listener->mPlatform, client);
	if(!peer->init()) {
		delete peer;
		return FALSE;
	}

	listener->mPlatform->registerPeer(peer);
	return TRUE;
}

void QFreeRdpListener::incomingNewPeer() {
	if(!listener->CheckFileDescriptor(listener))
		qDebug() << "unable to CheckFileDescriptor\n";
}


void QFreeRdpListener::initialize() {
	int fd;
	HANDLE events[32];

	listener = freerdp_listener_new();
	listener->PeerAccepted = QFreeRdpListener::rdp_incoming_peer;
	listener->param4 = this;

	auto config = mPlatform->mConfig;
	if (config->fixed_socket == -1) {
		if(!listener->Open(listener, config->bind_address, config->port)) {
			qCritical() << "unable to bind rdp socket\n";
			return;
		}

		if (!listener->GetEventHandles(listener, events, 32)) {
				qCritical("Failed to get FreeRDP event handle");
				return;
		}

		if (events[1]) {
			qDebug("freerdp is also listening on a second socket, but we ignore it");
		}

		if (!events[0]) {
			qCritical("could not obtain FreeRDP event handle");
			return;
		}

		fd = GetEventFileDescriptor(events[0]);

		if (fd < 0) {
			qCritical("could not obtain FreeRDP listening socket from event handle");
			return;
		}

		if (config->port == 0) {
			// set port number if specified port is 0 (random port number)
			struct sockaddr_in sin;
			socklen_t len = sizeof(sin);
			if (getsockname(fd, (struct sockaddr *)&sin, &len) == -1)
			    perror("getsockname");
			else
			    config->port = ntohs(sin.sin_port);
		}

	} else {
		// initialize with specified socket
		fd = config->fixed_socket;
	}
	// this function is sometimes invoked from a freerpd thread, but QSocketNotifier constructor
	// needs to be invoked on a QThread. QMetaObject::invokeMethod queues the function to be
	// called by the event loop, and more specifically the thread that owns this
	QMetaObject::invokeMethod(this, "startListener", Qt::QueuedConnection, Q_ARG(int, fd));
}

// must be invoked on the thread that owns `this`
void QFreeRdpListener::startListener(int fd) {
	mSocketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
	mPlatform->getDispatcher()->registerSocketNotifier(mSocketNotifier);

	connect(mSocketNotifier, SIGNAL(activated(int)), this, SLOT(incomingNewPeer()));
}


QFreeRdpPlatform::QFreeRdpPlatform(const QStringList& paramList)
: mFontDb(new QGenericUnixFontDatabase())
, mEventDispatcher(createUnixEventDispatcher())
, mNativeInterface(new QPlatformNativeInterface())
, mClipboard(new QFreeRdpClipboard())
, mConfig(new QFreeRdpPlatformConfig(paramList))
, mScreen(new QFreeRdpScreen(this, mConfig->screenSz.width(), mConfig->screenSz.height()))
, mWindowManager(new QFreeRdpWindowManager(this, mConfig->fps))
, mListener(new QFreeRdpListener(this))
, mResourcesLoaded(false)
{
	mListener->initialize();

	//Disable desktop settings for now (or themes crash)
	QGuiApplicationPrivate::obey_desktop_settings = false;
	QWindowSystemInterface::handleScreenAdded(mScreen);

	// set information on platform
	mNativeInterface->setProperty("freerdp_address", QVariant(mConfig->bind_address));
	mNativeInterface->setProperty("freerdp_port", QVariant(mConfig->port));

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
}

QFreeRdpPlatform::~QFreeRdpPlatform() {
	delete mConfig;
}

QPlatformWindow *QFreeRdpPlatform::createPlatformWindow(QWindow *window) const {
	qDebug() << "QFreeRdpPlatform::createPlatformWindow(modality=" << window->modality()
			<< " flags=" << window->flags() << ")";

	QFreeRdpWindow *ret = new QFreeRdpWindow(window, const_cast<QFreeRdpPlatform*>(this));
	mWindowManager->addWindow(ret);
	return ret;
}

QPlatformBackingStore *QFreeRdpPlatform::createPlatformBackingStore(QWindow *window) const {
    return new QFreeRdpBackingStore(window, const_cast<QFreeRdpPlatform*>(this));
}

#if QT_VERSION < 0x050200
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
    default:
    	return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformNativeInterface *QFreeRdpPlatform::nativeInterface() const {
	return mNativeInterface;
}

void QFreeRdpPlatform::initialize() {
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
}

void QFreeRdpPlatform::unregisterPeer(QFreeRdpPeer *peer) {
	mPeers.removeAll(peer);
}

void QFreeRdpPlatform::registerBackingStore(QWindow *w, QFreeRdpBackingStore *back) {
	foreach(QFreeRdpWindow *rdpWindow, *mWindowManager->getAllWindows()) {
		if(rdpWindow->window() == w) {
			rdpWindow->setBackingStore(back);
			return;
		}
	}
	qWarning("did not find window %p in window manager to register its backing store", (void*)w);
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

