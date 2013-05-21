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

#include <winpr/stream.h>
#include <freerdp/freerdp.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/codec/color.h>

#include "qfreerdppeer.h"
#include "qfreerdpwindow.h"
#include "qfreerdpscreen.h"
#include "qfreerdpplatform.h"
#include "qfreerdpwindowmanager.h"

#include <QAbstractEventDispatcher>
#include <QSocketNotifier>
#include <QDebug>
#include <QMutexLocker>
#include <QtGui/qpa/qwindowsysteminterface.h>
#include <X11/keysym.h>

struct RdpPeerContext {
	rdpContext _p;
	QFreeRdpPeer *rdpPeer;

	/* file descriptors and associated events */
	QSocketNotifier *events[32];

	RFX_CONTEXT *rfx_context;
	wStream *encode_stream;
	RFX_RECT *rfx_rects;
	NSC_CONTEXT *nsc_context;
};

static void
rdp_peer_context_new(freerdp_peer* client, RdpPeerContext* context)
{
	context->rfx_context = rfx_context_new();
	context->rfx_context->mode = RLGR3;
	context->rfx_context->width = client->settings->DesktopWidth;
	context->rfx_context->height = client->settings->DesktopHeight;
	rfx_context_set_pixel_format(context->rfx_context, RDP_PIXEL_FORMAT_B8G8R8A8);

	context->nsc_context = nsc_context_new();
	nsc_context_set_pixel_format(context->nsc_context, RDP_PIXEL_FORMAT_B8G8R8A8);

	context->encode_stream = Stream_New(0, 0x10000);
}

static void
rdp_peer_context_free(freerdp_peer* /*client*/, RdpPeerContext* context)
{
	if(!context)
		return;

	Stream_Free(context->encode_stream, TRUE);
	nsc_context_free(context->nsc_context);
	rfx_context_free(context->rfx_context);
	free(context->rfx_rects);
}

static void init_vk_translator(void);

QFreeRdpPeer::QFreeRdpPeer(QFreeRdpPlatform *platform, freerdp_peer* client) :
		mFlags(0),
		mPlatform(platform),
		mClient(client),
		mBogusCheckFileDescriptor(0),
		mLastButtons(Qt::NoButton),
		mXkbContext(0),
		mXkbKeymap(0),
		mXkbState(0)
{
	init_vk_translator();
}

QFreeRdpPeer::~QFreeRdpPeer() {
	for(int i = 0; i < 32; i++)
		;
}

void QFreeRdpPeer::xf_mouseEvent(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y) {
	RdpPeerContext *peerContext = (RdpPeerContext *)input->context;
	QFreeRdpPeer *peer = peerContext->rdpPeer;

	Qt::MouseButtons buttons;
	if (flags & PTR_FLAGS_BUTTON1)
		buttons |= Qt::LeftButton;
	if (flags & PTR_FLAGS_BUTTON2)
		buttons |= Qt::RightButton;
	if (flags & PTR_FLAGS_BUTTON3)
		buttons |= Qt::MiddleButton;

	if(flags & PTR_FLAGS_DOWN)
		peer->mLastButtons |= buttons;
	else
		peer->mLastButtons &= (~buttons);

	QFreeRdpWindowManager *windowManager = peer->mPlatform->mWindowManager;
	QWindow *window = windowManager->getWindowAt(QPoint(x, y));
	if(window) {
		//qDebug("%s: dest=%d flags=0x%x buttons=0x%x", __func__, window->winId(), flags, peer->mLastButtons);
		Qt::KeyboardModifiers modifiers = Qt::NoModifier;
		QPoint wTopLeft = window->geometry().topLeft();
	    QWindowSystemInterface::handleMouseEvent(window,
	    		QPoint(x - wTopLeft.x(), y - wTopLeft.y()),
	    		QPoint(x, y),
	    		peer->mLastButtons, modifiers
	    );

	    /*if(mLastButtons)
	    	windowManager->setActiveWindow((QFreeRdpWindow *)window->handle());*/
	}
}

