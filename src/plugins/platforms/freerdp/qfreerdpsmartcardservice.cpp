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

#include "qfreerdpsmartcardservice.h"

#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QVector>
#include <QByteArray>
#include <QTimer>
#include <QLocalServer>
#include <QLocalSocket>

#include <memory>
#include <map>
#include <cstring>

#include <winpr/wlog.h>
#include <winpr/smartcard.h>
#include <freerdp/channels/scard.h>
#include <freerdp/channels/rdpdr.h>

#include <unistd.h>

#define TAG "com.qfreerdp.smartcard"

/* ===================== pcsc-lite client protocol (4.4) =====================
 * taken from pcsc-lite winscard_msg.h / pcsclite.h, these structures are sent
 * as is (host endianness) on the local socket */
namespace {

constexpr int32_t PCSC_PROTOCOL_VERSION_MAJOR = 4;
constexpr int32_t PCSC_PROTOCOL_VERSION_MINOR = 4;

constexpr size_t PCSC_MAX_READERNAME = 128;
constexpr size_t PCSC_MAX_READERS = 16;  /* PCSCLITE_MAX_READERS_CONTEXTS */
constexpr size_t PCSC_MAX_ATR_SIZE = 33;
constexpr size_t PCSC_MAX_BUFFER_SIZE = 264;
constexpr size_t PCSC_MAX_BUFFER_SIZE_EXTENDED = 4 + 3 + (1 << 16) + 3 + 2;

/* pcsc-lite reader state bits (pcsclite.h) */
constexpr uint32_t PCSC_SCARD_UNKNOWN = 0x0001;
constexpr uint32_t PCSC_SCARD_ABSENT = 0x0002;
constexpr uint32_t PCSC_SCARD_PRESENT = 0x0004;
constexpr uint32_t PCSC_SCARD_SWALLOWED = 0x0008;
constexpr uint32_t PCSC_SCARD_POWERED = 0x0010;
constexpr uint32_t PCSC_SCARD_NEGOTIABLE = 0x0020;

/* pcsc-lite protocols, T0 and T1 have the same values as on Windows */
constexpr uint32_t PCSC_PROTOCOL_RAW = 0x0004;

constexpr int32_t PCSC_SHARING_EXCLUSIVE_CONTEXT = -1;
constexpr int32_t PCSC_SHARING_NO_CONTEXT = 0;
constexpr int32_t PCSC_SHARING_LAST_CONTEXT = 1;

enum PcscCommand : uint32_t {
	SCARD_ESTABLISH_CONTEXT = 0x01,
	SCARD_RELEASE_CONTEXT = 0x02,
	SCARD_LIST_READERS = 0x03,
	SCARD_CONNECT = 0x04,
	SCARD_RECONNECT = 0x05,
	SCARD_DISCONNECT = 0x06,
	SCARD_BEGIN_TRANSACTION = 0x07,
	SCARD_END_TRANSACTION = 0x08,
	SCARD_TRANSMIT = 0x09,
	SCARD_CONTROL = 0x0A,
	SCARD_STATUS = 0x0B,
	SCARD_GET_STATUS_CHANGE = 0x0C,
	SCARD_CANCEL = 0x0D,
	SCARD_CANCEL_TRANSACTION = 0x0E,
	SCARD_GET_ATTRIB = 0x0F,
	SCARD_SET_ATTRIB = 0x10,
	CMD_VERSION = 0x11,
	CMD_GET_READERS_STATE = 0x12,
	CMD_WAIT_READER_STATE_CHANGE = 0x13,
	CMD_STOP_WAITING_READER_STATE_CHANGE = 0x14,

	/* internal requests, not part of the protocol */
	INTERNAL_MONITOR_ESTABLISH = 0x1000,
	INTERNAL_MONITOR_LIST_READERS,
	INTERNAL_MONITOR_STATUS_CHANGE,
	INTERNAL_CLEANUP,
};

struct pcsc_header {
	uint32_t size;
	uint32_t command;
};

struct pcsc_version {
	int32_t major;
	int32_t minor;
	uint32_t rv;
};

struct pcsc_wait_reader_state_change {
	uint32_t timeOut;
	uint32_t rv;
};

struct pcsc_establish {
	uint32_t dwScope;
	uint32_t hContext;
	uint32_t rv;
};

struct pcsc_release {
	uint32_t hContext;
	uint32_t rv;
};

struct pcsc_connect {
	uint32_t hContext;
	char szReader[PCSC_MAX_READERNAME];
	uint32_t dwShareMode;
	uint32_t dwPreferredProtocols;
	int32_t hCard;
	uint32_t dwActiveProtocol;
	uint32_t rv;
};

struct pcsc_reconnect {
	int32_t hCard;
	uint32_t dwShareMode;
	uint32_t dwPreferredProtocols;
	uint32_t dwInitialization;
	uint32_t dwActiveProtocol;
	uint32_t rv;
};

struct pcsc_disconnect {
	int32_t hCard;
	uint32_t dwDisposition;
	uint32_t rv;
};

struct pcsc_begin {
	int32_t hCard;
	uint32_t rv;
};

struct pcsc_end {
	int32_t hCard;
	uint32_t dwDisposition;
	uint32_t rv;
};

struct pcsc_cancel {
	int32_t hContext;
	uint32_t rv;
};

struct pcsc_status {
	int32_t hCard;
	uint32_t rv;
};

struct pcsc_transmit {
	int32_t hCard;
	uint32_t ioSendPciProtocol;
	uint32_t ioSendPciLength;
	uint32_t cbSendLength;
	uint32_t ioRecvPciProtocol;
	uint32_t ioRecvPciLength;
	uint32_t pcbRecvLength;
	uint32_t rv;
};

struct pcsc_control {
	int32_t hCard;
	uint32_t dwControlCode;
	uint32_t cbSendLength;
	uint32_t cbRecvLength;
	uint32_t dwBytesReturned;
	uint32_t rv;
};

struct pcsc_getset {
	int32_t hCard;
	uint32_t dwAttrId;
	uint8_t pbAttr[PCSC_MAX_BUFFER_SIZE];
	uint32_t cbAttrLen;
	uint32_t rv;
};

/** pubReaderStatesList of pcsc-lite */
struct pcsc_reader_state {
	char readerName[PCSC_MAX_READERNAME];
	uint32_t eventCounter;
	uint32_t readerState;
	int32_t readerSharing;
	uint8_t cardAtr[PCSC_MAX_ATR_SIZE];
	uint32_t cardAtrLength;
	uint32_t cardProtocol;
};

/** forward SCardGetAttrib / SCardSetAttrib to the RDP client
 *
 * Disabled: FreeRDP's rdpdr server encodes these calls in a fixed size stream (64 bytes,
 * 64 + cbAttrLen) that is too small with 8 bytes context and handle values, and
 * smartcard_pack_redir_scard_{context,handle}_ref() write without growing the stream:
 * this asserts (or overflows the heap without WITH_VERBOSE_WINPR_ASSERT). Meanwhile
 * SCARD_ATTR_ATR_STRING is answered from the reader state. */
constexpr bool kForwardAttributes = false;

/** the largest message body (without the transmit / control payload) */
constexpr size_t PCSC_MAX_BODY = sizeof(pcsc_getset);

/** windows SCARD_CTL_CODE() */
constexpr uint32_t winScardCtlCode(uint32_t code) {
	return (0x31u << 16) | (code << 2);
}

uint32_t pcscToWinProtocols(uint32_t protocols) {
	uint32_t ret = protocols & (SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1);
	if (protocols & PCSC_PROTOCOL_RAW)
		ret |= SCARD_PROTOCOL_RAW;
	return ret;
}

uint32_t winToPcscProtocols(uint32_t protocols) {
	uint32_t ret = protocols & (SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1);
	if (protocols & SCARD_PROTOCOL_RAW)
		ret |= PCSC_PROTOCOL_RAW;
	return ret;
}

/** control codes built with the pcsc-lite SCARD_CTL_CODE() are converted to the
 * windows encoding, the others (like the ones returned by a feature request) are
 * kept as is */
uint32_t pcscToWinControlCode(uint32_t code) {
	if ((code & 0xFF000000) == 0x42000000)
		return winScardCtlCode(code & 0x00FFFFFF);
	return code;
}

template <typename T>
QByteArray structBytes(const T &v) {
	return QByteArray(reinterpret_cast<const char *>(&v), sizeof(v));
}

template <typename T>
T structFrom(const QByteArray &b) {
	T ret;
	memset(&ret, 0, sizeof(ret));
	memcpy(&ret, b.constData(), qMin(size_t(b.size()), sizeof(ret)));
	return ret;
}

} // anonymous namespace


