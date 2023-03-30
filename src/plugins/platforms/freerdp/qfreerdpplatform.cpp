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

#include "qfreerdpplatform.h"
#include "qfreerdplistener.h"
#include "qfreerdpscreen.h"
#include "qfreerdppeer.h"
#include "qfreerdpwindow.h"
#include "qfreerdpwindowmanager.h"
#include <sys/socket.h>
#include <netinet/in.h>

#include <QtCore/QtDebug>
#include <winpr/ssl.h>

#define DEFAULT_CERT_FILE 	"cert.crt"
#define DEFAULT_KEY_FILE 	"cert.key"


/** @brief private data for FreeRdpPlatform */
struct QFreeRdpPlatformConfig {
	/**
	 * @param params
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

	QList<QFreeRdpPeer *> peers;
	QList<QFreeRdpWindow *> windows;
	QSize screenSz;

	DisplayMode displayMode;
};

QFreeRdpPlatformConfig::QFreeRdpPlatformConfig(const QStringList &params) :
	bind_address(0), port(3389), fixed_socket(-1),
	server_cert( strdup(DEFAULT_CERT_FILE) ),
	server_key( strdup(DEFAULT_KEY_FILE) ),
	rdp_key( strdup(DEFAULT_KEY_FILE) ),
	tls_enabled(true),
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
	int rcount = 0;
	void* rfds[32];

	listener = freerdp_listener_new();
	listener->PeerAccepted = QFreeRdpListener::rdp_incoming_peer;
	listener->param4 = this;
	if (mPlatform->config->fixed_socket == -1) {
		if(!listener->Open(listener, mPlatform->config->bind_address, mPlatform->config->port)) {
			qDebug() << "unable to bind rdp socket\n";
			return;
		}

		if (!listener->GetFileDescriptor(listener, rfds, &rcount) || rcount < 1) {
				qDebug("Failed to get FreeRDP file descriptor\n");
				return;
		}

		fd = (int)(long)(rfds[0]);

		if (mPlatform->config->port == 0) {
			// set port number if specified port is 0 (random port number)
			struct sockaddr_in sin;
			socklen_t len = sizeof(sin);
			if (getsockname(fd, (struct sockaddr *)&sin, &len) == -1)
			    perror("getsockname");
			else
			    mPlatform->config->port = ntohs(sin.sin_port);
		}

	} else {
		// initialize with specified socket
		fd = mPlatform->config->fixed_socket;
	}

	mSocketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
	mPlatform->getDispatcher()->registerSocketNotifier(mSocketNotifier);

	connect(mSocketNotifier, SIGNAL(activated(int)), this, SLOT(incomingNewPeer()));
}


QFreeRdpPlatform::QFreeRdpPlatform(const QStringList& paramList, QAbstractEventDispatcher *dispatcher) :
	mDispatcher(dispatcher), mListener(0)
{
	config = new QFreeRdpPlatformConfig(paramList);
	mScreen = new QFreeRdpScreen(this, config->screenSz.width(), config->screenSz.height());

	mWindowManager = new QFreeRdpWindowManager(this);
	mListener = new QFreeRdpListener(this);
	mListener->initialize();
}

QFreeRdpPlatform::~QFreeRdpPlatform() {
	delete config; 
}

char* QFreeRdpPlatform::getListenAddress() const {
	return config->bind_address;
}

int QFreeRdpPlatform::getListenPort() const {
	return config->port;
}

void QFreeRdpPlatform::registerPeer(QFreeRdpPeer *peer) {
	config->peers.push_back(peer);
}

void QFreeRdpPlatform::unregisterPeer(QFreeRdpPeer *peer) {
	config->peers.removeAll(peer);
}

void QFreeRdpPlatform::registerBackingStore(QWindow *w, QFreeRdpBackingStore *back) {
	foreach(QFreeRdpWindow *rdpWindow, config->windows) {
		if(rdpWindow->window() == w) {
			rdpWindow->setBackingStore(back);
			break;
		}
	}
}

void QFreeRdpPlatform::repaint(const QRegion &region) {
	foreach(QFreeRdpPeer *peer, config->peers) {
		peer->repaintWithCompositor(region);
	}
}

QPlatformWindow *QFreeRdpPlatform::newWindow(QWindow *window) {
	QFreeRdpWindow *ret = new QFreeRdpWindow(window, this);
	config->windows.push_back(ret);
	mWindowManager->addWindow(ret);
	return ret;
}


void QFreeRdpPlatform::configureClient(rdpSettings *settings) {
	settings->RdpKeyFile = strdup(config->rdp_key);
	if(config->tls_enabled) {
		settings->TLSMinVersion = 0x0303; //TLS1.2 number registered to the IANA
		settings->CertificateFile = strdup(config->server_cert);
		settings->PrivateKeyFile = strdup(config->server_key);
	} else {
		settings->TlsSecurity = FALSE;
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
	return config->displayMode;
}

