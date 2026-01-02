/**
 * Copyright © 2023 David Fort <contact@hardening-consulting.com>
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

#include <winpr/stream.h>
#include <winpr/user.h>
#include <winpr/sysinfo.h>

#include <freerdp/version.h>
#include <freerdp/freerdp.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/codec/planar.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/input.h>
#include <freerdp/channels/cliprdr.h>
#include <freerdp/channels/drdynvc.h>
#include <freerdp/server/rdpgfx.h>

#include "qfreerdpplatform.h"
#include "qfreerdppeer.h"
#include "qfreerdpwindow.h"
#include "qfreerdpscreen.h"
#include "qfreerdpplatform.h"
#include "qfreerdpwindowmanager.h"
#include "qfreerdpclipboard.h"
#include "qfreerdppeerclipboard.h"
#include "qfreerdppeerkeyboard.h"

#include <QAbstractEventDispatcher>
#include <QSocketNotifier>
#include <QDebug>
#include <QMutexLocker>
#include <QStringList>
#include <QtGui/qpa/qwindowsysteminterface.h>
#include <qpa/qplatforminputcontext.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtCore/qmath.h>

struct RdpPeerContext {
	rdpContext _p;
	QFreeRdpPeer *rdpPeer;

	rdpSettings *settings;

	QSocketNotifier* event;
	QSocketNotifier* channelEvent;

	NSC_CONTEXT* nsc_context;
};


static BOOL rdp_peer_context_new(freerdp_peer* client, RdpPeerContext* context) {
	// init settings
	auto settings = client->context->settings;
	context->settings = settings;

	// init codecs
	auto codecs =
#if FREERDP_VERSION_MAJOR == 3 && FREERDP_VERSION_MINOR >= 5 || FREERDP_VERSION_MAJOR > 3
		freerdp_client_codecs_new( freerdp_settings_get_uint32(context->settings, FreeRDP_ThreadingFlags));
#else
		codecs_new(client->context);
#endif

	client->context->codecs = codecs;
	freerdp_client_codecs_prepare(codecs, FREERDP_CODEC_ALL, settings->DesktopWidth,
			settings->DesktopHeight);

	// Planar configuration
	// unfortunately we have to recreate the context to enable compression
	freerdp_bitmap_planar_context_free(codecs->planar);
	codecs->planar = freerdp_bitmap_planar_context_new(PLANAR_FORMAT_HEADER_RLE, 64, 64);
	if (!codecs->planar)
		return FALSE;

	// init NSC encoder
	context->nsc_context = nsc_context_new();
	nsc_context_reset(context->nsc_context, settings->DesktopWidth, settings->DesktopHeight);
	nsc_context_set_parameters(context->nsc_context, NSC_COLOR_LOSS_LEVEL, settings->NSCodecColorLossLevel);
	nsc_context_set_parameters(context->nsc_context, NSC_ALLOW_SUBSAMPLING, settings->NSCodecAllowSubsampling ? 1 : 0);
	nsc_context_set_parameters(context->nsc_context, NSC_DYNAMIC_COLOR_FIDELITY, settings->NSCodecAllowDynamicColorFidelity ? 1 : 0);
	nsc_context_set_parameters(context->nsc_context, NSC_COLOR_FORMAT, PIXEL_FORMAT_BGRX32);

	return TRUE;
}

static void rdp_peer_context_free(freerdp_peer* client, RdpPeerContext* context) {
	if(!context)
		return;

	nsc_context_free(context->nsc_context);
	context->nsc_context = NULL;

	// free codecs
#if FREERDP_VERSION_MAJOR == 3 && FREERDP_VERSION_MINOR >= 5 || FREERDP_VERSION_MAJOR > 3
	freerdp_client_codecs_free(client->context->codecs);
#else
	codecs_free(client->context->codecs);
#endif
}

QFreeRdpPeer::QFreeRdpPeer(QFreeRdpPlatform *platform, freerdp_peer* client) :
		mPlatform(platform),
		mClient(client),
		mBogusCheckFileDescriptor(0),
		mLastButtons(Qt::NoButton),
		mCurrentButton(Qt::NoButton),
		mKeyboard(platform->mConfig),
		mFirstFrame(true),
		mSurfaceOutputModeEnabled(false),
		mNsCodecSupported(false),
		mCompositor(platform),
		mRenderMode(RENDER_BITMAP_UPDATES),
		mVcm(nullptr),
		mClipboard(nullptr),
		mFrameId(0)
{
	connect(this, &QFreeRdpPeer::updatedMonitors, this, &QFreeRdpPeer::onUpdatedMonitors);
}

void QFreeRdpPeer::dropSocketNotifier(QSocketNotifier *notifier) {
	if(notifier) {
		notifier->setEnabled(false);
		disconnect(notifier, &QSocketNotifier::activated, this, &QFreeRdpPeer::incomingBytes);
		delete notifier;
	}
}

QFreeRdpPeer::~QFreeRdpPeer() {
	RdpPeerContext *peerCtx = (RdpPeerContext *)mClient->context;

	dropSocketNotifier(peerCtx->event);
	dropSocketNotifier(peerCtx->channelEvent);

	if (mEgfx.rdpgfx) {
		mEgfx.rdpgfx->Close(mEgfx.rdpgfx);
		rdpgfx_server_context_free(mEgfx.rdpgfx);
		mEgfx.rdpgfx = nullptr;
	}

	if (mClipboard)
		delete mClipboard;

	if (mDisplay.channel) {
		mDisplay.channel->Close(mDisplay.channel);
		disp_server_context_free(mDisplay.channel);
		mDisplay.channel = nullptr;
	}

	if (mVcm)
		WTSCloseServer(mVcm);
	mVcm = NULL;

	mClient->Close(mClient);

	freerdp_peer_context_free(mClient);
	freerdp_peer_free(mClient);
	mPlatform->unregisterPeer(this);
}

QFreeRdpPeer::EgfxSurface::EgfxSurface(UINT16 pSurfaceId, const QRect &pRdpGeom)
: surfaceId(pSurfaceId)
, rdpGeom(pRdpGeom)
{
}

QFreeRdpPeer::EgfxState::EgfxState()
: opened(false)
, rdpgfx(nullptr)
, surfaceCounter(1)
{
}

QFreeRdpPeer::DispState::DispState()
: opened(false)
, channel(nullptr)
, channelId(0xFFFFFFFF)
{
}


BOOL QFreeRdpPeer::xf_mouseEvent(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y) {
	RdpPeerContext *peerContext = (RdpPeerContext *)input->context;
	QFreeRdpPeer *peer = peerContext->rdpPeer;
	QFreeRdpWindowManager *windowManager = peer->mPlatform->mWindowManager;
	bool down;

	if (flags & PTR_FLAGS_WHEEL) {
		int wheelDelta = (flags & 0xff);
		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			wheelDelta = -wheelDelta;

		return windowManager->handleWheelEvent(peer->mLastMousePos, wheelDelta);
	}

	peer->updateMouseButtonsFromFlags(flags, down, false);

	peer->mLastMousePos = QPoint(x, y);
	return windowManager->handleMouseEvent(peer->mLastMousePos, peer->mLastButtons,
			peer->mCurrentButton, down);
}

BOOL QFreeRdpPeer::xf_extendedMouseEvent(rdpInput* /*input*/, UINT16 /*flags*/, UINT16 /*x*/, UINT16 /*y*/) {
	return TRUE;
}