/** @brief the result of a smartcard call, copied from the rdpdr thread */
struct ScardReply {
	quint64 requestId = 0;
	LONG rv = SCARD_F_INTERNAL_ERROR;
	REDIR_SCARDCONTEXT hContext{};
	REDIR_SCARDHANDLE hCard{};
	UINT32 protocol = 0;
	bool hasRecvPci = false;
	UINT32 recvPciProtocol = 0;
	QByteArray data;
	QVector<ReaderState_Return> readers;
};

/** @brief a smartcard handle of a pcsc client */
struct PcscHandle {
	REDIR_SCARDHANDLE handle{};
	quint32 contextId = 0;
	QByteArray readerName;
};

/** @brief a pcsc client connected on the local socket */
struct PcscdConnection {
	quint32 id = 0;
	QLocalSocket *socket = nullptr;
	QByteArray input;
	bool busy = false;          /**< waiting for the answer of the RDP client */
	bool waitingEvent = false;  /**< registered for reader events */
	bool broken = false;        /**< write error, to close */
	QSet<quint32> contexts;
	std::map<int32_t, PcscHandle> handles;
};

/** @brief a smartcard context */
struct PcscContext {
	quint32 connectionId = 0;
	REDIR_SCARDCONTEXT hContext{};
};

/** @brief a request sent to the RDP client */
struct PendingRequest {
	quint32 connectionId = 0; /**< 0 for internal requests */
	quint32 command = 0;
	QByteArray body;          /**< the pcsc request */
	REDIR_SCARDCONTEXT hContext{};
	int32_t hCard = 0;
};


/** @brief the service internals, living in the service thread (except the channels
 * registry, protected by mChannelsLock) */
class QFreeRdpSmartcardServicePriv : public QObject {
public:
	QFreeRdpSmartcardServicePriv(QFreeRdpSmartcardService *service);
	~QFreeRdpSmartcardServicePriv() override;

	bool start();
	void shutdown();

	/** channels registry, any thread
	 * @{ */
	void registerChannel(RdpdrServerContext *context);
	quint64 unregisterChannel(RdpdrServerContext *context);
	quint64 channelId(RdpdrServerContext *context);
	/** @} */

	/** events, service thread
	 * @{ */
	void onDeviceCreated(quint64 channel);
	void onDeviceRemoved(quint64 channel);
	void onChannelGone(quint64 channel);
	void onReply(quint64 channel, const ScardReply &reply);
	/** @} */

	void postReply(RdpdrServerContext *context, const ScardReply &reply);

	QFreeRdpSmartcardService *mService;
	std::unique_ptr<QThread> mThread;
	QString mListenPath;

protected:
	void onListening();
	void onNewConnection();
	void onReadyRead(quint32 connectionId);

	PcscdConnection *connection(quint32 id);
	void processInput(PcscdConnection *c);
	bool handleCommand(PcscdConnection *c, quint32 command, const QByteArray &body,
			const QByteArray &payload);
	void handleReply(PcscdConnection *c, const PendingRequest &request, const ScardReply &reply);
	void handleOrphanReply(const PendingRequest &request, const ScardReply &reply);
	void closeConnection(quint32 id, bool cleanupRemote);
	void send(PcscdConnection *c, const QByteArray &data);
	template <typename T> void sendStruct(PcscdConnection *c, const T &v) { send(c, structBytes(v)); }

	/** sends a request to the RDP client
	 * @param c the pcsc client, nullptr for internal requests
	 * @param request the request, the connection id is filled
	 * @param call the call to the channel
	 * @return if the request was sent */
	template <typename F>
	bool sendRequest(PcscdConnection *c, PendingRequest request, F call);

	PcscHandle *findHandle(PcscdConnection *c, int32_t hCard);
	PcscContext *findContext(quint32 id);
	quint32 newContextId();
	int32_t newHandleId();

	void activateChannel(quint64 channel);
	void dropChannel();

	/** readers monitoring
	 * @{ */
	void monitorEstablish();
	void monitorListReaders();
	void monitorStatusChange();
	void monitorRetry(void (QFreeRdpSmartcardServicePriv::*step)(), int delay);
	void handleMonitorReply(const PendingRequest &request, const ScardReply &reply);
	void updateReaders(const QList<QByteArray> &names, const QVector<ReaderState_Return> &states);
	void setReaderProtocol(const QByteArray &readerName, uint32_t winProtocol);
	void signalWaitingClients(LONG rv);
	/** @} */

	QLocalServer *mServer;
	bool mStarted;

	QMutex mChannelsLock;
	quint64 mChannelCounter;
	QHash<RdpdrServerContext *, quint64> mChannelIds;
	QHash<quint64, RdpdrServerContext *> mChannels;

	quint64 mActiveChannel;    /**< the channel with a smartcard device, 0 if none */
	bool mReady;               /**< the readers of the active channel are known */

	quint32 mConnectionCounter;
	std::map<quint32, std::unique_ptr<PcscdConnection>> mConnections;
	QHash<quint32, PcscContext> mContexts;
	quint32 mContextCounter;
	int32_t mHandleCounter;

	quint64 mRequestCounter;
	QHash<quint64, PendingRequest> mPending;

	REDIR_SCARDCONTEXT mMonitorContext;
	bool mMonitorHasContext;
	QList<QByteArray> mMonitorNames;          /**< readers of the running status change */
	QVector<DWORD> mMonitorStates;            /**< last event states, by reader */
	QTimer *mMonitorTimer;
	pcsc_reader_state mReaders[PCSC_MAX_READERS];
};


/* ======================= rdpdr callbacks (rdpdr thread) ======================= */

static QFreeRdpSmartcardServicePriv *privFromContext(RdpdrServerContext *context) {
	return static_cast<QFreeRdpSmartcardServicePriv *>(context->data);
}

static ScardReply makeReply(void *callbackData, UINT32 ioStatus, LONG returnCode, bool hasRet) {
	ScardReply reply;
	reply.requestId = static_cast<quint64>(reinterpret_cast<uintptr_t>(callbackData));
	if (ioStatus != STATUS_SUCCESS)
		reply.rv = SCARD_F_COMM_ERROR;
	else if (!hasRet && returnCode == SCARD_S_SUCCESS)
		reply.rv = SCARD_F_INTERNAL_ERROR;
	else
		reply.rv = returnCode;
	return reply;
}

static void onSimpleComplete(RdpdrServerContext *context, void *callbackData, UINT32 ioStatus,
		LONG returnCode)
{
	privFromContext(context)->postReply(context, makeReply(callbackData, ioStatus, returnCode, true));
}

static void onEstablishContextComplete(RdpdrServerContext *context, void *callbackData,
		UINT32 ioStatus, LONG returnCode, const EstablishContext_Return *ret)
{
	ScardReply reply = makeReply(callbackData, ioStatus, returnCode, ret);
	if (reply.rv == SCARD_S_SUCCESS)
		reply.hContext = ret->hContext;
	privFromContext(context)->postReply(context, reply);
}

static void onListReadersComplete(RdpdrServerContext *context, void *callbackData,
		UINT32 ioStatus, LONG returnCode, const ListReaders_Return *ret)
{
	ScardReply reply = makeReply(callbackData, ioStatus, returnCode, ret);
	if (reply.rv == SCARD_S_SUCCESS && ret->msz)
		reply.data = QByteArray(reinterpret_cast<const char *>(ret->msz), ret->cBytes);
	privFromContext(context)->postReply(context, reply);
}

static void onGetStatusChangeComplete(RdpdrServerContext *context, void *callbackData,
		UINT32 ioStatus, LONG returnCode, const GetStatusChange_Return *ret)
{
	ScardReply reply = makeReply(callbackData, ioStatus, returnCode, ret);
	if (reply.rv == SCARD_S_SUCCESS && ret->rgReaderStates) {
		const UINT32 n = qMin<UINT32>(ret->cReaders, PCSC_MAX_READERS);
		for (UINT32 i = 0; i < n; i++)
			reply.readers.append(ret->rgReaderStates[i]);
	}
	privFromContext(context)->postReply(context, reply);
}

static void onConnectComplete(RdpdrServerContext *context, void *callbackData,
		UINT32 ioStatus, LONG returnCode, const Connect_Return *ret)
{
	ScardReply reply = makeReply(callbackData, ioStatus, returnCode, ret);
	if (reply.rv == SCARD_S_SUCCESS) {
		reply.hCard = ret->hCard;
		reply.protocol = ret->dwActiveProtocol;
	}
	privFromContext(context)->postReply(context, reply);
}

static void onReconnectComplete(RdpdrServerContext *context, void *callbackData,
		UINT32 ioStatus, LONG returnCode, const Reconnect_Return *ret)
{
	ScardReply reply = makeReply(callbackData, ioStatus, returnCode, ret);
	if (reply.rv == SCARD_S_SUCCESS)
		reply.protocol = ret->dwActiveProtocol;
	privFromContext(context)->postReply(context, reply);
}