void QFreeRdpPeer::xf_extendedMouseEvent(rdpInput* /*input*/, UINT16 /*flags*/, UINT16 /*x*/, UINT16 /*y*/) {
	//RdpPeerContext *peerContext = (RdpPeerContext *)input->context;
}


void QFreeRdpPeer::xf_input_synchronize_event(rdpInput* input, UINT32 /*flags*/)
{
	freerdp_peer* client = input->context->peer;
	//rdpPointerUpdate *pointer = client->update->pointer;
	RdpPeerContext *peerCtx = (RdpPeerContext *)input->context;


	/* disable pointer on the client side
	pointer->pointer_system.type = SYSPTR_NULL;
	pointer->PointerSystem(client->context, &pointer->pointer_system);
	*/

	/* sends a full refresh */
	qDebug() << "xf_input_synchronize_event()";
	QRect refreshRect(0, 0, client->settings->DesktopWidth, client->settings->DesktopHeight);

	QFreeRdpPeer *peer = peerCtx->rdpPeer;
	const QImage *src = peer->mPlatform->mScreen->getScreenBits();
	if(src)
		peer->repaint(QRegion(refreshRect), src);
}

#ifndef NO_XKB_SUPPORT
extern "C" {
extern DWORD KEYCODE_TO_VKCODE_EVDEV[128];
}
static uint32_t vk_to_keycode[256];
static void init_vk_translator(void)
{
	int i;

	memset(vk_to_keycode, 0, sizeof(vk_to_keycode));
	for(i = 0; i < 256; i++)
		vk_to_keycode[KEYCODE_TO_VKCODE_EVDEV[i] & 0xff] = i/* - 8*/;
}

static Qt::KeyboardModifiers translateModifiers(xkb_state *state)
{
    Qt::KeyboardModifiers ret = Qt::NoModifier;
    xkb_state_component cstate = xkb_state_component(XKB_STATE_DEPRESSED | XKB_STATE_LATCHED);

    if (xkb_state_mod_name_is_active(state, "Shift", cstate))
        ret |= Qt::ShiftModifier;
    if (xkb_state_mod_name_is_active(state, "Control", cstate))
        ret |= Qt::ControlModifier;
    if (xkb_state_mod_name_is_active(state, "Alt", cstate))
        ret |= Qt::AltModifier;
    if (xkb_state_mod_name_is_active(state, "Mod1", cstate))
        ret |= Qt::AltModifier;
    if (xkb_state_mod_name_is_active(state, "Mod4", cstate))
        ret |= Qt::MetaModifier;

    return ret;
}
#endif