BOOL QFreeRdpPeer::xf_input_synchronize_event(rdpInput* input, UINT32 flags)
{
	qDebug("QFreeRdpPeer::%s()", __func__);
	freerdp_peer* client = input->context->peer;
	RdpPeerContext *peerCtx = (RdpPeerContext *)input->context;
	QFreeRdpPeer *rdpPeer = peerCtx->rdpPeer;

	rdpPeer->sendFullRefresh(client->context->settings);

	rdpPeer->mKeyboard.updateModifiersState(
			flags & KBD_SYNC_CAPS_LOCK,
			flags & KBD_SYNC_NUM_LOCK,
			flags & KBD_SYNC_SCROLL_LOCK,
			flags & KBD_SYNC_KANA_LOCK);

	return TRUE;
}

void QFreeRdpPeer::sendFullRefresh(rdpSettings *settings) {
	QRect refreshRect(0, 0, settings->DesktopWidth, settings->DesktopHeight);

	repaint(QRegion(refreshRect), false);
}

BOOL QFreeRdpPeer::xf_input_keyboard_event(rdpInput* input, UINT16 flags, UINT8 code)
{
	RdpPeerContext *peerCtx = (RdpPeerContext *)input->context;
	QFreeRdpPeer *rdpPeer = peerCtx->rdpPeer;
	rdpSettings *settings = rdpPeer->mClient->context->settings;
	QFreeRdpWindow *focusWindow = rdpPeer->mPlatform->mWindowManager->getFocusWindow();
	uint32_t kbdType = freerdp_settings_get_uint32(settings, FreeRDP_KeyboardType);

	rdpPeer->mKeyboard.handleRdpScancode(code, flags, kbdType, focusWindow);
	return TRUE;
}

BOOL QFreeRdpPeer::xf_input_unicode_keyboard_event(rdpInput* /*input*/, UINT16 /*flags*/, UINT16 /*code*/)
{
	// qDebug("Client sent a unicode keyboard event (flags:0x%X code:0x%X)\n", flags, code);
	qDebug("Client sent a unicode keyboard event: UNSUPPORTED\n");
	return TRUE;
}

BOOL QFreeRdpPeer::xf_refresh_rect(rdpContext *context, BYTE count, const RECTANGLE_16* areas) {
	qDebug("QFreeRdpPeer::%s()", __func__);

	// get RDP peer
	RdpPeerContext *peerCtx = (RdpPeerContext *)context;
	QFreeRdpPeer *rdpPeer = peerCtx->rdpPeer;
	QRegion refreshRegion;

	// collect rect areas for painting
	for(int i = 0; i < count; i++) {
		RECTANGLE_16 rect = areas[i];
		refreshRegion += QRect(rect.left, rect.top, rect.right, rect.bottom);
	}

	// Do not try to reduce the size of the update using the compositor's
	// cache. We got asked for a certain size and we're going to send all
	// of it.
	rdpPeer->repaint(refreshRegion, false);

	return TRUE;
}

BOOL QFreeRdpPeer::xf_suppress_output(rdpContext *context, BYTE allow, const RECTANGLE_16* /*area*/) {
	//qDebug("xf_suppress_output(allow=%d)", allow);

	RdpPeerContext *peerContext = (RdpPeerContext *)context;
	QFreeRdpPeer *rdpPeer = peerContext->rdpPeer;
	if(allow)
		rdpPeer->mFlags &= (~PEER_OUTPUT_DISABLED);
	else
		rdpPeer->mFlags |= PEER_OUTPUT_DISABLED;
	return TRUE;
}

BOOL QFreeRdpPeer::xf_peer_capabilities(freerdp_peer* /*client*/) {
	return TRUE;
}

BOOL QFreeRdpPeer::xf_surface_frame_acknowledge(rdpContext* context, UINT32 frameId) {
	RdpPeerContext *peerContext = (RdpPeerContext *)context;
	QFreeRdpPeer *rdpPeer = peerContext->rdpPeer;

	return rdpPeer->frameAck(frameId);
}

BOOL QFreeRdpPeer::dynvc_dynChannelCreationStatus(void* userdata, UINT32 channelId, INT32 creationStatus) {
	QFreeRdpPeer *peer = (QFreeRdpPeer *)userdata;

	if (peer->mDisplay.channel && (channelId == peer->mDisplay.channelId)) {
		if (creationStatus == ERROR_SUCCESS) {
			if (peer->mDisplay.channel->DisplayControlCaps(peer->mDisplay.channel) != CHANNEL_RC_OK) {
				   qDebug("error sending display capabilities");
			}
		}
	}
	return TRUE;
}

BOOL QFreeRdpPeer::display_channelIdAssigned(DispServerContext* context, UINT32 channelId) {
	QFreeRdpPeer *peer = (QFreeRdpPeer *)context->custom;
	peer->mDisplay.channelId = channelId;
	return TRUE;
}

DISPLAY_CONTROL_MONITOR_LAYOUT_PDU *DISPLAY_CONTROL_MONITOR_LAYOUT_PDU_dup(const DISPLAY_CONTROL_MONITOR_LAYOUT_PDU* pdu) {
	DISPLAY_CONTROL_MONITOR_LAYOUT_PDU *ret = (DISPLAY_CONTROL_MONITOR_LAYOUT_PDU *)calloc(1, sizeof(*ret) + pdu->NumMonitors * sizeof(*pdu->Monitors));
	if (!ret)
		return nullptr;

	ret->NumMonitors = pdu->NumMonitors;
	ret->Monitors = (DISPLAY_CONTROL_MONITOR_LAYOUT*)(ret + 1);
	memcpy(ret->Monitors, pdu->Monitors, pdu->NumMonitors * sizeof(*pdu->Monitors));
	return ret;
}

UINT QFreeRdpPeer::display_monitorLayout(DispServerContext* context, const DISPLAY_CONTROL_MONITOR_LAYOUT_PDU* pdu)
{
	QFreeRdpPeer *peer = (QFreeRdpPeer *)context->custom;
	qDebug("display layout updated");

	DISPLAY_CONTROL_MONITOR_LAYOUT_PDU *copy = DISPLAY_CONTROL_MONITOR_LAYOUT_PDU_dup(pdu);
	if (!copy)
		return CHANNEL_RC_NO_MEMORY;
	emit peer->updatedMonitors(copy);

	return CHANNEL_RC_OK;
}

void QFreeRdpPeer::onUpdatedMonitors(DISPLAY_CONTROL_MONITOR_LAYOUT_PDU* pdu)
{
	bool resized;
	if (!mPlatform->updateMonitors(pdu, &resized) || !initGfxDisplay())
		goto out;

	if (resized) {
		auto displaySize = mPlatform->monitorsRegion().boundingRect().size();
		mCompositor.reset(displaySize);
	}

out:
	free(pdu);
}