static void onStatusComplete(RdpdrServerContext *context, void *callbackData,
		UINT32 ioStatus, LONG returnCode, const Status_Return *ret)
{
	ScardReply reply = makeReply(callbackData, ioStatus, returnCode, ret);
	if (reply.rv == SCARD_S_SUCCESS)
		reply.protocol = ret->dwProtocol;
	privFromContext(context)->postReply(context, reply);
}

static void onTransmitComplete(RdpdrServerContext *context, void *callbackData,
		UINT32 ioStatus, LONG returnCode, const Transmit_Return *ret)
{
	ScardReply reply = makeReply(callbackData, ioStatus, returnCode, ret);
	if (reply.rv == SCARD_S_SUCCESS) {
		if (ret->pbRecvBuffer)
			reply.data = QByteArray(reinterpret_cast<const char *>(ret->pbRecvBuffer), ret->cbRecvLength);
		if (ret->pioRecvPci) {
			reply.hasRecvPci = true;
			reply.recvPciProtocol = ret->pioRecvPci->dwProtocol;
		}
	}
	privFromContext(context)->postReply(context, reply);
}

static void onControlComplete(RdpdrServerContext *context, void *callbackData,
		UINT32 ioStatus, LONG returnCode, const Control_Return *ret)
{
	ScardReply reply = makeReply(callbackData, ioStatus, returnCode, ret);
	if (reply.rv == SCARD_S_SUCCESS && ret->pvOutBuffer)
		reply.data = QByteArray(reinterpret_cast<const char *>(ret->pvOutBuffer), ret->cbOutBufferSize);
	privFromContext(context)->postReply(context, reply);
}

static void onGetAttribComplete(RdpdrServerContext *context, void *callbackData,
		UINT32 ioStatus, LONG returnCode, const GetAttrib_Return *ret)
{
	ScardReply reply = makeReply(callbackData, ioStatus, returnCode, ret);
	if (reply.rv == SCARD_S_SUCCESS && ret->pbAttr)
		reply.data = QByteArray(reinterpret_cast<const char *>(ret->pbAttr), ret->cbAttrLen);
	privFromContext(context)->postReply(context, reply);
}

static UINT onSmartcardCreate(RdpdrServerContext *context, const RdpdrDevice *device)
{
	WINPR_UNUSED(device);
	QFreeRdpSmartcardServicePriv *priv = privFromContext(context);
	const quint64 channel = priv->channelId(context);
	if (channel)
		QMetaObject::invokeMethod(priv, [priv, channel] { priv->onDeviceCreated(channel); }, Qt::QueuedConnection);
	return CHANNEL_RC_OK;
}

static UINT onSmartcardDelete(RdpdrServerContext *context, UINT32 deviceId)
{
	WINPR_UNUSED(deviceId);
	QFreeRdpSmartcardServicePriv *priv = privFromContext(context);
	const quint64 channel = priv->channelId(context);
	if (channel)
		QMetaObject::invokeMethod(priv, [priv, channel] { priv->onDeviceRemoved(channel); }, Qt::QueuedConnection);
	return CHANNEL_RC_OK;
}


/* ============================== public service ============================== */

QFreeRdpSmartcardService::QFreeRdpSmartcardService(QFreeRdpPlatform *platform)
: mPriv(new QFreeRdpSmartcardServicePriv(this))
{
	WINPR_UNUSED(platform);
}

QFreeRdpSmartcardService::~QFreeRdpSmartcardService()
{
	mPriv->shutdown();
	delete mPriv;
}

bool QFreeRdpSmartcardService::start()
{
	return mPriv->start();
}

QString QFreeRdpSmartcardService::pcscdPath() const
{
	return mPriv->mListenPath;
}

QObject *QFreeRdpSmartcardService::asQObject()
{
	return this;
}

void QFreeRdpSmartcardService::setupChannel(RdpdrServerContext *context)
{
	context->data = mPriv;
	context->supported = RDPDR_DTYP_SMARTCARD;
	context->OnSmartcardCreate = onSmartcardCreate;
	context->OnSmartcardDelete = onSmartcardDelete;

	context->OnSmartcardEstablishContextComplete = onEstablishContextComplete;
	context->OnSmartcardReleaseContextComplete = onSimpleComplete;
	context->OnSmartcardListReadersComplete = onListReadersComplete;
	context->OnSmartcardGetStatusChangeComplete = onGetStatusChangeComplete;
	context->OnSmartcardConnectComplete = onConnectComplete;
	context->OnSmartcardReconnectComplete = onReconnectComplete;
	context->OnSmartcardDisconnectComplete = onSimpleComplete;
	context->OnSmartcardBeginTransactionComplete = onSimpleComplete;
	context->OnSmartcardEndTransactionComplete = onSimpleComplete;
	context->OnSmartcardStatusComplete = onStatusComplete;
	context->OnSmartcardTransmitComplete = onTransmitComplete;
	context->OnSmartcardControlComplete = onControlComplete;
	context->OnSmartcardGetAttribComplete = onGetAttribComplete;
	context->OnSmartcardSetAttribComplete = onSimpleComplete;

	mPriv->registerChannel(context);
}

void QFreeRdpSmartcardService::detachChannel(RdpdrServerContext *context)
{
	const quint64 channel = mPriv->unregisterChannel(context);
	if (channel) {
		QFreeRdpSmartcardServicePriv *priv = mPriv;
		QMetaObject::invokeMethod(priv, [priv, channel] { priv->onChannelGone(channel); }, Qt::QueuedConnection);
	}
}


/* ============================== service internals ============================== */

QFreeRdpSmartcardServicePriv::QFreeRdpSmartcardServicePriv(QFreeRdpSmartcardService *service)
: mService(service)
, mThread(new QThread)
, mListenPath(QString("/tmp/qfreerdp.pcscd.%1").arg(getpid()))
, mServer(nullptr)
, mStarted(false)
, mChannelCounter(0)
, mActiveChannel(0)
, mReady(false)
, mConnectionCounter(0)
, mContextCounter(0)
, mHandleCounter(0)
, mRequestCounter(0)
, mMonitorContext{}
, mMonitorHasContext(false)
, mMonitorTimer(nullptr)
{
	memset(mReaders, 0, sizeof(mReaders));
	mThread->setObjectName("smartcard service");
	connect(mThread.get(), &QThread::started, this, &QFreeRdpSmartcardServicePriv::onListening);
	moveToThread(mThread.get());
}

QFreeRdpSmartcardServicePriv::~QFreeRdpSmartcardServicePriv()
{
}

bool QFreeRdpSmartcardServicePriv::start()
{
	/* called from the application, in the main thread */
	if (!mStarted) {
		mStarted = true;
		mThread->start();
	}
	return true;
}

void QFreeRdpSmartcardServicePriv::shutdown()
{
	auto cleanup = [this] {
		mConnections.clear();
		delete mServer;
		mServer = nullptr;
		delete mMonitorTimer;
		mMonitorTimer = nullptr;
	};

	if (mThread->isRunning()) {
		QMetaObject::invokeMethod(this, cleanup, Qt::BlockingQueuedConnection);
		mThread->quit();
		mThread->wait();
	} else {
		cleanup();
	}
}

void QFreeRdpSmartcardServicePriv::registerChannel(RdpdrServerContext *context)
{
	QMutexLocker locker(&mChannelsLock);
	const quint64 id = ++mChannelCounter;
	mChannelIds.insert(context, id);
	mChannels.insert(id, context);
}

quint64 QFreeRdpSmartcardServicePriv::unregisterChannel(RdpdrServerContext *context)
{
	QMutexLocker locker(&mChannelsLock);
	const quint64 id = mChannelIds.take(context);
	mChannels.remove(id);
	return id;
}

quint64 QFreeRdpSmartcardServicePriv::channelId(RdpdrServerContext *context)
{
	QMutexLocker locker(&mChannelsLock);
	return mChannelIds.value(context, 0);
}

void QFreeRdpSmartcardServicePriv::postReply(RdpdrServerContext *context, const ScardReply &reply)
{
	const quint64 channel = channelId(context);
	if (!channel)
		return;

	QMetaObject::invokeMethod(this, [this, channel, reply] { onReply(channel, reply); },
			Qt::QueuedConnection);
}

template <typename F>
bool QFreeRdpSmartcardServicePriv::sendRequest(PcscdConnection *c, PendingRequest request, F call)
{
	const quint64 requestId = ++mRequestCounter;
	void *callbackData = reinterpret_cast<void *>(static_cast<uintptr_t>(requestId));

	{
		/* the lock keeps the channel alive during the call */
		QMutexLocker locker(&mChannelsLock);
		RdpdrServerContext *context = mChannels.value(mActiveChannel, nullptr);
		if (!context)
			return false;

		UINT32 completionId = 0;
		if (call(context, callbackData, &completionId) != CHANNEL_RC_OK)
			return false;
	}

	request.connectionId = c ? c->id : 0;
	mPending.insert(requestId, request);
	if (c)
		c->busy = true;
	return true;
}