static const uint32_t KeyTbl[] = {
    XK_Escape,                  Qt::Key_Escape,
    XK_Tab,                     Qt::Key_Tab,
    XK_ISO_Left_Tab,            Qt::Key_Backtab,
    XK_BackSpace,               Qt::Key_Backspace,
    XK_Return,                  Qt::Key_Return,
    XK_Insert,                  Qt::Key_Insert,
    XK_Delete,                  Qt::Key_Delete,
    XK_Clear,                   Qt::Key_Delete,
    XK_Pause,                   Qt::Key_Pause,
    XK_Print,                   Qt::Key_Print,

    XK_Home,                    Qt::Key_Home,
    XK_End,                     Qt::Key_End,
    XK_Left,                    Qt::Key_Left,
    XK_Up,                      Qt::Key_Up,
    XK_Right,                   Qt::Key_Right,
    XK_Down,                    Qt::Key_Down,
    XK_Prior,                   Qt::Key_PageUp,
    XK_Next,                    Qt::Key_PageDown,

    XK_Shift_L,                 Qt::Key_Shift,
    XK_Shift_R,                 Qt::Key_Shift,
    XK_Shift_Lock,              Qt::Key_Shift,
    XK_Control_L,               Qt::Key_Control,
    XK_Control_R,               Qt::Key_Control,
    XK_Meta_L,                  Qt::Key_Meta,
    XK_Meta_R,                  Qt::Key_Meta,
    XK_Alt_L,                   Qt::Key_Alt,
    XK_Alt_R,                   Qt::Key_Alt,
    XK_Caps_Lock,               Qt::Key_CapsLock,
    XK_Num_Lock,                Qt::Key_NumLock,
    XK_Scroll_Lock,             Qt::Key_ScrollLock,
    XK_Super_L,                 Qt::Key_Super_L,
    XK_Super_R,                 Qt::Key_Super_R,
    XK_Menu,                    Qt::Key_Menu,
    XK_Hyper_L,                 Qt::Key_Hyper_L,
    XK_Hyper_R,                 Qt::Key_Hyper_R,
    XK_Help,                    Qt::Key_Help,

    XK_KP_Space,                Qt::Key_Space,
    XK_KP_Tab,                  Qt::Key_Tab,
    XK_KP_Enter,                Qt::Key_Enter,
    XK_KP_Home,                 Qt::Key_Home,
    XK_KP_Left,                 Qt::Key_Left,
    XK_KP_Up,                   Qt::Key_Up,
    XK_KP_Right,                Qt::Key_Right,
    XK_KP_Down,                 Qt::Key_Down,
    XK_KP_Prior,                Qt::Key_PageUp,
    XK_KP_Next,                 Qt::Key_PageDown,
    XK_KP_End,                  Qt::Key_End,
    XK_KP_Begin,                Qt::Key_Clear,
    XK_KP_Insert,               Qt::Key_Insert,
    XK_KP_Delete,               Qt::Key_Delete,
    XK_KP_Equal,                Qt::Key_Equal,
    XK_KP_Multiply,             Qt::Key_Asterisk,
    XK_KP_Add,                  Qt::Key_Plus,
    XK_KP_Separator,            Qt::Key_Comma,
    XK_KP_Subtract,             Qt::Key_Minus,
    XK_KP_Decimal,              Qt::Key_Period,
    XK_KP_Divide,               Qt::Key_Slash,

    XK_ISO_Level3_Shift,        Qt::Key_AltGr,
    XK_Multi_key,               Qt::Key_Multi_key,
    XK_Codeinput,               Qt::Key_Codeinput,
    XK_SingleCandidate,         Qt::Key_SingleCandidate,
    XK_MultipleCandidate,       Qt::Key_MultipleCandidate,
    XK_PreviousCandidate,       Qt::Key_PreviousCandidate,

    XK_Mode_switch,             Qt::Key_Mode_switch,
    XK_script_switch,           Qt::Key_Mode_switch,

    0,                          0
};

static uint32_t translateKey(uint32_t sym, char *string, size_t size)
{
    Q_UNUSED(size);
    string[0] = '\0';

    if (sym >= XK_F1 && sym <= XK_F35)
        return Qt::Key_F1 + (int(sym) - XK_F1);

    for (int i = 0; KeyTbl[i]; i += 2)
        if (sym == KeyTbl[i])
            return KeyTbl[i + 1];

    string[0] = sym;
    string[1] = '\0';
    return toupper(sym);
}

static uint32_t keytime = 0;
void QFreeRdpPeer::xf_input_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	RdpPeerContext *peerCtx = (RdpPeerContext *)input->context;
	QFreeRdpPeer *rdpPeer = peerCtx->rdpPeer;
	rdpSettings *settings = rdpPeer->mClient->settings;

#ifndef NO_XKB_SUPPORT
	if(!rdpPeer->mXkbKeymap) {
		qWarning("%s: no keyboard support, event dropped", __func__);
		return;
	}
#endif

	if( !(flags & (KBD_FLAGS_DOWN|KBD_FLAGS_RELEASE)) ) {
		qWarning("%s: notified for nothing, flags=%x", __func__, flags);
		return;
	}

	uint32_t full_code = code;
	if(flags & KBD_FLAGS_EXTENDED)
		full_code |= KBD_FLAGS_EXTENDED;

	uint32_t vk_code = GetVirtualKeyCodeFromVirtualScanCode(full_code, settings->KeyboardSubType);
	if(vk_code > 0xff) {
		qWarning("invalid vk_code %x", vk_code);
		return;
	}
	uint32_t scan_code = vk_to_keycode[vk_code];
	bool isDown = (flags & KBD_FLAGS_DOWN);

	QFreeRdpWindow *focusWindow = rdpPeer->mPlatform->mWindowManager->getActiveWindow();
	if(!focusWindow) {
		qWarning("%s: no windows has the focus", __func__);
		return;
	}