UINT QFreeRdpPeer::rdpgfx_caps_advertise(RdpgfxServerContext* context, const RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise) {
	QFreeRdpPeer *peer = (QFreeRdpPeer *)context->custom;
	UINT rc = CHANNEL_RC_OK;
	UINT32 versionsOrder[] = {
		RDPGFX_CAPVERSION_107,
		RDPGFX_CAPVERSION_106,
		RDPGFX_CAPVERSION_106_ERR,
		RDPGFX_CAPVERSION_105,
		RDPGFX_CAPVERSION_104,
		RDPGFX_CAPVERSION_103,
		RDPGFX_CAPVERSION_102,
		RDPGFX_CAPVERSION_101,
		RDPGFX_CAPVERSION_10,
	};

	for (size_t i = 0; i < ARRAYSIZE(versionsOrder); i++) {
		if (peer->egfx_caps_test(capsAdvertise, versionsOrder[i], rc)) {
			peer->mFlags.setFlag(PEER_WAITING_GRAPHICS, false);
			return rc;
		}
	}

	/* TODO: handle older versions */
	return CHANNEL_RC_OK;
}

UINT QFreeRdpPeer::rdpgfx_frame_acknowledge(RdpgfxServerContext* context, const RDPGFX_FRAME_ACKNOWLEDGE_PDU* frameAcknowledge) {
	QFreeRdpPeer *peer = (QFreeRdpPeer *)context->custom;
	peer->frameAck(frameAcknowledge->frameId);
	return CHANNEL_RC_OK;
}

BOOL QFreeRdpPeer::configureDisplayLegacyMode(rdpSettings *settings) {
	// Disable surface mode
	this->mSurfaceOutputModeEnabled = FALSE;

	// disable NS codec
	this->mNsCodecSupported = FALSE;

	if (settings->ColorDepth == 24) {
		// don't support 24 bits if surface mode not enabled
		return FALSE;
	}
	return TRUE;
}

BOOL QFreeRdpPeer::configureOptimizeMode(rdpSettings *settings) {
	// display surface and eventually NS codec
	if ((settings->SurfaceCommandsEnabled) && (settings->FastPathOutput)) {
		this->mSurfaceOutputModeEnabled = settings->FrameMarkerCommandEnabled && settings->SurfaceFrameMarkerEnabled;
	} else {
		qWarning("QFreeRdpPeer::%s: peer does not support Surface commands", __func__);
	}

	if ((settings->ColorDepth == 24) && (!this->mSurfaceOutputModeEnabled)) {
		// don't support 24 bits if surface mode not enabled
		return FALSE;
	}

	// check codec configuration
	if ((settings->NSCodec) && (this->mSurfaceOutputModeEnabled)) {
		// 1- NS codec in compatibility list
		// 2- Surface commands enabled
		this->mNsCodecSupported = FALSE;
	}
	return TRUE;
}

BOOL QFreeRdpPeer::detectDisplaySettings(freerdp_peer* client) {
	rdpSettings *settings = client->context->settings;

	// check color depths
	if (settings->ColorDepth == 8) {
		// don't support bpp 8
		return FALSE;
	}

	if (mPlatform->getDisplayMode() == DisplayMode::LEGACY) {
		// display only bitmap updates
		return configureDisplayLegacyMode(settings);

	} else if (mPlatform->getDisplayMode() == DisplayMode::OPTIMIZE) {
		// compute surface commands and NSC settings
		return configureOptimizeMode(settings);

	} else if (mPlatform->getDisplayMode() == DisplayMode::AUTODETECT) {

		// XXX: Has this ever seen the light of day?! oO
		// // If RDP client is a windows client -> Optimize mode
		// if (settings->OsMajorType == OSMAJORTYPE_WINDOWS) {
		// 	return configureOptimizeMode(settings);
		// }

		// else -> legacy mode
		return configureDisplayLegacyMode(settings);
	}

	return FALSE;
}

BOOL QFreeRdpPeer::xf_peer_activate(freerdp_peer *client) {
	RdpPeerContext *ctx = (RdpPeerContext *)client->context;
	QFreeRdpPeer *rdpPeer = ctx->rdpPeer;

	if (!rdpPeer->initializeChannels())
		return FALSE;

	rdpPeer->mFlags.setFlag(PEER_ACTIVATED);
	if (rdpPeer->mFlags & PEER_WAITING_DYNVC) {
		rdpPeer->checkDrdynvcState();
	} else {
		rdpPeer->sendFullRefresh(client->context->settings);
	}
	return TRUE;
}


BOOL QFreeRdpPeer::xf_peer_post_connect(freerdp_peer* client) {
	RdpPeerContext *ctx = (RdpPeerContext *)client->context;
	QFreeRdpPeer *rdpPeer = ctx->rdpPeer;
	rdpSettings *settings = client->context->settings;

	if (!rdpPeer->detectDisplaySettings(client)) {
		// can't detect display settings
		qWarning("Can not detect correctly settings");
		return FALSE;
	}

	return rdpPeer->mKeyboard.setKeymap(settings);
}

bool QFreeRdpPeer::initializeChannels()
{
	/* =============== clipboard =================*/
	delete mClipboard;
	mClipboard = nullptr;

	QFreeRdpPlatformConfig *config = mPlatform->mConfig;

	if (config->clipboard_enabled && WTSVirtualChannelManagerIsChannelJoined(mVcm, CLIPRDR_SVC_CHANNEL_NAME))
	{
		qDebug() << "instantiating clipboard component";

		mClipboard = new QFreerdpPeerClipboard(this, mVcm);
		if (!mClipboard->start()) {
			qDebug() << "error starting clipboard";
			return false;
		}
		mPlatform->mClipboard->registerPeer(mClipboard);
	}

	/* =============== egfx =================*/
	if (mEgfx.rdpgfx) {
		rdpgfx_server_context_free(mEgfx.rdpgfx);
		mEgfx.rdpgfx = nullptr;
	}

	if (WTSVirtualChannelManagerIsChannelJoined(mVcm, DRDYNVC_SVC_CHANNEL_NAME))
	{
		auto settings = mClient->context->settings;
		WTSVirtualChannelManagerSetDVCCreationCallback(mVcm, dynvc_dynChannelCreationStatus, this);

		if (settings->SupportGraphicsPipeline && config->egfx_enabled) {
			qDebug() << "instantiating EGFX component";
			mEgfx.rdpgfx = rdpgfx_server_context_new(mVcm);
			if (!mEgfx.rdpgfx) {
				qDebug() << "error instantiating egfx";
				return false;
			}

			mEgfx.rdpgfx->rdpcontext = mClient->context;
			mEgfx.rdpgfx->custom = this;
			mEgfx.rdpgfx->CapsAdvertise = rdpgfx_caps_advertise;
			mEgfx.rdpgfx->FrameAcknowledge = rdpgfx_frame_acknowledge;

			mFlags.setFlag(PEER_WAITING_DYNVC, true);
			mFlags.setFlag(PEER_WAITING_GRAPHICS, true);
		} else {
			qDebug() << "no support for EGFX";
		}

		if (config->resize_enabled) {
			mDisplay.channel = disp_server_context_new(mVcm);
			if (!mDisplay.channel) {
				qDebug() << "error instantiating display channel";
				return false;
			}

			mDisplay.channel->custom = this;
			mDisplay.channel->ChannelIdAssigned = display_channelIdAssigned;
			mDisplay.channel->DispMonitorLayout = display_monitorLayout;
			mDisplay.channel->MaxNumMonitors = 4;
			mDisplay.channel->MaxMonitorAreaFactorA = 2048;
			mDisplay.channel->MaxMonitorAreaFactorB = 2048;
			mFlags.setFlag(PEER_WAITING_DYNVC, true);
		} else {
			qDebug() << "no support for Display resize";
		}

		// note: this will cause the dynamic channel to be started
		if ((mFlags & PEER_WAITING_DYNVC) && !WTSVirtualChannelManagerOpen(mVcm))
			return false;
	}

	return init_display(mClient);
}

