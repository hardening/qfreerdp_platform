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

#ifndef QFREERDP_H_
#define QFREERDP_H_

#include <QObject>
#include <QString>
#include <QThread>
#include <QEventLoop>
#include <QCoreApplication>

#include <functional>
#include <memory>

#define QFREERDP_SMARTCARD "smartcardService"

namespace QFreeRdp {

/**
 * @brief runs a blocking function without blocking the RDP traffic
 *
 * With the freerdp QPA, the RDP connection (and so the smartcard redirection
 * channel) is serviced by the main thread. A blocking call that needs the
 * RDP client (a pkinit, a PKCS#11 / pcsc operation) would deadlock when made
 * from the main thread. This helper runs fn in a worker thread and spins a
 * local event loop until it returns, so the call looks synchronous to the caller.
 *
 * By default user input events are not processed during the call (they are
 * delivered afterwards), which avoids re-entering the code that triggered the
 * call (a second click on the same button). Timers, sockets and painting still run.
 * Objects used by fn must outlive the call.
 *
 * When not called from the main thread, fn is simply called directly.
 */
inline void runBlocking(std::function<void()> fn,
		QEventLoop::ProcessEventsFlags flags = QEventLoop::ExcludeUserInputEvents)
{
	QCoreApplication *app = QCoreApplication::instance();
	if (!app || QThread::currentThread() != app->thread()) {
		fn();
		return;
	}

	QEventLoop loop;
	std::unique_ptr<QThread> worker(QThread::create(std::move(fn)));
	/* finished is emitted from the worker, so quit() is queued to the loop and can't be lost */
	QObject::connect(worker.get(), &QThread::finished, &loop, &QEventLoop::quit, Qt::QueuedConnection);
	worker->start();
	loop.exec(flags);
	worker->wait();
}

} // namespace QFreeRdp


/**
 * @brief access to the smartcards of the RDP client
 *
 * Obtained with the platform native interface:
 * \code
 * QObject *obj = reinterpret_cast<QObject *>(
 *     QGuiApplication::platformNativeInterface()->nativeResourceForIntegration(QFREERDP_SMARTCARD));
 * IFreeRdpSmartcardService *service = qobject_cast<IFreeRdpSmartcardService *>(obj);
 * \endcode
 *
 * The object returned by asQObject() emits the smartcardChannelCreated() signal when a
 * smartcard device of the RDP client is available and smartcardChannelRemoved() when
 * it is gone (connect them with the SIGNAL() syntax).
 *
 * This is a pure interface (not a QObject) so that qobject_cast works across the
 * plugin boundary: it only compares the interface identifier.
 */
class IFreeRdpSmartcardService {
public:
	virtual ~IFreeRdpSmartcardService() = default;

	/** starts the service (listening for pcsc-lite clients)
	 * @return if the operation was successful */
	virtual bool start() = 0;

	/** @return the socket path to put in PCSCLITE_CSOCK_NAME */
	virtual QString pcscdPath() const = 0;

	/** @return the object emitting the signals */
	virtual QObject *asQObject() = 0;
};

#define IFreeRdpSmartcardService_iid "com.hardeningconsulting.qfreerdpSmartcardService/1.0"

Q_DECLARE_INTERFACE(IFreeRdpSmartcardService, IFreeRdpSmartcardService_iid)

#endif