#ifndef NO_XKB_SUPPORT
    const xkb_keysym_t *syms = 0;
    uint32_t numSyms = xkb_key_get_syms(rdpPeer->mXkbState, scan_code, &syms);
    xkb_state_update_key(rdpPeer->mXkbState, scan_code, (isDown ? XKB_KEY_DOWN : XKB_KEY_UP));

	qWarning("%s: numSyms:%d code=0x%x vkCode=0x%x scanCode=0x%x", __func__, numSyms,
			full_code, vk_code, scan_code);
    if (numSyms == 1) {
        xkb_keysym_t xsym = syms[0];
        Qt::KeyboardModifiers modifiers = translateModifiers(rdpPeer->mXkbState);
        QEvent::Type type = isDown ? QEvent::KeyPress : QEvent::KeyRelease;

        char s[2];
        uint32_t qtsym = translateKey(xsym, s, sizeof s);

        qWarning("%s: xsym=0x%x qtsym=0x%x modifiers=0x%x", __func__, xsym, qtsym, (int)modifiers);
		QWindowSystemInterface::handleExtendedKeyEvent(focusWindow->window(),
													   ++keytime, type, qtsym,
													   modifiers,
													   0, 0, 0,
													   QString::fromLatin1(s));
    } else {
    	qDebug("no sending anything");
    }
#endif
}

void QFreeRdpPeer::xf_input_unicode_keyboard_event(rdpInput* /*input*/, UINT16 flags, UINT16 code)
{
	qDebug("Client sent a unicode keyboard event (flags:0x%X code:0x%X)\n", flags, code);
}


void QFreeRdpPeer::xf_suppress_output(rdpContext *context, BYTE allow, RECTANGLE_16* /*area*/) {
	qDebug("xf_suppress_output(allow=%d)", allow);

	RdpPeerContext *peerContext = (RdpPeerContext *)context;
	QFreeRdpPeer *rdpPeer = peerContext->rdpPeer;
	if(allow)
		rdpPeer->mFlags &= (~PEER_OUTPUT_DISABLED);
	else
		rdpPeer->mFlags |= PEER_OUTPUT_DISABLED;
}

BOOL QFreeRdpPeer::xf_peer_capabilities(freerdp_peer* /*client*/) {
	return TRUE;
}

struct rdp_to_xkb_keyboard_layout {
	UINT32 rdpLayoutCode;
	const char *xkbLayout;
};

/* picked from http://technet.microsoft.com/en-us/library/cc766503(WS.10).aspx */
static struct rdp_to_xkb_keyboard_layout rdp_keyboards[] = {
	{0x00000406, "dk"},
	{0x00000407, "de"},
	{0x00000409, "us"},
	{0x0000040c, "fr"},
	{0x00000410, "it"},
	{0x00000813, "be"},
	{0x00000000, 0},
};

/* taken from 2.2.7.1.6 Input Capability Set (TS_INPUT_CAPABILITYSET) */
static const char *rdp_keyboard_types[] = {
	"",	/* 0: unused */
	"", /* 1: IBM PC/XT or compatible (83-key) keyboard */
	"", /* 2: Olivetti "ICO" (102-key) keyboard */
	"", /* 3: IBM PC/AT (84-key) or similar keyboard */
	"pc102",/* 4: IBM enhanced (101- or 102-key) keyboard */
	"", /* 5: Nokia 1050 and similar keyboards */
	"",	/* 6: Nokia 9140 and similar keyboards */
	""	/* 7: Japanese keyboard */
};