/* ----------------------------- local socket ----------------------------- */

void QFreeRdpSmartcardServicePriv::onListening()
{
	mMonitorTimer = new QTimer();
	mMonitorTimer->setSingleShot(true);

	mServer = new QLocalServer();
	connect(mServer, &QLocalServer::newConnection, this, &QFreeRdpSmartcardServicePriv::onNewConnection);

	WLog_DBG(TAG, "fake pcscd listening on %s", mListenPath.toLocal8Bit().constData());
	if (!mServer->listen(mListenPath))
		WLog_ERR(TAG, "unable to listen on %s: %s", mListenPath.toLocal8Bit().constData(),
				mServer->errorString().toLocal8Bit().constData());
}

void QFreeRdpSmartcardServicePriv::onNewConnection()
{
	while (QLocalSocket *socket = mServer->nextPendingConnection()) {
		auto c = std::make_unique<PcscdConnection>();
		c->id = ++mConnectionCounter;
		c->socket = socket;

		const quint32 id = c->id;
		connect(socket, &QLocalSocket::readyRead, this, [this, id] { onReadyRead(id); });
		connect(socket, &QLocalSocket::disconnected, this, [this, id] { closeConnection(id, true); });

		WLog_DBG(TAG, "pcsc client %u connected", id);
		mConnections[id] = std::move(c);
	}
}

PcscdConnection *QFreeRdpSmartcardServicePriv::connection(quint32 id)
{
	auto it = mConnections.find(id);
	return (it != mConnections.end()) ? it->second.get() : nullptr;
}

void QFreeRdpSmartcardServicePriv::onReadyRead(quint32 connectionId)
{
	PcscdConnection *c = connection(connectionId);
	if (!c)
		return;

	c->input += c->socket->readAll();

	/* a valid client never has more than one request in flight */
	if (size_t(c->input.size()) > sizeof(pcsc_header) + PCSC_MAX_BODY + PCSC_MAX_BUFFER_SIZE_EXTENDED) {
		WLog_ERR(TAG, "pcsc client %u: too much data", connectionId);
		closeConnection(connectionId, true);
		return;
	}

	processInput(c);
}

void QFreeRdpSmartcardServicePriv::send(PcscdConnection *c, const QByteArray &data)
{
	if (c->broken)
		return;

	if (c->socket->write(data) != data.size()) {
		WLog_ERR(TAG, "pcsc client %u: error writing", c->id);
		c->broken = true;
	}
}

void QFreeRdpSmartcardServicePriv::processInput(PcscdConnection *c)
{
	const quint32 id = c->id;

	while (!c->busy && !c->broken) {
		pcsc_header header;
		if (size_t(c->input.size()) < sizeof(header))
			break;
		memcpy(&header, c->input.constData(), sizeof(header));

		if (header.size > PCSC_MAX_BODY) {
			WLog_ERR(TAG, "pcsc client %u: invalid message size %u", id, header.size);
			c->broken = true;
			break;
		}

		size_t needed = sizeof(header) + header.size;
		if (size_t(c->input.size()) < needed)
			break;

		/* transmit and control send their data after the structure */
		size_t payloadLen = 0;
		if (header.command == SCARD_TRANSMIT && header.size == sizeof(pcsc_transmit))
			payloadLen = structFrom<pcsc_transmit>(c->input.mid(sizeof(header), header.size)).cbSendLength;
		else if (header.command == SCARD_CONTROL && header.size == sizeof(pcsc_control))
			payloadLen = structFrom<pcsc_control>(c->input.mid(sizeof(header), header.size)).cbSendLength;

		if (payloadLen > PCSC_MAX_BUFFER_SIZE_EXTENDED) {
			WLog_ERR(TAG, "pcsc client %u: invalid payload size %zu", id, payloadLen);
			c->broken = true;
			break;
		}

		if (size_t(c->input.size()) < needed + payloadLen)
			break;

		const QByteArray body = c->input.mid(sizeof(header), header.size);
		const QByteArray payload = c->input.mid(needed, payloadLen);
		c->input.remove(0, needed + payloadLen);

		if (!handleCommand(c, header.command, body, payload))
			c->broken = true;
	}

	if (c->broken)
		closeConnection(id, true);
}

PcscHandle *QFreeRdpSmartcardServicePriv::findHandle(PcscdConnection *c, int32_t hCard)
{
	auto it = c->handles.find(hCard);
	return (it != c->handles.end()) ? &it->second : nullptr;
}

PcscContext *QFreeRdpSmartcardServicePriv::findContext(quint32 id)
{
	auto it = mContexts.find(id);
	return (it != mContexts.end()) ? &it.value() : nullptr;
}

quint32 QFreeRdpSmartcardServicePriv::newContextId()
{
	do {
		mContextCounter++;
	} while (!mContextCounter || mContexts.contains(mContextCounter));
	return mContextCounter;
}

int32_t QFreeRdpSmartcardServicePriv::newHandleId()
{
	mHandleCounter = (mHandleCounter == INT32_MAX) ? 1 : mHandleCounter + 1;
	return mHandleCounter;
}

