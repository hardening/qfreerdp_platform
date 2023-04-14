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
#ifndef __QFREERDPPEER_H__
#define __QFREERDPPEER_H__

#include <memory>

#include <freerdp/peer.h>

#include <QImage>
#include <QMap>

#ifndef NO_XKB_SUPPORT
#include <xkbcommon/xkbcommon.h>
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

    Q_OBJECT

public:
    QFreeRdpPeer(QFreeRdpPlatform *platform, freerdp_peer* client);
    ~QFreeRdpPeer();

    bool init();
    void repaintWithCompositor(const QRegion &rect);

    QSize getGeometry();

protected:
	void repaint(const QRegion &rect);
	void repaint_raw(const QRegion &rect);
	void handleVirtualKeycode(quint32 flags, quint32 vk_code);
	void updateModifiersState(bool capsLock, bool numLock, bool scrollLock, bool kanaLock);
	void init_display(freerdp_peer* client);

public slots:
	void incomingBytes(int sock);
	void channelTraffic(int sock);


protected:
	/** freerdp callbacks
	 * @{ */
	static BOOL xf_peer_capabilities(freerdp_peer* client);
	static BOOL xf_peer_post_connect(freerdp_peer *client);
	static BOOL xf_peer_activate(freerdp_peer *client);
	static BOOL xf_mouseEvent(rdpInput *input, UINT16 flags, UINT16 x, UINT16 y);
	static BOOL xf_extendedMouseEvent(rdpInput *input, UINT16 flags, UINT16 x, UINT16 y);
	static BOOL xf_input_synchronize_event(rdpInput *input, UINT32 flags);
	static BOOL xf_input_keyboard_event(rdpInput *input, UINT16 flags, UINT8 code);
	static BOOL xf_input_unicode_keyboard_event(rdpInput *input, UINT16 flags, UINT16 code);
	static BOOL xf_refresh_rect(rdpContext *context, BYTE count, const RECTANGLE_16* areas);
	static BOOL xf_suppress_output(rdpContext* context, BYTE allow, const RECTANGLE_16 *area);
	/** @} */

	void dropSocketNotifier(QSocketNotifier *notifier);
protected:
	/** @brief */
	enum PeerFlags {
		PEER_ACTIVATED = 0x00001,
		PEER_OUTPUT_DISABLED = 0x0002
	};
	QFlags<PeerFlags> mFlags;

    QFreeRdpPlatform *mPlatform;
    freerdp_peer *mClient;
    int mBogusCheckFileDescriptor;

    QPoint mLastMousePos;
    Qt::MouseButtons mLastButtons;
    quint32 mKeyTime;

    bool mSurfaceOutputModeEnabled;
    bool mNsCodecSupported;
    QFreeRdpCompositor mCompositor;

    HANDLE mVcm;
    QFreerdpPeerClipboard *mClipboard;


#ifndef NO_XKB_SUPPORT
    struct xkb_context *mXkbContext;
    struct xkb_keymap *mXkbKeymap;
    struct xkb_state *mXkbState;

	xkb_mod_index_t mCapsLockModIndex;
	xkb_mod_index_t mNumLockModIndex;
	xkb_mod_index_t mScrollLockModIndex;

	QMap<QString, bool> mKbdStateModifiers;
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