bool QFreeRdpPeer::frameAck(UINT32 frameId) {
	Q_UNUSED(frameId);
	return true;
}

bool QFreeRdpPeer::egfx_caps_test(const RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise, UINT32 version, UINT &rc) {
	for (UINT16 i = 0; i < capsAdvertise->capsSetCount; i++) {
		RDPGFX_CAPSET *capSet = &capsAdvertise->capsSets[i];
		if (capSet->version == version) {
			RDPGFX_CAPSET caps = *capSet;
			RDPGFX_CAPS_CONFIRM_PDU pdu = { &caps };

			caps.flags |= RDPGFX_CAPS_FLAG_AVC_DISABLED; /* no encoding at all */
			rc = mEgfx.rdpgfx->CapsConfirm(mEgfx.rdpgfx, &pdu);
			return true;
		}
	}

	return false;
}

bool QFreeRdpPeer::initGfxDisplay() {
	const QList<QFreeRdpScreen *> screens = mPlatform->getScreens();
	const QRegion &allMonitors = mPlatform->monitorsRegion();
	QRect allGeometry = allMonitors.boundingRect();

	auto primaryScreen = QGuiApplication::primaryScreen()->handle();
	MONITOR_DEF monitors[16];
	for (auto i = 0; i < qMin(screens.size(), 16); i++) {
		auto screen = screens.at(i);
		QRect rdpGeom = screen->rdpGeometry();
		MONITOR_DEF monitor = {
			rdpGeom.left(), rdpGeom.top(), rdpGeom.right(), rdpGeom.bottom(),
			static_cast<UINT32>((primaryScreen  == screen) ? MONITOR_PRIMARY : 0)
		};
		monitors[i] = monitor;
	}

	RDPGFX_RESET_GRAPHICS_PDU pdu = {
		(UINT32)allGeometry.width(), (UINT32)allGeometry.height(),
		(UINT32)screens.size(), monitors
	};

	if (mEgfx.rdpgfx->ResetGraphics(mEgfx.rdpgfx, &pdu) != CHANNEL_RC_OK) {
		qDebug("error resetting graphics");
		return false;
	}

	for (int i = 0; i < qMin(screens.size(), 16); i++) {
		QFreeRdpScreen *freerdpScreen = screens.at(i);
		QScreen *screen = freerdpScreen->screen();
		QRect geom = screen->geometry();

		if (i <= mEgfx.surfaces.size()-1) {
			const EgfxSurface &surface = mEgfx.surfaces.at(i);
			RDPGFX_DELETE_SURFACE_PDU deleteSurface = { surface.surfaceId };
			if (mEgfx.rdpgfx->DeleteSurface(mEgfx.rdpgfx, &deleteSurface) != CHANNEL_RC_OK) {
				qDebug("error deleting surface %d", surface.surfaceId);
				return false;
			}
		} else {
			qDebug("creating surface %d", mEgfx.surfaceCounter);
			mEgfx.surfaces.push_back(EgfxSurface(mEgfx.surfaceCounter++, freerdpScreen->rdpGeometry()));
		}

		EgfxSurface &surface = mEgfx.surfaces[i];
		RDPGFX_CREATE_SURFACE_PDU createSurface = {
			surface.surfaceId,
			(UINT16)geom.width(), (UINT16)geom.height(),
			GFX_PIXEL_FORMAT_XRGB_8888
		};

		if (mEgfx.rdpgfx->CreateSurface(mEgfx.rdpgfx, &createSurface) != CHANNEL_RC_OK) {
			qDebug("error creating surface %d", surface.surfaceId);
			return false;
		}

		RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU surfaceToOutput = {
			surface.surfaceId, 0,
			(UINT32)(geom.left() - allGeometry.left()), (UINT32)(geom.top() - allGeometry.top())
		};
		if (mEgfx.rdpgfx->MapSurfaceToOutput(mEgfx.rdpgfx, &surfaceToOutput) != CHANNEL_RC_OK) {
			qDebug("error mapping surface to output");
			return false;
		}
	}
	return true;
}

static void settingsMonitors_to_layout(rdpSettings* settings, DISPLAY_CONTROL_MONITOR_LAYOUT_PDU *layout) {
	layout->NumMonitors = qMin(qMax((UINT32)1, settings->MonitorCount), layout->MonitorLayoutSize);

	if (settings->MonitorCount) {
		for (UINT32 i = 0; i < settings->MonitorCount; i++) {
			rdpMonitor *m = &settings->MonitorDefArray[i];
			DISPLAY_CONTROL_MONITOR_LAYOUT *monitor = &layout->Monitors[i];
			monitor->Flags = m->is_primary ? DISPLAY_CONTROL_MONITOR_PRIMARY : 0;
			monitor->Left = m->x;
			monitor->Top = m->y;
			monitor->Width = (UINT32)m->width;
			monitor->Height = (UINT32)m->height;
			monitor->PhysicalWidth = (UINT32)m->attributes.physicalWidth;
			monitor->PhysicalHeight = (UINT32)m->attributes.physicalHeight;
			monitor->DesktopScaleFactor = m->attributes.desktopScaleFactor;
			monitor->DeviceScaleFactor = m->attributes.deviceScaleFactor;
		}
	} else {
		DISPLAY_CONTROL_MONITOR_LAYOUT *monitor = &layout->Monitors[0];
		monitor->Flags = DISPLAY_CONTROL_MONITOR_PRIMARY;
		monitor->Left = 0;
		monitor->Top = 0;
		monitor->Width = settings->DesktopWidth;
		monitor->Height = settings->DesktopHeight;
		monitor->PhysicalWidth = settings->DesktopPhysicalWidth;
		monitor->PhysicalHeight = settings->DesktopPhysicalHeight;
		monitor->DesktopScaleFactor = settings->DesktopScaleFactor;
		monitor->DeviceScaleFactor = settings->DeviceScaleFactor;
	}
}

bool QFreeRdpPeer::init_display(freerdp_peer* client) {
	rdpSettings* settings = client->context->settings;
	bool resized;

	DISPLAY_CONTROL_MONITOR_LAYOUT monitors[16] = { 0 };
	DISPLAY_CONTROL_MONITOR_LAYOUT_PDU monitorLayout = { 16, 0, monitors };
	settingsMonitors_to_layout(settings, &monitorLayout);

	if (!mPlatform->updateMonitors(&monitorLayout, &resized))
		return false;

	QSize imgSize = mPlatform->monitorsRegion().boundingRect().size();
	QSize settingsSize(settings->DesktopWidth, settings->DesktopHeight);
	if (imgSize != settingsSize) {
		qDebug("case with settingsSize != allMonitorsSize not handled yet");
		return false;
	}

	mCompositor.reset(settingsSize);

	// default : show mouse
	POINTER_SYSTEM_UPDATE pointer_system;
	pointer_system.type = SYSPTR_DEFAULT;
	return client->context->update->pointer->PointerSystem(client->context, &pointer_system);
}