#define CHECK_BODY_SIZE(TYPE) \
	do { \
		if (size_t(body.size()) != sizeof(TYPE)) { \
			WLog_ERR(TAG, "pcsc client %u: invalid %s size %d", c->id, #TYPE, (int)body.size()); \
			return false; \
		} \
	} while (0)

#define CHECK_EMPTY_BODY() \
	do { \
		if (!body.isEmpty()) { \
			WLog_ERR(TAG, "pcsc client %u: unexpected body for command 0x%x", c->id, command); \
			return false; \
		} \
	} while (0)

bool QFreeRdpSmartcardServicePriv::handleCommand(PcscdConnection *c, quint32 command,
		const QByteArray &body, const QByteArray &payload)
{
	PendingRequest request;
	request.command = command;
	request.body = body;

	switch (command) {
	case CMD_VERSION: {
		CHECK_BODY_SIZE(pcsc_version);
		pcsc_version version = structFrom<pcsc_version>(body);
		WLog_DBG(TAG, "pcsc client %u: protocol %d.%d", c->id, version.major, version.minor);

		version.rv = SCARD_S_SUCCESS;
		if (version.major != PCSC_PROTOCOL_VERSION_MAJOR || version.minor != PCSC_PROTOCOL_VERSION_MINOR) {
			WLog_ERR(TAG, "pcsc client %u: unsupported protocol %d.%d", c->id, version.major, version.minor);
			version.rv = SCARD_E_SERVICE_STOPPED;
		}
		version.major = PCSC_PROTOCOL_VERSION_MAJOR;
		version.minor = PCSC_PROTOCOL_VERSION_MINOR;
		sendStruct(c, version);
		return true;
	}

	case CMD_GET_READERS_STATE:
		CHECK_EMPTY_BODY();
		send(c, QByteArray(reinterpret_cast<const char *>(mReaders), sizeof(mReaders)));
		return true;

	case CMD_WAIT_READER_STATE_CHANGE:
		/* registers for the next event and sends the current states */
		CHECK_EMPTY_BODY();
		c->waitingEvent = true;
		send(c, QByteArray(reinterpret_cast<const char *>(mReaders), sizeof(mReaders)));
		return true;

	case CMD_STOP_WAITING_READER_STATE_CHANGE: {
		CHECK_EMPTY_BODY();
		/* when the event has already been sent, the client reads it as the answer */
		if (c->waitingEvent) {
			c->waitingEvent = false;
			pcsc_wait_reader_state_change wait = { 0, SCARD_S_SUCCESS };
			sendStruct(c, wait);
		}
		return true;
	}

	case SCARD_ESTABLISH_CONTEXT: {
		CHECK_BODY_SIZE(pcsc_establish);
		pcsc_establish establish = structFrom<pcsc_establish>(body);

		LONG rv = SCARD_S_SUCCESS;
		if (establish.dwScope != SCARD_SCOPE_USER && establish.dwScope != SCARD_SCOPE_TERMINAL &&
				establish.dwScope != SCARD_SCOPE_SYSTEM)
			rv = SCARD_E_INVALID_VALUE;
		else if (!sendRequest(c, request, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
					return ctx->SmartcardEstablishContext(ctx, cb, establish.dwScope, cid);
				}))
			rv = SCARD_E_NO_SERVICE;

		if (rv != SCARD_S_SUCCESS) {
			establish.hContext = 0;
			establish.rv = rv;
			sendStruct(c, establish);
		}
		return true;
	}

	case SCARD_RELEASE_CONTEXT: {
		CHECK_BODY_SIZE(pcsc_release);
		pcsc_release release = structFrom<pcsc_release>(body);

		PcscContext *context = c->contexts.contains(release.hContext) ? findContext(release.hContext) : nullptr;
		if (!context) {
			release.rv = SCARD_E_INVALID_HANDLE;
			sendStruct(c, release);
			return true;
		}

		/* forget the context and its handles whatever the answer is */
		request.hContext = context->hContext;
		mContexts.remove(release.hContext);
		c->contexts.remove(release.hContext);
		for (auto it = c->handles.begin(); it != c->handles.end(); ) {
			if (it->second.contextId == release.hContext)
				it = c->handles.erase(it);
			else
				++it;
		}

		if (!sendRequest(c, request, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
					return ctx->SmartcardReleaseContext(ctx, cb, &request.hContext, cid);
				}))
		{
			release.rv = SCARD_E_NO_SERVICE;
			sendStruct(c, release);
		}
		return true;
	}

	case SCARD_CANCEL: {
		CHECK_BODY_SIZE(pcsc_cancel);
		pcsc_cancel cancel = structFrom<pcsc_cancel>(body);

		/* SCardCancel() comes from another connection than the cancelled context */
		PcscContext *context = findContext(static_cast<quint32>(cancel.hContext));
		cancel.rv = context ? SCARD_S_SUCCESS : SCARD_E_INVALID_HANDLE;
		if (context) {
			PcscdConnection *target = connection(context->connectionId);
			if (target && target->waitingEvent) {
				target->waitingEvent = false;
				pcsc_wait_reader_state_change wait = { 0, static_cast<uint32_t>(SCARD_E_CANCELLED) };
				sendStruct(target, wait);
			}
		}
		sendStruct(c, cancel);
		return true;
	}

	case SCARD_CONNECT: {
		CHECK_BODY_SIZE(pcsc_connect);
		pcsc_connect connectS = structFrom<pcsc_connect>(body);
		connectS.szReader[sizeof(connectS.szReader) - 1] = 0;
		request.body = structBytes(connectS);

		PcscContext *context = c->contexts.contains(connectS.hContext) ? findContext(connectS.hContext) : nullptr;
		LONG rv = SCARD_S_SUCCESS;
		if (!context) {
			rv = SCARD_E_INVALID_HANDLE;
		} else {
			request.hContext = context->hContext;

			ConnectA_Call call = {};
			call.szReader = connectS.szReader;
			call.Common.handles.hContext = context->hContext;
			call.Common.dwShareMode = connectS.dwShareMode;
			call.Common.dwPreferredProtocols = pcscToWinProtocols(connectS.dwPreferredProtocols);
			if (!sendRequest(c, request, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
						return ctx->SmartcardConnectA(ctx, cb, &call, cid);
					}))
				rv = SCARD_E_NO_SERVICE;
		}

		if (rv != SCARD_S_SUCCESS) {
			connectS.rv = rv;
			sendStruct(c, connectS);
		}
		return true;
	}

	case SCARD_RECONNECT: {
		CHECK_BODY_SIZE(pcsc_reconnect);
		pcsc_reconnect reconnect = structFrom<pcsc_reconnect>(body);

		PcscHandle *handle = findHandle(c, reconnect.hCard);
		PcscContext *context = handle ? findContext(handle->contextId) : nullptr;
		LONG rv = SCARD_S_SUCCESS;
		if (!context) {
			rv = SCARD_E_INVALID_HANDLE;
		} else {
			request.hCard = reconnect.hCard;

			Reconnect_Call call = {};
			call.handles.hContext = context->hContext;
			call.handles.hCard = handle->handle;
			call.dwShareMode = reconnect.dwShareMode;
			call.dwPreferredProtocols = pcscToWinProtocols(reconnect.dwPreferredProtocols);
			call.dwInitialization = reconnect.dwInitialization;
			if (!sendRequest(c, request, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
						return ctx->SmartcardReconnect(ctx, cb, &call, cid);
					}))
				rv = SCARD_E_NO_SERVICE;
		}

		if (rv != SCARD_S_SUCCESS) {
			reconnect.rv = rv;
			sendStruct(c, reconnect);
		}
		return true;
	}

	case SCARD_DISCONNECT:
	case SCARD_BEGIN_TRANSACTION:
	case SCARD_END_TRANSACTION: {
		int32_t hCard = 0;
		DWORD disposition = SCARD_LEAVE_CARD;
		if (command == SCARD_DISCONNECT) {
			CHECK_BODY_SIZE(pcsc_disconnect);
			pcsc_disconnect s = structFrom<pcsc_disconnect>(body);
			hCard = s.hCard;
			disposition = s.dwDisposition;
		} else if (command == SCARD_BEGIN_TRANSACTION) {
			CHECK_BODY_SIZE(pcsc_begin);
			hCard = structFrom<pcsc_begin>(body).hCard;
		} else {
			CHECK_BODY_SIZE(pcsc_end);
			pcsc_end s = structFrom<pcsc_end>(body);
			hCard = s.hCard;
			disposition = s.dwDisposition;
		}

		PcscHandle *handle = findHandle(c, hCard);
		PcscContext *context = handle ? findContext(handle->contextId) : nullptr;
		LONG rv = SCARD_S_SUCCESS;
		if (!context) {
			rv = SCARD_E_INVALID_HANDLE;
		} else {
			request.hCard = hCard;

			HCardAndDisposition_Call call = {};
			call.handles.hContext = context->hContext;
			call.handles.hCard = handle->handle;
			call.dwDisposition = disposition;
			const bool sent = sendRequest(c, request, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
				switch (command) {
				case SCARD_DISCONNECT:
					return ctx->SmartcardDisconnect(ctx, cb, &call, cid);
				case SCARD_BEGIN_TRANSACTION:
					return ctx->SmartcardBeginTransaction(ctx, cb, &call, cid);
				default:
					return ctx->SmartcardEndTransaction(ctx, cb, &call, cid);
				}
			});
			if (!sent)
				rv = SCARD_E_NO_SERVICE;
		}

		if (rv != SCARD_S_SUCCESS) {
			/* rv is at the end of all these structures */
			QByteArray answer = body;
			const uint32_t urv = static_cast<uint32_t>(rv);
			memcpy(answer.data() + answer.size() - sizeof(urv), &urv, sizeof(urv));
			send(c, answer);
		}
		return true;
	}

	case SCARD_STATUS: {
		CHECK_BODY_SIZE(pcsc_status);
		pcsc_status status = structFrom<pcsc_status>(body);

		PcscHandle *handle = findHandle(c, status.hCard);
		PcscContext *context = handle ? findContext(handle->contextId) : nullptr;
		LONG rv = SCARD_S_SUCCESS;
		if (!context) {
			rv = SCARD_E_INVALID_HANDLE;
		} else {
			request.hCard = status.hCard;

			Status_Call call = {};
			call.handles.hContext = context->hContext;
			call.handles.hCard = handle->handle;
			call.fmszReaderNamesIsNULL = FALSE;
			call.cchReaderLen = SCARD_AUTOALLOCATE;
			call.cbAtrLen = 32;
			if (!sendRequest(c, request, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
						return ctx->SmartcardStatusA(ctx, cb, &call, cid);
					}))
				rv = SCARD_E_NO_SERVICE;
		}

		if (rv != SCARD_S_SUCCESS) {
			status.rv = rv;
			sendStruct(c, status);
		}
		return true;
	}

	case SCARD_TRANSMIT: {
		CHECK_BODY_SIZE(pcsc_transmit);
		pcsc_transmit transmit = structFrom<pcsc_transmit>(body);

		PcscHandle *handle = findHandle(c, transmit.hCard);
		PcscContext *context = handle ? findContext(handle->contextId) : nullptr;
		LONG rv = SCARD_S_SUCCESS;
		if (!context) {
			rv = SCARD_E_INVALID_HANDLE;
		} else {
			request.hCard = transmit.hCard;

			SCARD_IO_REQUEST sendPci = {};
			sendPci.dwProtocol = pcscToWinProtocols(transmit.ioSendPciProtocol);
			sendPci.cbPciLength = sizeof(SCARD_IO_REQUEST);

			Transmit_Call call = {};
			call.handles.hContext = context->hContext;
			call.handles.hCard = handle->handle;
			call.pioSendPci = &sendPci;
			call.cbSendLength = payload.size();
			call.pbSendBuffer = reinterpret_cast<BYTE *>(const_cast<char *>(payload.constData()));
			call.pioRecvPci = nullptr;
			call.fpbRecvBufferIsNULL = FALSE;
			/* like pcscd, ask for the largest answer and compare with the client buffer
			 * afterwards (some smartcard stacks truncate silently) */
			call.cbRecvLength = PCSC_MAX_BUFFER_SIZE_EXTENDED;
			if (!sendRequest(c, request, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
						return ctx->SmartcardTransmit(ctx, cb, &call, cid);
					}))
				rv = SCARD_E_NO_SERVICE;
		}

		if (rv != SCARD_S_SUCCESS) {
			transmit.rv = rv;
			transmit.pcbRecvLength = 0;
			sendStruct(c, transmit);
		}
		return true;
	}

	case SCARD_CONTROL: {
		CHECK_BODY_SIZE(pcsc_control);
		pcsc_control control = structFrom<pcsc_control>(body);

		PcscHandle *handle = findHandle(c, control.hCard);
		PcscContext *context = handle ? findContext(handle->contextId) : nullptr;
		LONG rv = SCARD_S_SUCCESS;
		if (!context) {
			rv = SCARD_E_INVALID_HANDLE;
		} else {
			request.hCard = control.hCard;

			Control_Call call = {};
			call.handles.hContext = context->hContext;
			call.handles.hCard = handle->handle;
			call.dwControlCode = pcscToWinControlCode(control.dwControlCode);
			call.cbInBufferSize = payload.size();
			call.pvInBuffer = reinterpret_cast<BYTE *>(const_cast<char *>(payload.constData()));
			call.fpvOutBufferIsNULL = FALSE;
			call.cbOutBufferSize = PCSC_MAX_BUFFER_SIZE_EXTENDED;
			if (!sendRequest(c, request, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
						return ctx->SmartcardControl(ctx, cb, &call, cid);
					}))
				rv = SCARD_E_NO_SERVICE;
		}

		if (rv != SCARD_S_SUCCESS) {
			control.rv = rv;
			control.dwBytesReturned = 0;
			sendStruct(c, control);
		}
		return true;
	}

	case SCARD_GET_ATTRIB:
	case SCARD_SET_ATTRIB: {
		CHECK_BODY_SIZE(pcsc_getset);
		pcsc_getset getset = structFrom<pcsc_getset>(body);
		if (getset.cbAttrLen > sizeof(getset.pbAttr)) {
			WLog_ERR(TAG, "pcsc client %u: invalid attribute length %u", c->id, getset.cbAttrLen);
			return false;
		}

		PcscHandle *handle = findHandle(c, getset.hCard);
		PcscContext *context = handle ? findContext(handle->contextId) : nullptr;
		LONG rv = SCARD_S_SUCCESS;
		if (!context) {
			rv = SCARD_E_INVALID_HANDLE;
		} else if (!kForwardAttributes) {
			/* the ATR is known from the reader state, the other attributes are not available */
			rv = SCARD_E_UNSUPPORTED_FEATURE;
			if (command == SCARD_GET_ATTRIB && getset.dwAttrId == SCARD_ATTR_ATR_STRING) {
				for (const pcsc_reader_state &r : mReaders) {
					if (!r.readerName[0] || handle->readerName != r.readerName)
						continue;
					if (!(r.readerState & PCSC_SCARD_PRESENT))
						rv = SCARD_W_REMOVED_CARD;
					else if (r.cardAtrLength > getset.cbAttrLen)
						rv = SCARD_E_INSUFFICIENT_BUFFER;
					else {
						memcpy(getset.pbAttr, r.cardAtr, r.cardAtrLength);
						rv = SCARD_S_SUCCESS;
					}
					getset.cbAttrLen = r.cardAtrLength;
					break;
				}
			}

			getset.rv = rv;
			if (rv != SCARD_S_SUCCESS && rv != SCARD_E_INSUFFICIENT_BUFFER)
				getset.cbAttrLen = 0;
			sendStruct(c, getset);
			return true;
		} else {
			request.hCard = getset.hCard;

			bool sent;
			if (command == SCARD_GET_ATTRIB) {
				GetAttrib_Call call = {};
				call.handles.hContext = context->hContext;
				call.handles.hCard = handle->handle;
				call.dwAttrId = getset.dwAttrId;
				call.fpbAttrIsNULL = FALSE;
				call.cbAttrLen = getset.cbAttrLen;
				sent = sendRequest(c, request, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
					return ctx->SmartcardGetAttrib(ctx, cb, &call, cid);
				});
			} else {
				SetAttrib_Call call = {};
				call.handles.hContext = context->hContext;
				call.handles.hCard = handle->handle;
				call.dwAttrId = getset.dwAttrId;
				call.cbAttrLen = getset.cbAttrLen;
				call.pbAttr = getset.pbAttr;
				sent = sendRequest(c, request, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
					return ctx->SmartcardSetAttrib(ctx, cb, &call, cid);
				});
			}
			if (!sent)
				rv = SCARD_E_NO_SERVICE;
		}

		if (rv != SCARD_S_SUCCESS) {
			getset.rv = rv;
			if (command == SCARD_GET_ATTRIB)
				getset.cbAttrLen = 0;
			sendStruct(c, getset);
		}
		return true;
	}

	default:
		/* includes the commands that pcsc-lite doesn't use anymore, pcscd also
		 * drops the client in that case */
		WLog_ERR(TAG, "pcsc client %u: unsupported command 0x%x", c->id, command);
		return false;
	}
}

