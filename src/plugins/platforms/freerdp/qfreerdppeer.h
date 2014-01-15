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

#include <freerdp/peer.h>
#include <QImage>

#ifndef NO_XKB_SUPPORT
#include <xkbcommon/xkbcommon.h>
#endif


QT_BEGIN_NAMESPACE

class QFreeRdpPlatform;

/**
 * @brief a peer connected in RDP to the Qt5 backend
 */
class QFreeRdpPeer : public QObject {
    Q_OBJECT

public:
    QFreeRdpPeer(QFreeRdpPlatform *platform, freerdp_peer* client);
    ~QFreeRdpPeer();

    bool init();
    void repaint(const QRegion &rect);

    QSize getGeometry();

protected:
	void repaint_raw(const QRegion &rect);
	void handleVirtualKeycode(quint32 flags, quint32 vk_code);
	void updateModifiersState(bool capsLock, bool numLock, bool scrollLock, bool kanaLock);

public slots:
	void incomingBytes(int sock);

protected:
	/** freerdp callbacks
	 * @{ */
	static BOOL xf_peer_capabilities(freerdp_peer* client);
	static BOOL xf_peer_post_connect(freerdp_peer *client);
	static BOOL xf_peer_activate(freerdp_peer *client);
	static void xf_mouseEvent(rdpInput *input, UINT16 flags, UINT16 x, UINT16 y);
	static void xf_extendedMouseEvent(rdpInput *input, UINT16 flags, UINT16 x, UINT16 y);
	static void xf_input_synchronize_event(rdpInput *input, UINT32 flags);
	static void xf_input_keyboard_event(rdpInput *input, UINT16 flags, UINT16 code);
	static void xf_input_unicode_keyboard_event(rdpInput *input, UINT16 flags, UINT16 code);
	static void xf_suppress_output(rdpContext* context, BYTE allow, RECTANGLE_16 *area);
	/** @} */


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
#ifndef NO_XKB_SUPPORT
    struct xkb_context *mXkbContext;
    struct xkb_keymap *mXkbKeymap;
    struct xkb_state *mXkbState;

	xkb_mod_index_t mCapsLockModIndex;
	xkb_mod_index_t mNumLockModIndex;
	xkb_mod_index_t mScrollLockModIndex;
#endif
};

QT_END_NAMESPACE

#endif /* __QFREERDPPEER_H___ */
