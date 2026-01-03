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
#ifndef __QFREERDPPEER_H__
#define __QFREERDPPEER_H__

#include <freerdp/peer.h>
#include <freerdp/pointer.h>
#include <freerdp/server/rdpgfx.h>
#include <freerdp/server/disp.h>

#include <QImage>
#include <QMap>


#include "qfreerdpcompositor.h"
#include "qfreerdppeerkeyboard.h"

QT_BEGIN_NAMESPACE

class QFreeRdpPlatform;
class QFreerdpPeerClipboard;
class QSocketNotifier;

/**
 * @brief a peer connected in RDP to the Qt5 backend
 */
class QFreeRdpPeer : public QObject {
    Q_OBJECT

	friend class QFreerdpPeerClipboard;
	friend class QFreeRdpPlatform;

public:
	/** @brief an egfx surface */
	struct EgfxSurface {
		EgfxSurface(UINT16 pSurfaceId, const QRect &pRdpGeom);

		UINT16 surfaceId;
		QRect rdpGeom;
	};

	/** @brief state for the egfx channel */
	struct EgfxState {
		EgfxState();
		bool opened;
		RdpgfxServerContext* rdpgfx;
		UINT16 surfaceCounter;
		QList<EgfxSurface> surfaces;
	};

	/** @brief state for the display channel */
	struct DispState {
		DispState();
		bool opened;
		DispServerContext* channel;
		UINT32 channelId;
	};

public:
    QFreeRdpPeer(QFreeRdpPlatform *platform, freerdp_peer* client);
    virtual ~QFreeRdpPeer();

    bool init();

    QSize getGeometry();

    bool setBlankCursor();
    bool setPointer(const POINTER_LARGE_UPDATE *pointer, Qt::CursorShape newShape);
    freerdp_peer *freerdpPeer() const;

protected:
	// Sends bitmap updates for
	// - the rectangles contained in `region`
	// - rectangles internally marked as dirty
	void repaint(const QRegion &rect, bool useCompositorCache = true);
	void repaint_raw(const QRegion &rect);
	bool repaint_egfx(const QRegion &rect, bool compress);
	void handleVirtualKeycode(quint32 flags, quint32 vk_code);
	void updateMouseButtonsFromFlags(DWORD flags, bool &down, bool extended);
	bool init_display(freerdp_peer* client);
	UINT16 getCursorCacheIndex(Qt::CursorShape shape, bool &isNew, bool &isUpdate);
	bool initializeChannels();

	bool frameAck(UINT32 frameId);
	bool initGfxDisplay();
	bool egfx_caps_test(const RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise, UINT32 version, UINT &rc);
	void checkDrdynvcState();

signals:
	void updatedMonitors(DISPLAY_CONTROL_MONITOR_LAYOUT_PDU* pdu);

public slots:
	void incomingBytes(int sock);
	void channelTraffic(int sock);
	void onUpdatedMonitors(DISPLAY_CONTROL_MONITOR_LAYOUT_PDU* pdu);


protected:
	/** FreeRDP callbacks
	 * @{ */
	static BOOL xf_peer_capabilities(freerdp_peer* client);
	static BOOL xf_peer_activate(freerdp_peer *client);
	static BOOL xf_peer_post_connect(freerdp_peer *client);
	static BOOL xf_mouseEvent(rdpInput *input, UINT16 flags, UINT16 x, UINT16 y);
	static BOOL xf_extendedMouseEvent(rdpInput *input, UINT16 flags, UINT16 x, UINT16 y);
	static BOOL xf_input_synchronize_event(rdpInput *input, UINT32 flags);
	static BOOL xf_input_keyboard_event(rdpInput *input, UINT16 flags, UINT8 code);
	static BOOL xf_input_unicode_keyboard_event(rdpInput *input, UINT16 flags, UINT16 code);
	// [MS-RDPBGGR] 2.2.11.2 Client Refresh Rect PDU
	static BOOL xf_refresh_rect(rdpContext *context, BYTE count, const RECTANGLE_16* areas);
	// [MS-RDPBGGR] 2.2.11.2 Client Suppress Output PDU
	static BOOL xf_suppress_output(rdpContext* context, BYTE allow, const RECTANGLE_16 *area);
	static BOOL xf_surface_frame_acknowledge(rdpContext* context, UINT32 frameId);

	static BOOL dynvc_dynChannelCreationStatus(void* userdata, UINT32 channelId, INT32 creationStatus);

	static UINT rdpgfx_caps_advertise(RdpgfxServerContext* context, const RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise);
	static UINT rdpgfx_frame_acknowledge(RdpgfxServerContext* context, const RDPGFX_FRAME_ACKNOWLEDGE_PDU* frameAcknowledge);

	static BOOL display_channelIdAssigned(DispServerContext* context, UINT32 channelId);
	static UINT display_monitorLayout(DispServerContext* context, const DISPLAY_CONTROL_MONITOR_LAYOUT_PDU* pdu);
	/** @} */

	void dropSocketNotifier(QSocketNotifier *notifier);

protected:
	/** @brief flags about a connected RDP peer */
	enum PeerFlags {
		PEER_ACTIVATED = 0x00001,
		PEER_OUTPUT_DISABLED = 0x0002,
		PEER_WAITING_DYNVC = 0x0004,
		PEER_WAITING_GRAPHICS = 0x0008
	};
	QFlags<PeerFlags> mFlags;

    QFreeRdpPlatform *mPlatform;
    freerdp_peer *mClient;
    int mBogusCheckFileDescriptor;

    QPoint mLastMousePos;
    Qt::MouseButtons mLastButtons;
    Qt::MouseButton mCurrentButton;
    QFreeRdpPeerKeyboard mKeyboard;

    /** @brief how to render screen content updates */
    enum RenderMode {
		RENDER_BITMAP_UPDATES,
		RENDER_EGFX,
    };

    bool mFirstFrame;
    bool mSurfaceOutputModeEnabled;
    bool mNsCodecSupported;
    QFreeRdpCompositor mCompositor;
    RenderMode mRenderMode;

    HANDLE mVcm;
    QFreerdpPeerClipboard *mClipboard;
    UINT32 mFrameId;

    EgfxState mEgfx;
    DispState mDisplay;

    /** @brief a cursor cache entry */
	struct CursorCacheItem {
		UINT16 cacheIndex;
		UINT64 lastUse;
	};
	typedef QMap<Qt::CursorShape, CursorCacheItem> CursorCache;
	CursorCache mCursorCache;


private :
	void sendFullRefresh(rdpSettings *settings);
	void paintBitmap(const QVector<QRect> &rects);
	void paintSurface(const QVector<QRect> &rects);
	BOOL detectDisplaySettings(freerdp_peer* client);
	BOOL configureDisplayLegacyMode(rdpSettings *settings);
	BOOL configureOptimizeMode(rdpSettings *settings);
};

QT_END_NAMESPACE

#endif /* __QFREERDPPEER_H___ */