void QFreeRdpSmartcardServicePriv::onReply(quint64 channel, const ScardReply &reply)
{
	auto it = mPending.find(reply.requestId);
	if (it == mPending.end())
		return;

	const PendingRequest request = it.value();
	mPending.erase(it);

	if (channel != mActiveChannel)
		return;

	if (!request.connectionId) {
		handleMonitorReply(request, reply);
		return;
	}

	PcscdConnection *c = connection(request.connectionId);
	if (!c) {
		handleOrphanReply(request, reply);
		return;
	}

	c->busy = false;
	handleReply(c, request, reply);
	processInput(c);
}

void QFreeRdpSmartcardServicePriv::handleReply(PcscdConnection *c, const PendingRequest &request,
		const ScardReply &reply)
{
	const uint32_t rv = static_cast<uint32_t>(reply.rv);

	switch (request.command) {
	case SCARD_ESTABLISH_CONTEXT: {
		pcsc_establish establish = structFrom<pcsc_establish>(request.body);
		establish.rv = rv;
		establish.hContext = 0;
		if (reply.rv == SCARD_S_SUCCESS) {
			const quint32 id = newContextId();
			PcscContext context;
			context.connectionId = c->id;
			context.hContext = reply.hContext;
			mContexts.insert(id, context);
			c->contexts.insert(id);
			establish.hContext = id;
		}
		sendStruct(c, establish);
		break;
	}

	case SCARD_RELEASE_CONTEXT: {
		pcsc_release release = structFrom<pcsc_release>(request.body);
		release.rv = rv;
		sendStruct(c, release);
		break;
	}

	case SCARD_CONNECT: {
		pcsc_connect connectS = structFrom<pcsc_connect>(request.body);
		connectS.rv = rv;
		connectS.hCard = 0;
		connectS.dwActiveProtocol = 0;

		if (reply.rv == SCARD_S_SUCCESS) {
			/* the context may have been released by another thread of the client */
			if (c->contexts.contains(connectS.hContext)) {
				PcscHandle handle;
				handle.handle = reply.hCard;
				handle.contextId = connectS.hContext;
				handle.readerName = QByteArray(connectS.szReader);

				connectS.hCard = newHandleId();
				connectS.dwActiveProtocol = winToPcscProtocols(reply.protocol);
				c->handles[connectS.hCard] = handle;
				setReaderProtocol(handle.readerName, reply.protocol);
			} else {
				connectS.rv = static_cast<uint32_t>(SCARD_E_INVALID_HANDLE);
			}
		}
		sendStruct(c, connectS);
		break;
	}

	case SCARD_RECONNECT: {
		pcsc_reconnect reconnect = structFrom<pcsc_reconnect>(request.body);
		reconnect.rv = rv;
		reconnect.dwActiveProtocol = 0;
		if (reply.rv == SCARD_S_SUCCESS) {
			reconnect.dwActiveProtocol = winToPcscProtocols(reply.protocol);
			if (PcscHandle *handle = findHandle(c, request.hCard))
				setReaderProtocol(handle->readerName, reply.protocol);
		}
		sendStruct(c, reconnect);
		break;
	}

	case SCARD_DISCONNECT:
	case SCARD_BEGIN_TRANSACTION:
	case SCARD_END_TRANSACTION: {
		if (request.command == SCARD_DISCONNECT && reply.rv == SCARD_S_SUCCESS)
			c->handles.erase(request.hCard);

		QByteArray answer = request.body;
		memcpy(answer.data() + answer.size() - sizeof(rv), &rv, sizeof(rv));
		send(c, answer);
		break;
	}

	case SCARD_STATUS: {
		pcsc_status status = structFrom<pcsc_status>(request.body);
		status.rv = rv;
		if (reply.rv == SCARD_S_SUCCESS) {
			/* the client takes the protocol from the reader states */
			if (PcscHandle *handle = findHandle(c, request.hCard))
				setReaderProtocol(handle->readerName, reply.protocol);
		}
		sendStruct(c, status);
		break;
	}

	case SCARD_TRANSMIT: {
		pcsc_transmit transmit = structFrom<pcsc_transmit>(request.body);
		transmit.rv = rv;
		if (reply.rv == SCARD_S_SUCCESS) {
			if (uint32_t(reply.data.size()) > transmit.pcbRecvLength)
				transmit.rv = static_cast<uint32_t>(SCARD_E_INSUFFICIENT_BUFFER);
			transmit.pcbRecvLength = reply.data.size();
			if (reply.hasRecvPci)
				transmit.ioRecvPciProtocol = winToPcscProtocols(reply.recvPciProtocol);
		} else {
			transmit.pcbRecvLength = 0;
		}

		sendStruct(c, transmit);
		if (transmit.rv == SCARD_S_SUCCESS)
			send(c, reply.data);
		break;
	}

	case SCARD_CONTROL: {
		pcsc_control control = structFrom<pcsc_control>(request.body);
		control.rv = rv;
		control.dwBytesReturned = 0;
		if (reply.rv == SCARD_S_SUCCESS) {
			if (uint32_t(reply.data.size()) > control.cbRecvLength)
				control.rv = static_cast<uint32_t>(SCARD_E_INSUFFICIENT_BUFFER);
			control.dwBytesReturned = reply.data.size();
		}

		sendStruct(c, control);
		if (control.rv == SCARD_S_SUCCESS)
			send(c, reply.data);
		break;
	}

	case SCARD_GET_ATTRIB: {
		pcsc_getset getset = structFrom<pcsc_getset>(request.body);
		getset.rv = rv;
		if (reply.rv == SCARD_S_SUCCESS) {
			getset.cbAttrLen = reply.data.size();
			if (size_t(reply.data.size()) > sizeof(getset.pbAttr))
				getset.rv = static_cast<uint32_t>(SCARD_E_INSUFFICIENT_BUFFER);
			else
				memcpy(getset.pbAttr, reply.data.constData(), reply.data.size());
		} else {
			getset.cbAttrLen = 0;
		}
		sendStruct(c, getset);
		break;
	}

	case SCARD_SET_ATTRIB: {
		pcsc_getset getset = structFrom<pcsc_getset>(request.body);
		getset.rv = rv;
		sendStruct(c, getset);
		break;
	}

	default:
		WLog_ERR(TAG, "unexpected reply for command 0x%x", request.command);
		break;
	}
}