bool QFreeRdpPeer::init() {
	mClient->ContextSize = sizeof(RdpPeerContext);
	mClient->ContextNew = (psPeerContextNew)rdp_peer_context_new;
	mClient->ContextFree = (psPeerContextFree)rdp_peer_context_free;

	int clientFd = mClient->sockfd;
	if (!freerdp_peer_context_new(mClient))
		return false;

	RdpPeerContext *peerCtx = (RdpPeerContext *)mClient->context;
	peerCtx->rdpPeer = this;

	rdpSettings	*settings;
	settings = mClient->context->settings;
	settings->MultitransportFlags = 0;
	settings->NlaSecurity = FALSE;
	settings->RemoteFxCodec = FALSE;
	settings->NSCodec = TRUE; // support NS codec
	settings->ColorDepth = 32;
	mPlatform->configureClient(settings);

	mClient->Capabilities = QFreeRdpPeer::xf_peer_capabilities;
	mClient->PostConnect = QFreeRdpPeer::xf_peer_post_connect;
	mClient->Activate = QFreeRdpPeer::xf_peer_activate;

	rdpUpdate *update = mClient->context->update;
	update->RefreshRect = QFreeRdpPeer::xf_refresh_rect;
	update->SuppressOutput = QFreeRdpPeer::xf_suppress_output;
	update->autoCalculateBitmapData = FALSE;

	rdpInput *input = mClient->context->input;
	input->SynchronizeEvent = QFreeRdpPeer::xf_input_synchronize_event;
	input->MouseEvent = QFreeRdpPeer::xf_mouseEvent;
	input->ExtendedMouseEvent = QFreeRdpPeer::xf_extendedMouseEvent;
	input->KeyboardEvent = QFreeRdpPeer::xf_input_keyboard_event;
	input->UnicodeKeyboardEvent = QFreeRdpPeer::xf_input_unicode_keyboard_event;

	if (!mClient->Initialize(mClient))
		return false;

	mVcm = WTSOpenServerA((LPSTR)mClient->context);
	if (!mVcm) {
		qDebug() << "unable to retrieve client VCM\n";
		return false;
	}

	peerCtx->event = new QSocketNotifier(clientFd, QSocketNotifier::Read);
	connect(peerCtx->event, &QSocketNotifier::activated, this, &QFreeRdpPeer::incomingBytes);

	HANDLE vcmHandle = WTSVirtualChannelManagerGetEventHandle(mVcm);
	if (vcmHandle)
	{
		int vcmFd = GetEventFileDescriptor(vcmHandle);
		peerCtx->channelEvent = new QSocketNotifier(vcmFd, QSocketNotifier::Read);
		connect(peerCtx->channelEvent, &QSocketNotifier::activated, this, &QFreeRdpPeer::channelTraffic);
	}

	return true;
}

void QFreeRdpPeer::incomingBytes(int) {
	//qDebug() << "incomingBytes()";
	do {
		if(!mClient->CheckFileDescriptor(mClient)) {
			qDebug() << "error checking file descriptor";
			deleteLater();
			return;
		}
	} while (mClient->HasMoreToRead(mClient));
}

void QFreeRdpPeer::checkDrdynvcState() {
	switch (WTSVirtualChannelManagerGetDrdynvcState(mVcm))
	{
	case DRDYNVC_STATE_NONE:
		qDebug("drdynvc not ready");
		break;
	case DRDYNVC_STATE_READY:
		if (mFlags.testFlag(PEER_WAITING_DYNVC)) {
			auto context = mClient->context;
			/* EGFX */
			if (mEgfx.rdpgfx && !mEgfx.opened)	{
				qDebug("drdynvc ready, opening GraphicsPipeline");
				if (!mEgfx.rdpgfx->Open(mEgfx.rdpgfx))
				{
					qDebug("Failed to open GraphicsPipeline");
					context->settings->SupportGraphicsPipeline = FALSE;
					mRenderMode = RENDER_BITMAP_UPDATES;
				}
				else
				{
					qDebug("Gfx Pipeline Opened");
					mEgfx.opened = true;
					mRenderMode = RENDER_EGFX;
					freerdp_planar_topdown_image(context->codecs->planar, TRUE);
				}
			}

			/* Display channel */
			if (mDisplay.channel) {
				qDebug("drdynvc ready, opening disp channel");
				if (mDisplay.channel->Open(mDisplay.channel) != CHANNEL_RC_OK) {
					qDebug("error opening display channel");

					disp_server_context_free(mDisplay.channel);
					mDisplay.channel = nullptr;
					return;
				}
				mDisplay.opened = true;
			}

			mFlags.setFlag(PEER_WAITING_DYNVC, false);
		}
		break;
	case DRDYNVC_STATE_FAILED:
		qDebug("drdynvc failed");
		break;
	default:
		break;
	}

}

void QFreeRdpPeer::channelTraffic(int) {
	if (!WTSVirtualChannelManagerCheckFileDescriptor(mVcm)) {
		qDebug() << "error treating channels";
		deleteLater();
		return;
	}

	if (mFlags.testFlag(PEER_ACTIVATED) && WTSVirtualChannelManagerIsChannelJoined(mVcm, DRDYNVC_SVC_CHANNEL_NAME))	{
		checkDrdynvcState();
	}
}


void QFreeRdpPeer::repaint(const QRegion &region, bool useCompositorCache) {
	if(!mFlags.testFlag(PEER_ACTIVATED) ||
	   mFlags.testFlag(PEER_OUTPUT_DISABLED) ||
	   mFlags.testFlag(PEER_WAITING_DYNVC) ||
	   mFlags.testFlag(PEER_WAITING_GRAPHICS))
		return;

	QRegion repaintRegion = region;
	if (mFirstFrame) {
		repaintRegion = mPlatform->mAllMonitorsRegion;
		mFirstFrame = false;
	}

	QRegion dirty = mCompositor.qtToRdpDirtyRegion(repaintRegion);
	// We bypass the compositor only _after_ giving it the lastest update, so
	// that its cache stays in sync with what we send to our peer.
	if (!useCompositorCache)
		dirty = region;


	// qDebug() << "QFreeRdpPeer::repaint(" << dirty << ")";

	switch(mRenderMode) {
	case RENDER_BITMAP_UPDATES:
		repaint_raw(dirty);
		break;
	case RENDER_EGFX: {
		bool doCompress = freerdp_settings_get_bool(mClient->context->settings, FreeRDP_GfxPlanar);
		repaint_egfx(dirty, doCompress);
		break;
	}
	default:
		break;
	}
}

void qimage_subrect(const QRect &rect, const QImage *img, BYTE *dest, bool flip_vertical) {
	// get stride (number of bytes per line) in source image
	int stride = img->bytesPerLine();

	// get origin point
	QPoint orig = flip_vertical ? rect.bottomLeft() : rect.topLeft();

	// length to copy
	int lengthtoCopy = rect.width() * 4;

	// get src bytes
	const uchar *src = img->bits() + (orig.y() * stride) + (orig.x() * 4);

	if (flip_vertical) {
		// flip bytes (vertical)
		for(int h = 0; h < rect.height(); h++, src -= stride, dest += lengthtoCopy)
			memcpy(dest, src, lengthtoCopy);
	} else {
		// normal copy
		for(int h = 0; h < rect.height(); h++, src += stride, dest += lengthtoCopy)
			memcpy(dest, src, lengthtoCopy);
	}


}