BOOL QFreeRdpPeer::xf_peer_post_connect(freerdp_peer* client) {
#ifndef NO_XKB_SUPPORT
	struct xkb_rule_names xkbRuleNames;
#endif

	RdpPeerContext *ctx = (RdpPeerContext *)client->context;
	QFreeRdpPeer *rdpPeer = ctx->rdpPeer;
	rdpSettings *settings = client->settings;


#ifndef NO_XKB_SUPPORT
	memset(&xkbRuleNames, 0, sizeof(xkbRuleNames));
	xkbRuleNames.rules = "evdev";
	if(settings->KeyboardType <= 7)
		xkbRuleNames.model = rdp_keyboard_types[settings->KeyboardType];
	for(int i = 0; rdp_keyboards[i].xkbLayout; i++) {
		if(rdp_keyboards[i].rdpLayoutCode == settings->KeyboardLayout) {
			xkbRuleNames.layout = rdp_keyboards[i].xkbLayout;
			break;
		}
	}

	if(!xkbRuleNames.layout) {
		qWarning("don't have a rule to match keyboard layout 0x%x, keyboard will not work",
				settings->KeyboardLayout);
		return TRUE;
	}

	rdpPeer->mXkbContext = xkb_context_new((xkb_context_flags)0);
	if(!rdpPeer->mXkbContext) {
		qWarning("unable to create a xkb_context\n");
		return FALSE;
	}

	rdpPeer->mXkbKeymap = xkb_keymap_new_from_names(rdpPeer->mXkbContext, &xkbRuleNames,
			(xkb_keymap_compile_flags)0);
	if(rdpPeer->mXkbKeymap)
		rdpPeer->mXkbState = xkb_state_new(rdpPeer->mXkbKeymap);
#endif

	rdpPeer->mPlatform->getScreen()->setGeometry(0, 0, settings->DesktopWidth,
			settings->DesktopHeight);
	rdpPeer->mFlags |= PEER_ACTIVATED;

	// full refresh
	QRect refreshRect(0, 0, settings->DesktopWidth, settings->DesktopHeight);
	const QImage *src = rdpPeer->mPlatform->mScreen->getScreenBits();
	if(src)
		rdpPeer->repaint(QRegion(refreshRect), src);

	return TRUE;
}

bool QFreeRdpPeer::init() {
	int rcount = 0;
	void *rfds[32];
	int i, fd;
	rdpSettings	*settings;
	rdpInput *input;
	RdpPeerContext *peerCtx;

	mClient->context_size = sizeof(RdpPeerContext);
	mClient->ContextNew = (psPeerContextNew)rdp_peer_context_new;
	mClient->ContextFree = (psPeerContextFree)rdp_peer_context_free;
	freerdp_peer_context_new(mClient);

	peerCtx = (RdpPeerContext *)mClient->context;
	peerCtx->rdpPeer = this;

	settings = mClient->settings;
	settings->NlaSecurity = FALSE;
	mPlatform->configureClient(settings);

	mClient->Capabilities = QFreeRdpPeer::xf_peer_capabilities;
	mClient->PostConnect = QFreeRdpPeer::xf_peer_post_connect;
	//mClient->Activate = QFreeRdpPeer::xf_peer_activate;
	mClient->update->SuppressOutput = xf_suppress_output;

	input = mClient->input;
	input->SynchronizeEvent = QFreeRdpPeer::xf_input_synchronize_event;
	input->MouseEvent = QFreeRdpPeer::xf_mouseEvent;
	input->ExtendedMouseEvent = QFreeRdpPeer::xf_extendedMouseEvent;
	input->KeyboardEvent = QFreeRdpPeer::xf_input_keyboard_event;
	input->UnicodeKeyboardEvent = QFreeRdpPeer::xf_input_unicode_keyboard_event;

	mClient->Initialize(mClient);

	if (!mClient->GetFileDescriptor(mClient, rfds, &rcount)) {
		qDebug() << "unable to retrieve client fds\n";
		return false;
	}

	for(i = 0; i < rcount; i++) {
		fd = (int)(long)(rfds[i]);

		peerCtx->events[i] = new QSocketNotifier(fd, QSocketNotifier::Read);
		mPlatform->getDispatcher()->registerSocketNotifier(peerCtx->events[i]);

		connect(peerCtx->events[i], SIGNAL(activated(int)), this, SLOT(incomingBytes(int)) );
	}

	for( ; i < 32; i++) {
		peerCtx->events[i] = 0;
	}
	return true;
}