void QFreeRdpSmartcardServicePriv::handleOrphanReply(const PendingRequest &request,
		const ScardReply &reply)
{
	/* the client is gone: free what the RDP client has just created for it */
	if (reply.rv != SCARD_S_SUCCESS)
		return;

	PendingRequest cleanup;
	cleanup.command = INTERNAL_CLEANUP;

	if (request.command == SCARD_ESTABLISH_CONTEXT) {
		const REDIR_SCARDCONTEXT hContext = reply.hContext;
		sendRequest(nullptr, cleanup, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
			return ctx->SmartcardReleaseContext(ctx, cb, &hContext, cid);
		});
	} else if (request.command == SCARD_CONNECT) {
		HCardAndDisposition_Call call = {};
		call.handles.hContext = request.hContext;
		call.handles.hCard = reply.hCard;
		call.dwDisposition = SCARD_LEAVE_CARD;
		sendRequest(nullptr, cleanup, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
			return ctx->SmartcardDisconnect(ctx, cb, &call, cid);
		});
	}
}

void QFreeRdpSmartcardServicePriv::closeConnection(quint32 id, bool cleanupRemote)
{
	auto it = mConnections.find(id);
	if (it == mConnections.end())
		return;

	std::unique_ptr<PcscdConnection> c = std::move(it->second);
	mConnections.erase(it);
	WLog_DBG(TAG, "pcsc client %u gone", id);

	/* release what the client left on the RDP client, like pcscd does */
	for (const auto &item : c->handles) {
		PcscContext *context = findContext(item.second.contextId);
		if (!cleanupRemote || !context)
			continue;

		PendingRequest cleanup;
		cleanup.command = INTERNAL_CLEANUP;
		HCardAndDisposition_Call call = {};
		call.handles.hContext = context->hContext;
		call.handles.hCard = item.second.handle;
		call.dwDisposition = SCARD_LEAVE_CARD;
		sendRequest(nullptr, cleanup, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
			return ctx->SmartcardDisconnect(ctx, cb, &call, cid);
		});
	}

	for (quint32 contextId : c->contexts) {
		const PcscContext context = mContexts.take(contextId);
		if (!cleanupRemote)
			continue;

		PendingRequest cleanup;
		cleanup.command = INTERNAL_CLEANUP;
		sendRequest(nullptr, cleanup, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
			return ctx->SmartcardReleaseContext(ctx, cb, &context.hContext, cid);
		});
	}

	/* the socket may be the sender of the signal being handled */
	c->socket->disconnect(this);
	c->socket->abort();
	c->socket->deleteLater();
}


/* ----------------------------- channel events ----------------------------- */

void QFreeRdpSmartcardServicePriv::onDeviceCreated(quint64 channel)
{
	{
		QMutexLocker locker(&mChannelsLock);
		if (!mChannels.contains(channel))
			return;
	}

	if (mActiveChannel == channel)
		return;

	/* the last connected client with a smartcard wins */
	if (mActiveChannel)
		dropChannel();
	activateChannel(channel);
}

void QFreeRdpSmartcardServicePriv::onDeviceRemoved(quint64 channel)
{
	if (channel == mActiveChannel)
		dropChannel();
}

void QFreeRdpSmartcardServicePriv::onChannelGone(quint64 channel)
{
	if (channel == mActiveChannel)
		dropChannel();
}

void QFreeRdpSmartcardServicePriv::activateChannel(quint64 channel)
{
	WLog_DBG(TAG, "smartcard device on channel %llu", (unsigned long long)channel);
	mActiveChannel = channel;
	mReady = false;
	monitorEstablish();
}

void QFreeRdpSmartcardServicePriv::dropChannel()
{
	WLog_DBG(TAG, "smartcard device on channel %llu removed", (unsigned long long)mActiveChannel);

	/* the contexts and handles of the clients are lost with the device */
	std::vector<quint32> ids;
	for (const auto &item : mConnections)
		ids.push_back(item.first);
	for (quint32 id : ids)
		closeConnection(id, false);

	mContexts.clear();
	mPending.clear();
	mActiveChannel = 0;

	if (mMonitorTimer)
		mMonitorTimer->stop();
	mMonitorHasContext = false;
	mMonitorNames.clear();
	mMonitorStates.clear();
	memset(mReaders, 0, sizeof(mReaders));

	if (mReady) {
		mReady = false;
		emit mService->smartcardChannelRemoved();
	}
}


/* ----------------------------- readers monitoring ----------------------------- */

void QFreeRdpSmartcardServicePriv::monitorRetry(void (QFreeRdpSmartcardServicePriv::*step)(), int delay)
{
	if (!mMonitorTimer)
		return;

	mMonitorTimer->disconnect();
	connect(mMonitorTimer, &QTimer::timeout, this, step);
	mMonitorTimer->start(delay);
}

void QFreeRdpSmartcardServicePriv::monitorEstablish()
{
	PendingRequest request;
	request.command = INTERNAL_MONITOR_ESTABLISH;
	if (!sendRequest(nullptr, request, [](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
				return ctx->SmartcardEstablishContext(ctx, cb, SCARD_SCOPE_SYSTEM, cid);
			}))
		monitorRetry(&QFreeRdpSmartcardServicePriv::monitorEstablish, 1000);
}

void QFreeRdpSmartcardServicePriv::monitorListReaders()
{
	static BYTE allReaders[] = "SCard$AllReaders\0";

	ListReaders_Call call = {};
	call.handles.hContext = mMonitorContext;
	call.cBytes = sizeof(allReaders);
	call.mszGroups = allReaders;
	call.fmszReadersIsNULL = FALSE;
	call.cchReaders = SCARD_AUTOALLOCATE;

	PendingRequest request;
	request.command = INTERNAL_MONITOR_LIST_READERS;
	if (!sendRequest(nullptr, request, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
				return ctx->SmartcardListReadersA(ctx, cb, &call, cid);
			}))
		monitorRetry(&QFreeRdpSmartcardServicePriv::monitorListReaders, 1000);
}