void QFreeRdpPeer::repaint_raw(const QRegion &region) {

	QVector<QRect> rects;
	for (const QRect& boundingRect: region) {

		// divide boundingRect in several rects
		int subRectWidth = 64;
		int subRectHeight = 64;

		int boundingRectX1 = boundingRect.left();
		int boundingRectY1 = boundingRect.top();
		int boundingRectX2 = boundingRect.right();
		int boundingRectY2 = boundingRect.bottom();
//		qDebug() << "boundingRectX1 " << boundingRectX1 << " boundingRectY1 " << boundingRectY1
//								<< " boundingRectX2 " << boundingRectX2 << " boundingRectY2 " << boundingRectY2;

		int y = boundingRectY1;
		// height
		while ( y <= boundingRectY2 ) {
			// check height
			int height = (y + subRectHeight - 1) > boundingRectY2 ? (boundingRectY2 - y + 1) : subRectHeight;

			// width
			int x = boundingRectX1;
			while ( x <= boundingRectX2 ){

				// check width
				int width = (x + subRectWidth - 1) > boundingRectX2 ? (boundingRectX2 - x + 1) : subRectWidth;

				// create subRect
				QRect subRect = QRect(QPoint(x, y), QSize(width, height));
//				qDebug() << "x1 " << subRect.left() << " y1 " << subRect.top()
//						<< " x2 " << subRect.right() << " y2 " << subRect.bottom();

				// add rects
				rects.append(subRect);

				// increment x
				x += subRectWidth;
			}

			// increment y
			y += subRectHeight;
		}
	}

	// send rects
	if (!rects.empty()) {
		mSurfaceOutputModeEnabled ? paintSurface(rects) : paintBitmap(rects);
	}
}

static void adjustRectForPlanar(QRect &rect, const QRect &screen) {
	/* xfreerdp doesn't seen to like 1px size for planar, let's give
	 * it a little more */
	if (rect.width() < 4) {
		if (rect.right() + 4 > screen.right()) {
			/* update rect is at the right of the screen, increase the rect by the left */
			rect.setLeft(rect.left() - 4);
		} else {
			rect.setRight(rect.right() + 4);
		}
	}

	if (rect.height() < 4) {
		if (rect.bottom() + 4 > screen.bottom()) {
			/* update rect is at the bottom of the screen, increase the rect by the top */
			rect.setTop(rect.top() - 4);
		} else {
			rect.setBottom(rect.bottom() + 4);
		}
	}
}

bool QFreeRdpPeer::repaint_egfx(const QRegion &region, bool compress) {
	//if (!mSurfaceCreated && !initGfxDisplay())
	//	return false;

	//qDebug() << "repaint_egfx_raw(" << region << ")";
	SYSTEMTIME sTime;
	GetSystemTime(&sTime);

	RDPGFX_START_FRAME_PDU startFrame;
	startFrame.frameId = ++mFrameId;
	startFrame.timestamp = (UINT32)(sTime.wHour << 22U | sTime.wMinute << 16U |
			sTime.wSecond << 10U | sTime.wMilliseconds);
	if (mEgfx.rdpgfx->StartFrame(mEgfx.rdpgfx, &startFrame) != CHANNEL_RC_OK)
		return false;

	RDPGFX_SURFACE_COMMAND cmd;
	cmd.codecId = compress ? RDPGFX_CODECID_PLANAR : RDPGFX_CODECID_UNCOMPRESSED;
	cmd.surfaceId = 1;
	cmd.format = PIXEL_FORMAT_BGRA32;
	cmd.contextId = 1;
	cmd.data = nullptr;

	auto settings = mClient->context->settings;
	QSize peerSize(settings->DesktopWidth, settings->DesktopHeight);

	BYTE *data = nullptr;
	const QImage *src = mPlatform->getDesktopBits();
	QPoint origin = mPlatform->monitorsRegion().boundingRect().topLeft();
	int monitorIndex = 0;

	for (const QFreeRdpScreen *screen: mPlatform->monitors()) {
		QRect monitorGeometry = screen->geometry().translated(-origin);
		QPoint monitorOrigin = monitorGeometry.topLeft();

		UINT16 surfaceId;
		bool doCreateSurface = false;
		if (monitorIndex >= mEgfx.surfaces.size()) {
			surfaceId = mEgfx.surfaceCounter++;
			mEgfx.surfaces.push_back(EgfxSurface(surfaceId, screen->rdpGeometry()));
			doCreateSurface = true;
		} else {
			auto & surface = mEgfx.surfaces[monitorIndex];
			if (surface.rdpGeom != screen->rdpGeometry()) {
				qDebug("recreating surface for screen %d", monitorIndex);
				RDPGFX_DELETE_SURFACE_PDU deleteSurface = {
					surface.surfaceId
				};

				if (mEgfx.rdpgfx->DeleteSurface(mEgfx.rdpgfx, &deleteSurface) != CHANNEL_RC_OK) {
					qDebug("error deleting surface");
					return false;
				}
				surface.rdpGeom = screen->rdpGeometry();
				doCreateSurface = true;
			}
			surfaceId = surface.surfaceId;
		}

		if (doCreateSurface) {
			QRect allGeometry = mPlatform->mAllMonitorsRegion.boundingRect();

			RDPGFX_CREATE_SURFACE_PDU createSurface = {
				surfaceId,
				(UINT16)monitorGeometry.width(), (UINT16)monitorGeometry.height(),
				GFX_PIXEL_FORMAT_XRGB_8888
			};

			if (mEgfx.rdpgfx->CreateSurface(mEgfx.rdpgfx, &createSurface) != CHANNEL_RC_OK) {
				qDebug("error creating surface");
				return false;
			}

			RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU surfaceToOutput = {
				surfaceId, 0,
				(UINT32)(monitorGeometry.left() - allGeometry.left()),
				(UINT32)(monitorGeometry.top() - allGeometry.top())
			};
			if (mEgfx.rdpgfx->MapSurfaceToOutput(mEgfx.rdpgfx, &surfaceToOutput) != CHANNEL_RC_OK) {
				qDebug("error mapping surface to output");
				return false;
			}
		} else {
		}

		cmd.surfaceId = surfaceId;

		for (auto rect : region.intersected(monitorGeometry)) {
			//qDebug() << "repaint_egfx(" << rect << ")";
			if (compress)
				adjustRectForPlanar(rect, monitorGeometry);

			cmd.left = rect.left();
			cmd.top = rect.top();
			cmd.right = rect.right() + 1;
			cmd.bottom = rect.bottom() + 1;
			cmd.width = rect.width();
			cmd.height = rect.height();

			if (compress) {
				auto planar = mClient->context->codecs->planar;
				freerdp_bitmap_planar_context_reset(planar, cmd.width, cmd.height);
				freerdp_planar_topdown_image(planar, TRUE);
				const BYTE *srcBytes = (const BYTE *)src->bits() + (cmd.top * src->bytesPerLine()) + (cmd.left * 4);
				cmd.length = 0;
				cmd.data = freerdp_bitmap_compress_planar(planar, srcBytes, PIXEL_FORMAT_BGRA32,
						cmd.width, cmd.height, src->bytesPerLine(), NULL, &cmd.length);
				if (!cmd.data && cmd.length != 0) {
					qDebug("error while planar compression");
					return false;
				}
			} else {
				BYTE *tmp = (BYTE *)realloc(data, cmd.width * cmd.height * 4);
				if (!tmp) {
					free(data);
					qDebug("unable to realloc tmpData");
					return false;
				}
				data = tmp;

				if (!freerdp_image_copy(data, cmd.format, 0 /*nDstStep*/, 0, 0, cmd.width, cmd.height,
							(const BYTE *)src->bits(), PIXEL_FORMAT_BGRA32, src->bytesPerLine(),
							rect.left(), rect.top(), nullptr, 0)) {
					free(data);
					qDebug("error during freerdp_image_copy()");
					return false;
				}

				cmd.data = data;
				cmd.length = cmd.width * cmd.height * 4;
			}
			cmd.left -= monitorOrigin.x();
			cmd.right -= monitorOrigin.x();
			cmd.top -= monitorOrigin.y();
			cmd.bottom -= monitorOrigin.y();

			if (mEgfx.rdpgfx->SurfaceCommand(mEgfx.rdpgfx, &cmd) != CHANNEL_RC_OK) {
				free(data);
				qDebug("error during surfaceCommand");
				return false;
			}

			if (compress)
				free(cmd.data);
		}

		monitorIndex++;
	}

	RDPGFX_END_FRAME_PDU endFrame = { mFrameId };
	if (mEgfx.rdpgfx->EndFrame(mEgfx.rdpgfx, &endFrame) != CHANNEL_RC_OK)
		return false;

	return true;
}