void QFreeRdpPeer::incomingBytes(int) {
	//qDebug() << "incomingBytes()";
	RdpPeerContext *peerCtx = (RdpPeerContext *)mClient->context;
	if(!mClient->CheckFileDescriptor(mClient)) {
		mBogusCheckFileDescriptor++;

		if(mBogusCheckFileDescriptor > 2) {
			qDebug() << "error checking file descriptor";
			mClient->Close(mClient);
			for(int i = 0; i < 32; i++) {
				if(peerCtx->events[i]) {
					disconnect(peerCtx->events[i], SIGNAL(activated(int)), this, SLOT(incomingBytes(int)) );
					mPlatform->getDispatcher()->unregisterSocketNotifier(peerCtx->events[i]);
				}
			}
			delete this;
			return;
		}
	}
}

void QFreeRdpPeer::repaint(const QRegion &region, const QImage *src) {
	if(!mFlags.testFlag(PEER_ACTIVATED))
		return;
	if(mFlags.testFlag(PEER_OUTPUT_DISABLED))
		return;

	repaint_raw(region, src);
}

void qimage_flipped_subrect(const QRect &rect, const QImage *img, BYTE *dest) {
	int stride = img->bytesPerLine();
	QPoint bottomLeft = rect.bottomLeft();
	int toCopy = rect.width() * 4;
	const uchar *src = img->bits() + (bottomLeft.y() * stride) + (bottomLeft.x() * 4);

	for(int h = 0; h < rect.height(); h++, src -= stride, dest += toCopy)
		memcpy(dest, src, toCopy);
}

#if 0
void qt_fillcolor(BYTE *dest, int count, UINT32 color) {
	UINT32 *d = (UINT32 *)dest;
	for(int i = 0; i < count; i++, d++)
		*d = color;

}
#endif

void QFreeRdpPeer::repaint_raw(const QRegion &region, const QImage *src) {
	rdpUpdate *update = mClient->update;
	SURFACE_BITS_COMMAND *cmd = &update->surface_bits_command;
	SURFACE_FRAME_MARKER *marker = &update->surface_frame_marker;

	cmd->bpp = 32;
	cmd->codecID = 0;
	marker->frameId++;
	marker->frameAction = SURFACECMD_FRAMEACTION_BEGIN;
	update->SurfaceFrameMarker(mClient->context, marker);

	foreach(QRect rect, region.rects()) {
		cmd->width = rect.width();
		cmd->destLeft = rect.left();
		cmd->destRight = rect.right();

		// we split the surface to fit in the negociated packet size
		// 16 is an approximation of bytes required for the surface command
		int heightIncrement = mClient->settings->MultifragMaxRequestSize / (16 + cmd->width * 4);
		int remainingHeight = rect.height();
		int top = rect.top();
		while(remainingHeight) {
			cmd->height = (remainingHeight > heightIncrement) ? heightIncrement : remainingHeight;
			cmd->destTop = top;
			cmd->destBottom = top + cmd->height;

			cmd->bitmapDataLength = cmd->width * cmd->height * 4;
			cmd->bitmapData = (BYTE *)realloc(cmd->bitmapData, cmd->bitmapDataLength);

			QRect subRect(QPoint(cmd->destLeft, cmd->destTop), QSize(cmd->width, cmd->height));
			qimage_flipped_subrect(subRect, src, cmd->bitmapData);
			update->SurfaceBits(mClient->context, cmd);

			remainingHeight -= cmd->height;
			top += cmd->height;
		}
	}
	marker->frameAction = SURFACECMD_FRAMEACTION_END;
	update->SurfaceFrameMarker(mClient->context, marker);
}



QSize QFreeRdpPeer::getGeometry() {
	return QSize(mClient->settings->DesktopWidth, mClient->settings->DesktopHeight);
}

