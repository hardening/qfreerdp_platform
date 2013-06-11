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

#include <QtCore/QtDebug>

/** @brief private data for FreeRdpPlatform */
struct QFreeRdpPlatformConfig {
	/**
	 * @param parent
	 */
	QFreeRdpPlatformConfig(const QStringList &params);


	char *bind_address;
	int port;

	char *server_cert;
	char *server_key;
	char *rdp_key;
	bool tls_enabled;

	QList<QFreeRdpPeer *> peers;
	QList<QFreeRdpWindow *> windows;
	QSize screenSz;
};

QFreeRdpPlatformConfig::QFreeRdpPlatformConfig(const QStringList &params) :
	bind_address(0), port(3389),
	server_cert( strdup("/home/david/.freerdp/server/server.crt") ),
	server_key( strdup("/home/david/.freerdp/server/server.key") ),
	rdp_key( strdup("/home/david/.freerdp/server/server.key") ),
	tls_enabled(true),
	screenSz(800, 600)
{
	QString subVal;
	bool ok = true;
	int val;
	for(int i = 0; i < params.size(); i++) {
		QString param = params[i];
		if(param.startsWith(QLatin1String("width="))) {
			subVal = param.mid(strlen("width="));
			val = subVal.toInt(&ok);
			if(!ok)
				qWarning() << "invalid width" << subVal;

			screenSz.setWidth(val);
		} else if(param.startsWith(QLatin1String("height="))) {
			subVal = param.mid(strlen("height="));
			val = subVal.toInt(&ok);
			if(!ok)
				qWarning() << "invalid height" << subVal;
			screenSz.setHeight(val);
		}
	}
}

QFreeRdpPlatform::QFreeRdpPlatform(const QStringList& paramList, QAbstractEventDispatcher *dispatcher) :
	mDispatcher(dispatcher)
{
	config = new QFreeRdpPlatformConfig(paramList);
	mScreen = new QFreeRdpScreen(this, config->screenSz.width(), config->screenSz.height());

	mWindowManager = new QFreeRdpWindowManager(this);
	mListener = new QFreeRdpListener(this);
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
	foreach(QFreeRdpPeer *peer, config->peers) {
		peer->repaint(region);
	}
}

QPlatformWindow *QFreeRdpPlatform::newWindow(QWindow *window) {
	QFreeRdpWindow *ret = new QFreeRdpWindow(window, this);
	config->windows.push_back(ret);
	mWindowManager->addWindow(ret);
	return ret;
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