void QFreeRdpPeer::updateMouseButtonsFromFlags(DWORD flags, bool &down, bool extended) {
	mCurrentButton = Qt::NoButton;

	if(!extended) {
		if (flags & PTR_FLAGS_BUTTON1)
			mCurrentButton = Qt::LeftButton;
		else if (flags & PTR_FLAGS_BUTTON2)
			mCurrentButton = Qt::RightButton;
		else if (flags & PTR_FLAGS_BUTTON3)
			mCurrentButton = Qt::MiddleButton;
	} else {
		qWarning("extended mouse button not handled");
	}

	if ((down = (flags & PTR_FLAGS_DOWN)))
		mLastButtons |= mCurrentButton;
	else
		mLastButtons &= (~mCurrentButton);
}

QSize QFreeRdpPeer::getGeometry() {
	return QSize(mClient->context->settings->DesktopWidth, mClient->context->settings->DesktopHeight);
}

bool QFreeRdpPeer::setBlankCursor()
{
	POINTER_SYSTEM_UPDATE system_pointer = { 0 };
	system_pointer.type = SYSPTR_NULL;

	rdpPointerUpdate* pointer = mClient->context->update->pointer;
	return pointer->PointerSystem(mClient->context, &system_pointer);
}

freerdp_peer *QFreeRdpPeer::freerdpPeer() const {
	return mClient;
}

bool QFreeRdpPeer::setPointer(const POINTER_LARGE_UPDATE *largePointer, Qt::CursorShape newShape)
{
	if (!(mFlags & PEER_ACTIVATED))
		return true;

	bool isNew, isUpdate;
	UINT16 cacheIndex = getCursorCacheIndex(newShape, isNew, isUpdate);
	/*qDebug() << "pointer cache entry" << cacheIndex
			<< " isNew=" << isNew
			<< " isUpdate=" << isUpdate;*/

	rdpPointerUpdate* pointer = mClient->context->update->pointer;
	if (isNew || isUpdate) {
		POINTER_NEW_UPDATE npointer;
		POINTER_COLOR_UPDATE *lpointer = &npointer.colorPtrAttr;

		npointer.xorBpp = largePointer->xorBpp;
		lpointer->cacheIndex = cacheIndex;
		lpointer->width = largePointer->width;
		lpointer->height = largePointer->height;
#if FREERDP_VERSION_MAJOR == 3 && FREERDP_VERSION_MINOR >= 1 || FREERDP_VERSION_MAJOR > 3
		lpointer->hotSpotX = largePointer->hotSpotX;
		lpointer->hotSpotY = largePointer->hotSpotY;
#else
		lpointer->xPos = largePointer->hotSpotX;
		lpointer->yPos = largePointer->hotSpotY;
#endif
		lpointer->xorMaskData = largePointer->xorMaskData;
		lpointer->lengthXorMask = largePointer->lengthXorMask;
		lpointer->andMaskData = largePointer->andMaskData;
		lpointer->lengthAndMask = largePointer->lengthAndMask;

		return pointer->PointerNew(mClient->context, &npointer);
	}

	POINTER_CACHED_UPDATE update;
	update.cacheIndex = cacheIndex;
	return pointer->PointerCached(mClient->context, &update);
}

UINT16 QFreeRdpPeer::getCursorCacheIndex(Qt::CursorShape shape, bool &isNew, bool &isUpdate)
{
	auto it = mCursorCache.find(shape);
	if (it != mCursorCache.end()) {
		it->lastUse = GetTickCount64();
		isNew = false;
		isUpdate = false;
		return it->cacheIndex;
	}

	isNew = true;
	isUpdate = false;
	auto settings = mClient->context->settings;
	UINT16 ret = mCursorCache.size();
	if (settings->PointerCacheSize && (mCursorCache.size() == (int)settings->PointerCacheSize)) {
		auto it = mCursorCache.begin();
		auto removeIt = mCursorCache.begin();

		it++;
		for( ; it != mCursorCache.end(); it++) {
			if (it->lastUse < removeIt->lastUse)
				removeIt = it;
		}

		ret = removeIt->cacheIndex;
		mCursorCache.erase(removeIt);
		isUpdate = true;
		qDebug() << "removing pointer cache index " << ret;
	}

	mCursorCache[shape] = {ret, GetTickCount64()};

	return ret;
}

