/**
 * Copyright © 2013-2026 David Fort <contact@hardening-consulting.com>
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
#pragma once

#include <QObject>

#include <freerdp/server/rdpdr.h>

#include <qfreerdp.h>

class QFreeRdpPlatform;
class QFreeRdpSmartcardServicePriv;

/**
 * @brief exposes the smartcards of the RDP client as a pcscd
 *
 * The service listens on a local socket speaking the pcsc-lite client protocol
 * (applications point PCSCLITE_CSOCK_NAME to pcscdPath()) and forwards the PC/SC
 * calls to the smartcard device of the RDP client through the RDPDR channel.
 *
 * The service runs in its own thread. The RDPDR channels are owned by the peers:
 * a peer prepares its channel with setupChannel() before starting it and calls
 * detachChannel() before stopping and freeing it.
 */
class QFreeRdpSmartcardService : public QObject, public IFreeRdpSmartcardService
{
	Q_OBJECT
	Q_INTERFACES(IFreeRdpSmartcardService)

	friend class QFreeRdpSmartcardServicePriv;
public:
	QFreeRdpSmartcardService(QFreeRdpPlatform *platform);
	virtual ~QFreeRdpSmartcardService();

	/** @overload IFreeRdpSmartcardService
	 * @{ */
	bool start() override;
	QString pcscdPath() const override;
	QObject *asQObject() override;
	/** @} */

	/** registers a RDPDR channel for smartcard redirection, to call before starting it
	 * @param context the channel */
	void setupChannel(RdpdrServerContext *context);

	/** the service stops using the channel, to call before stopping and freeing it
	 * @param context the channel */
	void detachChannel(RdpdrServerContext *context);

signals:
	/** a smartcard device is available (its readers are known) */
	void smartcardChannelCreated();

	/** the smartcard device is gone */
	void smartcardChannelRemoved();

protected:
	QFreeRdpSmartcardServicePriv *mPriv;
};
