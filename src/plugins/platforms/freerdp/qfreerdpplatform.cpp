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

/** @brief private data for FreeRdpPlatform */
struct QFreeRdpPlatformConfig {
	/**
	 * @param parent
	 */
	QFreeRdpPlatformConfig();

	char *bind_address;
	int port;

	char *server_cert;
	char *server_key;
	char *rdp_key;
	bool tls_enabled;

	QList<QFreeRdpPeer *> peers;
	QList<QFreeRdpWindow *> windows;
};

QFreeRdpPlatformConfig::QFreeRdpPlatformConfig() :
	bind_address(0), port(3389),
	server_cert( strdup("/home/david/.freerdp/server/server.crt") ),
	server_key( strdup("/home/david/.freerdp/server/server.key") ),
	rdp_key( strdup("/home/david/.freerdp/server/server.key") ),
	tls_enabled(true)
{
}

QFreeRdpPlatform::QFreeRdpPlatform(QAbstractEventDispatcher *dispatcher) :
	mDispatcher(dispatcher),
	config(new QFreeRdpPlatformConfig()),
	mScreen(new QFreeRdpScreen(this, 640, 480)),
	mWindowManager(new QFreeRdpWindowManager(this)),
	mListener(new QFreeRdpListener(this))
{
    mListener->initialize();
}

void QFreeRdpPlatform::registerPeer(QFreeRdpPeer *peer) {
	config->peers.push_back(peer);
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
	const QImage *bits = mScreen->getScreenBits();
	foreach(QFreeRdpPeer *peer, config->peers) {
		peer->repaint(region, bits);
	}
}

QPlatformWindow *QFreeRdpPlatform::newWindow(QWindow *window) {
	QFreeRdpWindow *ret = new QFreeRdpWindow(window, this);
	config->windows.push_back(ret);
	mWindowManager->addWindow(ret);
	return ret;
}

QFreeRdpWindow *QFreeRdpPlatform::getFrontWindow() {
	return config->windows.front();
}

void QFreeRdpPlatform::configureClient(rdpSettings *settings) {
	settings->RdpKeyFile = config->rdp_key;
	if(config->tls_enabled) {
		settings->CertificateFile = config->server_cert;
		settings->PrivateKeyFile = config->server_key;
	} else {
		settings->TlsSecurity = FALSE;
	}
}