void QFreeRdpPeer::paintBitmap(const QVector<QRect> &rects) {
	rdpUpdate *update = mClient->context->update;

	// use bitmap update
	BITMAP_UPDATE *bitmapUpdate = (BITMAP_UPDATE*) malloc(sizeof(BITMAP_UPDATE));

	bitmapUpdate->skipCompression = false;

	// set bitmap rectangles number
	bitmapUpdate->number = rects.size();
//	qDebug() << "number of rectangles : " << bitmapUpdate->number << endl;

	bitmapUpdate->rectangles = (BITMAP_DATA*)malloc(bitmapUpdate->number * sizeof(BITMAP_DATA));

	if (bitmapUpdate->rectangles == NULL)
		return;

	// get source image bits
	const QImage *src = mPlatform->getDesktopBits();

	int i = 0;
	auto settings = mClient->context->settings;

	// fill bitmap data
	foreach(QRect rect, rects) {
		BITMAP_DATA bitmapData;

		// coord
		bitmapData.destLeft = rect.left();
		bitmapData.destTop = rect.top();
		bitmapData.destRight = rect.right();
		bitmapData.destBottom = rect.bottom();
		bitmapData.flags = 0x401; /* BITMAP_COMPRESSION | NO_BITMAP_COMPRESSION_HDR */

		// width (inclusive bounds)
		// freerdp requires it to be a multiple of 4
		UINT32 width = (bitmapData.destRight - bitmapData.destLeft) + 1;
		bitmapData.width = qCeil(qreal(width) / 16) * 16;

		// height (inclusive bounds)
		bitmapData.height = rect.bottom() - rect.top() + 1;

		// bits per pixel
		UINT32 origFormat = PIXEL_FORMAT_BGRX32;
		switch (src->format()) {
			case QImage::Format_RGB555 :
				origFormat = PIXEL_FORMAT_RGB15;
				break;
			case QImage::Format_RGB16 :
				origFormat = PIXEL_FORMAT_RGB16;
				break;
			case QImage::Format_RGB888 :
				origFormat = PIXEL_FORMAT_RGB24;
				break;
			case QImage::Format_RGB32 :
			default :
				origFormat = PIXEL_FORMAT_BGRX32;
				break;
		}
		bitmapData.bitsPerPixel = settings->ColorDepth;

		// options
		bitmapData.cbCompFirstRowSize = 0x0000;
		bitmapData.cbCompMainBodySize = 0x0000;
		bitmapData.cbScanWidth = 0x0000;
		bitmapData.cbUncompressedSize = 0x0000;
		bitmapData.bitmapDataStream = NULL;

		// compute subRect
		QRect subRect(QPoint(bitmapData.destLeft, bitmapData.destTop), QSize(bitmapData.width, bitmapData.height));
		QImage image = src->copy(subRect);

		auto codecs = mClient->context->codecs;
		if (settings->ColorDepth == 32) {
			// bpp 32 bits for client -> use planar codec
			UINT32 dstSize;

			bitmapData.bitmapDataStream = freerdp_bitmap_compress_planar(codecs->planar, image.bits(), PIXEL_FORMAT_BGRX32,
					bitmapData.width, bitmapData.height, bitmapData.width * 4, NULL, &dstSize);

			bitmapData.bitmapLength = dstSize;

		} else {
			// bpp is other than 32 bits
			UINT32 dstBitsPerPixel = settings->ColorDepth;
			UINT32 dstBytesPerPixel =  ((dstBitsPerPixel + 7) / 8);
			UINT32 srcBytesPerPixel = (image.depth() + 7) / 8;
			// allocate a buffer for the compressed image
			// if it is too small, freerdp crashes on the following assertion:
			// [FATAL][com.freerdp.winpr.assert] - Stream_GetRemainingCapacity(_s) >= _n
			// so let's use the size of the original (uncompressed) image
			UINT32 DstSize = bitmapData.width * bitmapData.height * srcBytesPerPixel;
			BYTE* buffer = (BYTE*) malloc(DstSize);

			BOOL status = interleaved_compress(codecs->interleaved, buffer, &DstSize,
											bitmapData.width, bitmapData.height,
											image.bits(), origFormat, bitmapData.width * srcBytesPerPixel,
											0, 0, &mClient->context->gdi->palette,
											dstBitsPerPixel);

			if (!status) {
				qCritical() << "Can not interleaved compress";
			}

			bitmapData.bitmapDataStream = buffer;
			bitmapData.bitmapLength = DstSize;
			bitmapData.bitsPerPixel = dstBitsPerPixel;
			bitmapData.cbScanWidth = bitmapData.width * dstBytesPerPixel;
			bitmapData.cbUncompressedSize = bitmapData.width * bitmapData.height * dstBytesPerPixel;
			bitmapData.cbCompMainBodySize = bitmapData.bitmapLength;
			bitmapData.compressed = true;
		}


		// add bitmap data to bitmap update
		bitmapUpdate->rectangles[i] = bitmapData;
		i++;

		// debug
//		qDebug() << "BITMAP [" << bitmapData.destLeft << "," << bitmapData.destTop
//				<< "," << bitmapData.destRight << "," << bitmapData.destBottom << "]"
//				<< "[w " << bitmapData.width << ",h " << bitmapData.height << "]"
//				<< "[bpp " << bitmapData.bitsPerPixel << ",f " << bitmapData.flags
//				<< ",bmpLen " << bitmapData.bitmapLength << "]"
//				<< "[cbCompFirstRowSize " << bitmapData.cbCompFirstRowSize
//				<< ", cbCompMainBodySize " << bitmapData.cbCompMainBodySize
//				<< ",cbScanWidth " << bitmapData.cbScanWidth
//				<< ",cbUncompressedSize " << bitmapData.cbUncompressedSize
//				<< ",compressed " << bitmapData.compressed << "]";
	}

	// send bitmap update
	update->BitmapUpdate(mClient->context, bitmapUpdate);

	// free bitmap
	for (UINT32 i = 0; i < bitmapUpdate->number; ++i){
		free(bitmapUpdate->rectangles[i].bitmapDataStream);
	}

	free(bitmapUpdate->rectangles);
	free(bitmapUpdate);
}

void QFreeRdpPeer::paintSurface(const QVector<QRect> &rects) {
	rdpUpdate *update = mClient->context->update;
	SURFACE_BITS_COMMAND cmd;
	SURFACE_FRAME_MARKER marker;
	marker.frameId = 0;
	marker.frameId++;
	marker.frameAction = SURFACECMD_FRAMEACTION_BEGIN;
	update->SurfaceFrameMarker(mClient->context, &marker);

	const QImage *src = mPlatform->getDesktopBits();
	foreach(QRect rect, rects) {
		cmd.bmp.width = rect.width();
		cmd.destLeft = rect.left();
		cmd.destRight = rect.right() + 1;

		// we split the surface to fit in the negotiated packet size
		// 16 is an approximation of bytes required for the surface command
		int heightIncrement = mClient->context->settings->MultifragMaxRequestSize / (16 + cmd.bmp.width * 4);
		int remainingHeight = rect.height();
		int top = rect.top();
		while(remainingHeight) {
			cmd.bmp.height = (remainingHeight > heightIncrement) ? heightIncrement : remainingHeight;
			cmd.destTop = top;
			cmd.destBottom = top + cmd.bmp.height;

			QRect subRect(QPoint(cmd.destLeft, cmd.destTop), QSize(cmd.bmp.width, cmd.bmp.height));

			cmd.bmp.bitmapDataLength = cmd.bmp.width * cmd.bmp.height * 4;
			cmd.bmp.bitmapData = (BYTE *)malloc(cmd.bmp.bitmapDataLength * sizeof(BYTE));

			if (mNsCodecSupported) {
				// set codec params
				cmd.bmp.bpp = 32;
				cmd.bmp.codecID = mClient->context->settings->NSCodecId;

				// get bytes in cmd.bitmapData (no vertical flip)
				qimage_subrect(subRect, src, cmd.bmp.bitmapData, false);

				// create stream of data
				wStream* s = Stream_New(NULL, cmd.bmp.bitmapDataLength);
				Stream_Clear(s);
				Stream_SetPosition(s, 0);

				// compute data with NSC codec
				RdpPeerContext *ctx = (RdpPeerContext *)mClient->context;

				nsc_compose_message(ctx->nsc_context, s,
		                    cmd.bmp.bitmapData, subRect.width(), subRect.height(), subRect.width() * 4);

				cmd.bmp.bitmapDataLength = Stream_GetPosition(s);
				cmd.bmp.bitmapData = Stream_Buffer(s);
			} else {
				// no codec used
				cmd.bmp.bpp = 32;
				cmd.bmp.codecID = 0;

				// get bytes in cmd.bitmapData (vertical flip)
				qimage_subrect(subRect, src, cmd.bmp.bitmapData, true);
			}

			update->SurfaceBits(mClient->context, &cmd);

			free(cmd.bmp.bitmapData);

			remainingHeight -= cmd.bmp.height;
			top += cmd.bmp.height;
		}
	}
	marker.frameAction = SURFACECMD_FRAMEACTION_END;
	update->SurfaceFrameMarker(mClient->context, &marker);
}