void QFreeRdpSmartcardServicePriv::monitorStatusChange()
{
	QVector<SCARD_READERSTATEA> states(mMonitorNames.size());
	for (int i = 0; i < mMonitorNames.size(); i++) {
		SCARD_READERSTATEA &state = states[i];
		memset(&state, 0, sizeof(state));
		state.szReader = const_cast<char *>(mMonitorNames[i].constData());
		state.dwCurrentState = mMonitorStates.value(i, SCARD_STATE_UNAWARE);
	}

	GetStatusChangeA_Call call = {};
	call.handles.hContext = mMonitorContext;
	call.dwTimeOut = 2000;
	call.cReaders = states.size();
	call.rgReaderStates = states.data();

	PendingRequest request;
	request.command = INTERNAL_MONITOR_STATUS_CHANGE;
	if (!sendRequest(nullptr, request, [&](RdpdrServerContext *ctx, void *cb, UINT32 *cid) {
				return ctx->SmartcardGetStatusChangeA(ctx, cb, &call, cid);
			}))
		monitorRetry(&QFreeRdpSmartcardServicePriv::monitorListReaders, 1000);
}

void QFreeRdpSmartcardServicePriv::handleMonitorReply(const PendingRequest &request,
		const ScardReply &reply)
{
	switch (request.command) {
	case INTERNAL_MONITOR_ESTABLISH:
		if (reply.rv != SCARD_S_SUCCESS) {
			WLog_ERR(TAG, "unable to establish the monitoring context: 0x%08x", (unsigned)reply.rv);
			monitorRetry(&QFreeRdpSmartcardServicePriv::monitorEstablish, 1000);
			return;
		}
		mMonitorContext = reply.hContext;
		mMonitorHasContext = true;
		monitorListReaders();
		break;

	case INTERNAL_MONITOR_LIST_READERS: {
		if (reply.rv != SCARD_S_SUCCESS && reply.rv != SCARD_E_NO_READERS_AVAILABLE) {
			WLog_DBG(TAG, "error listing readers: 0x%08x", (unsigned)reply.rv);
			monitorRetry(&QFreeRdpSmartcardServicePriv::monitorListReaders, 1000);
			return;
		}

		/* multi-string, with no trust in the RDP client */
		QList<QByteArray> names;
		if (reply.rv == SCARD_S_SUCCESS) {
			for (const QByteArray &name : reply.data.split('\0')) {
				if (name.isEmpty())
					continue;
				if (size_t(names.size()) == PCSC_MAX_READERS) {
					WLog_WARN(TAG, "too many readers, ignoring the others");
					break;
				}
				names.append(name.left(PCSC_MAX_READERNAME - 1));
			}
		}

		if (names.isEmpty()) {
			updateReaders(names, {});
			monitorRetry(&QFreeRdpSmartcardServicePriv::monitorListReaders, 1000);
			return;
		}

		if (names != mMonitorNames) {
			/* the new readers start unaware, the others keep their state */
			QVector<DWORD> states;
			for (const QByteArray &name : names) {
				const int index = mMonitorNames.indexOf(name);
				states.append(index >= 0 ? mMonitorStates.value(index, SCARD_STATE_UNAWARE) : SCARD_STATE_UNAWARE);
			}
			mMonitorNames = names;
			mMonitorStates = states;
		}
		monitorStatusChange();
		break;
	}

	case INTERNAL_MONITOR_STATUS_CHANGE:
		if (reply.rv == SCARD_S_SUCCESS) {
			bool changed = false;
			for (int i = 0; i < reply.readers.size() && i < mMonitorStates.size(); i++) {
				if (reply.readers[i].dwEventState & SCARD_STATE_CHANGED)
					changed = true;
				mMonitorStates[i] = reply.readers[i].dwEventState & ~SCARD_STATE_CHANGED;
			}

			updateReaders(mMonitorNames, reply.readers);

			/* also detects added / removed readers; don't spin with a client
			 * returning immediately without change */
			if (changed)
				monitorListReaders();
			else
				monitorRetry(&QFreeRdpSmartcardServicePriv::monitorListReaders, 500);
		} else if (reply.rv == SCARD_E_TIMEOUT) {
			monitorListReaders();
		} else {
			WLog_DBG(TAG, "error waiting for status change: 0x%08x", (unsigned)reply.rv);
			monitorRetry(&QFreeRdpSmartcardServicePriv::monitorListReaders, 1000);
		}
		break;

	default:
		/* cleanups */
		break;
	}
}

void QFreeRdpSmartcardServicePriv::updateReaders(const QList<QByteArray> &names,
		const QVector<ReaderState_Return> &states)
{
	pcsc_reader_state readers[PCSC_MAX_READERS];
	memset(readers, 0, sizeof(readers));

	for (int i = 0; i < names.size() && size_t(i) < PCSC_MAX_READERS; i++) {
		pcsc_reader_state &r = readers[i];
		const QByteArray &name = names[i];
		memcpy(r.readerName, name.constData(), qMin(size_t(name.size()), sizeof(r.readerName) - 1));

		/* keep the event counter and the protocol of a known reader */
		const pcsc_reader_state *previous = nullptr;
		for (size_t j = 0; j < PCSC_MAX_READERS; j++) {
			if (mReaders[j].readerName[0] && strcmp(mReaders[j].readerName, r.readerName) == 0) {
				previous = &mReaders[j];
				break;
			}
		}

		if (i >= states.size()) {
			r.readerState = PCSC_SCARD_UNKNOWN;
			continue;
		}

		const ReaderState_Return &state = states[i];
		const DWORD ev = state.dwEventState;
		if (ev & (SCARD_STATE_UNKNOWN | SCARD_STATE_UNAVAILABLE | SCARD_STATE_IGNORE)) {
			r.readerState = PCSC_SCARD_UNKNOWN;
		} else if (ev & SCARD_STATE_PRESENT) {
			r.readerState = PCSC_SCARD_PRESENT;
			r.readerState |= (ev & SCARD_STATE_MUTE) ? PCSC_SCARD_SWALLOWED : (PCSC_SCARD_POWERED | PCSC_SCARD_NEGOTIABLE);
			r.cardAtrLength = qMin<DWORD>(state.cbAtr, qMin(sizeof(state.rgbAtr), sizeof(r.cardAtr)));
			memcpy(r.cardAtr, state.rgbAtr, r.cardAtrLength);
		} else {
			r.readerState = PCSC_SCARD_ABSENT;
		}

		if (ev & SCARD_STATE_EXCLUSIVE)
			r.readerSharing = PCSC_SHARING_EXCLUSIVE_CONTEXT;
		else if (ev & SCARD_STATE_INUSE)
			r.readerSharing = PCSC_SHARING_LAST_CONTEXT;
		else
			r.readerSharing = PCSC_SHARING_NO_CONTEXT;

		if (previous) {
			r.eventCounter = previous->eventCounter;
			const bool wasPresent = previous->readerState & PCSC_SCARD_PRESENT;
			const bool isPresent = r.readerState & PCSC_SCARD_PRESENT;
			if (wasPresent != isPresent || r.cardAtrLength != previous->cardAtrLength ||
					memcmp(r.cardAtr, previous->cardAtr, r.cardAtrLength) != 0)
				r.eventCounter = (r.eventCounter + 1) & 0xFFFF;
			else if (isPresent)
				r.cardProtocol = previous->cardProtocol;
		} else if (r.readerState & PCSC_SCARD_PRESENT) {
			r.eventCounter = 1;
		}
	}

	const bool changed = memcmp(readers, mReaders, sizeof(readers)) != 0;
	memcpy(mReaders, readers, sizeof(readers));

	if (changed)
		signalWaitingClients(SCARD_S_SUCCESS);

	if (!mReady) {
		mReady = true;
		emit mService->smartcardChannelCreated();
	}
}

void QFreeRdpSmartcardServicePriv::setReaderProtocol(const QByteArray &readerName, uint32_t winProtocol)
{
	for (size_t i = 0; i < PCSC_MAX_READERS; i++) {
		pcsc_reader_state &r = mReaders[i];
		if (r.readerName[0] && readerName == r.readerName) {
			r.cardProtocol = winToPcscProtocols(winProtocol);
			break;
		}
	}
}

void QFreeRdpSmartcardServicePriv::signalWaitingClients(LONG rv)
{
	std::vector<quint32> broken;
	for (auto &item : mConnections) {
		PcscdConnection *c = item.second.get();
		if (!c->waitingEvent)
			continue;

		c->waitingEvent = false;
		pcsc_wait_reader_state_change wait = { 0, static_cast<uint32_t>(rv) };
		sendStruct(c, wait);
		if (c->broken)
			broken.push_back(c->id);
	}

	for (quint32 id : broken)
		closeConnection(id, true);
}
