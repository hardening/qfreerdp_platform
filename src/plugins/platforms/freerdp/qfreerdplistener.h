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
#ifndef __QFREERDPLISTENER_H___
#define __QFREERDPLISTENER_H___

#include <freerdp/listener.h>
#include <freerdp/peer.h>

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QSocketNotifier>

class QFreeRdpPlatform;
class QFreeRdpPeer;

class QFreeRdpListener : public QObject {
	Q_OBJECT
public:
	/**
	 * @param parent
	 */
	QFreeRdpListener(QFreeRdpPlatform *parent);
	virtual ~QFreeRdpListener();

	/** */
	void initialize();

protected slots:
	void incomingNewPeer();
	void startListener(int fd);

protected:
	static BOOL rdp_incoming_peer(freerdp_listener* instance, freerdp_peer* client);

	freerdp_listener *listener;
	QSocketNotifier *mSocketNotifier;

	QFreeRdpPlatform *mPlatform;
};


#endif /* __QFREERDPLISTENER_H___ */
