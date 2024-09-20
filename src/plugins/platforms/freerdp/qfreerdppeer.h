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

#include <memory>

#include <freerdp/peer.h>
#include <freerdp/pointer.h>
#include <freerdp/server/rdpgfx.h>

#include <QImage>
#include <QMap>

#ifndef NO_XKB_SUPPORT
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#endif

#include "qfreerdpcompositor.h"

QT_BEGIN_NAMESPACE

class QFreeRdpPlatform;
class QFreerdpPeerClipboard;
class QSocketNotifier;

/**
 * @brief a peer connected in RDP to the Qt5 backend
 */
class QFreeRdpPeer : public QObject {
	friend class QFreerdpPeerClipboard;
	friend class QFreeRdpPlatform;

    Q_OBJECT

public:
    QFreeRdpPeer(QFreeRdpPlatform *platform, freerdp_peer* client);
    ~QFreeRdpPeer();

    bool init();

    QSize getGeometry();

    bool setBlankCursor();
    bool setPointer(const POINTER_LARGE_UPDATE *pointer, Qt::CursorShape newShape);
    freerdp_peer *freerdpPeer() const;

protected:
	// Sends bitmap updates for
	// - the rectangles contained in `region`
	// - rectangles internally marked as dirty
	void repaint(const QRegion &rect);
	void repaint_raw(const QRegion &rect);
	bool repaint_egfx(const QRegion &rect, bool compress);
	void handleVirtualKeycode(quint32 flags, quint32 vk_code);
	void updateMouseButtonsFromFlags(DWORD flags, bool &down, bool extended);
	void updateModifiersState(bool capsLock, bool numLock, bool scrollLock, bool kanaLock);
	void init_display(freerdp_peer* client);
	UINT16 getCursorCacheIndex(Qt::CursorShape shape, bool &isNew, bool &isUpdate);
	bool initializeChannels();

	bool frameAck(UINT32 frameId);
	bool initGfxDisplay();
	bool egfx_caps_test(const RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise, UINT32 version, UINT &rc);
	void checkDrdynvcState();

public slots:
	void incomingBytes(int sock);
	void channelTraffic(int sock);


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

	static UINT rdpgfx_caps_advertise(RdpgfxServerContext* context, const RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise);
	static UINT rdpgfx_frame_acknowledge(RdpgfxServerContext* context, const RDPGFX_FRAME_ACKNOWLEDGE_PDU* frameAcknowledge);
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
    quint32 mKeyTime;

    /** @brief how to render screen content updates */
    enum RenderMode {
		RENDER_BITMAP_UPDATES,
		RENDER_EGFX,
    };

    bool mSurfaceOutputModeEnabled;
    bool mNsCodecSupported;
    QFreeRdpCompositor mCompositor;
    RenderMode mRenderMode;
    QRegion mDirtyRegion;

    HANDLE mVcm;
    QFreerdpPeerClipboard *mClipboard;
    RdpgfxServerContext* mRdpgfx;
    bool mGfxOpened;
    bool mSurfaceCreated;
    UINT16 mSurfaceId;
    UINT32 mFrameId;

    /** @brief a cursor cache entry */
	struct CursorCacheItem {
		UINT16 cacheIndex;
		UINT64 lastUse;
	};
	typedef QMap<Qt::CursorShape, CursorCacheItem> CursorCache;
	CursorCache mCursorCache;


#ifndef NO_XKB_SUPPORT
    struct xkb_context *mXkbContext;
    struct xkb_keymap *mXkbKeymap;
    struct xkb_state *mXkbState;
    struct xkb_compose_state *mXkbComposeState;
    struct xkb_compose_table *mXkbComposeTable;

	xkb_mod_index_t mCapsLockModIndex;
	xkb_mod_index_t mNumLockModIndex;
	xkb_mod_index_t mScrollLockModIndex;
#endif

private :
	void paintBitmap(const QVector<QRect> &rects);
	void paintSurface(const QVector<QRect> &rects);
	BOOL detectDisplaySettings(freerdp_peer* client);
	BOOL configureDisplayLegacyMode(rdpSettings *settings);
	BOOL configureOptimizeMode(rdpSettings *settings);

#ifndef NO_XKB_SUPPORT
	xkb_keysym_t getXkbSymbol(const quint32 &scanCode, const bool &isDown);
#endif
};

QT_END_NAMESPACE

#endif /* __QFREERDPPEER_H___ */
