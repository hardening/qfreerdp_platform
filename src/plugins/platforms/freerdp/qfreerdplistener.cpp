/**
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
#include "qfreerdplistener.h"
#include "qfreerdpplatform.h"
#include "qfreerdppeer.h"

#include <QDebug>

QFreeRdpListener::QFreeRdpListener(QFreeRdpPlatform *platform) :
	listener(nullptr),
	mSocketNotifier(nullptr),
	mPlatform(platform)
{
}

QFreeRdpListener::~QFreeRdpListener() {
	disconnect(mSocketNotifier, &QSocketNotifier::activated, this, &QFreeRdpListener::incomingNewPeer);
	delete mSocketNotifier;

	if (listener) {
		listener->Close(listener);
		freerdp_listener_free(listener);
	}
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

	// this function is sometimes invoked from a freerpd thread, but QSocketNotifier constructor
	// needs to be invoked on a QThread. QMetaObject::invokeMethod queues the function to be
	// called by the event loop, and more specifically the thread that owns this
	QMetaObject::invokeMethod(this, "startListener", Qt::QueuedConnection, Q_ARG(int, fd));
}

// must be invoked on the thread that owns `this`
void QFreeRdpListener::startListener(int fd) {
	mSocketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
	mPlatform->getDispatcher()->registerSocketNotifier(mSocketNotifier);

	connect(mSocketNotifier, &QSocketNotifier::activated, this, &QFreeRdpListener::incomingNewPeer);
}
