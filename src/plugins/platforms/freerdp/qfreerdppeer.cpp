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
#include <winpr/user.h>
#include <winpr/sysinfo.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/input.h>
#include <freerdp/channels/cliprdr.h>

#include "qfreerdppeer.h"
#include "qfreerdpwindow.h"
#include "qfreerdpscreen.h"
#include "qfreerdpplatform.h"
#include "qfreerdpwindowmanager.h"
#include "qfreerdpclipboard.h"
#include "qfreerdppeerclipboard.h"

#include <QAbstractEventDispatcher>
#include <QSocketNotifier>
#include <QDebug>
#include <QMutexLocker>
#include <QStringList>
#include <QtGui/qpa/qwindowsysteminterface.h>
#include <qpa/qplatforminputcontext.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtCore/qmath.h>
#include <X11/keysym.h>
#include <ctype.h>

#include <freerdp/locale/keyboard.h>

#define DEFAULT_KBD_LANG "us"
#define RUBYCAT_DEFAULT_WINDOWS_FR_LAYOUT "rubycat_fr_windows"
#define RUBYCAT_DEFAULT_WINDOWS_GB_LAYOUT "rubycat_gb_windows"
#define RUBYCAT_DEFAULT_WINDOWS_LATAM_LAYOUT "rubycat_latam_windows"
#define RUBYCAT_DEFAULT_WINDOWS_BE_LAYOUT "rubycat_be_windows"

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
	auto codecs = codecs_new(client->context);
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
	codecs_free(client->context->codecs);
}

#ifndef NO_XKB_SUPPORT
#include <X11/keysym.h>
#include <xkbcommon/xkbcommon.h>


struct rdp_to_xkb_keyboard_layout {
	UINT32 rdpLayoutCode;
	const char *xkbLayout;
};

/* table reversed from
 	 https://github.com/awakecoding/FreeRDP/blob/master/libfreerdp/locale/xkb_layout_ids.c#L811 */
static
struct rdp_to_xkb_keyboard_layout rdp_keyboards[] = {

		{KBD_ARABIC_101, "ara"},
		{KBD_BULGARIAN, "bg"},
		{KBD_CHINESE_TRADITIONAL_US, 0},
		{KBD_CZECH, "cz"},
		{KBD_DANISH, "dk"},
		{KBD_GERMAN, "de"},
		{KBD_GREEK, "gr"},
		{KBD_US, "us"},
		{KBD_SPANISH, "es"},
		{KBD_FINNISH, "fi"},
		{KBD_FRENCH, "fr"},
		{KBD_HEBREW, "il"},
		{KBD_HUNGARIAN, "hu"},
		{KBD_ICELANDIC, "is"},
		{KBD_ITALIAN, "it"},
		{KBD_JAPANESE, "jp"},
		{KBD_KOREAN, "kr"},
		{KBD_DUTCH, "nl"},
		{KBD_NORWEGIAN, "no"},
		{KBD_POLISH_PROGRAMMERS, 0},
//		{KBD_PORTUGUESE_BRAZILIAN_ABN0416, 0},
		{KBD_ROMANIAN, 0},
		{KBD_RUSSIAN, "ru"},
		{KBD_CROATIAN, 0},
		{KBD_SLOVAK, 0},
		{KBD_ALBANIAN, 0},
		{KBD_SWEDISH, 0},
		{KBD_THAI_KEDMANEE, 0},
		{KBD_TURKISH_Q, 0},
		{KBD_URDU, 0},
		{KBD_UKRAINIAN, 0},
		{KBD_BELARUSIAN, 0},
		{KBD_SLOVENIAN, 0},
		{KBD_ESTONIAN, "ee"},
		{KBD_LATVIAN, 0},
		{KBD_LITHUANIAN_IBM, 0},
		{KBD_FARSI, 0},
		{KBD_VIETNAMESE, 0},
		{KBD_ARMENIAN_EASTERN, 0},
		{KBD_AZERI_LATIN, 0},
		{KBD_FYRO_MACEDONIAN, 0},
		{KBD_GEORGIAN, 0},
		{KBD_FAEROESE, 0},
		{KBD_DEVANAGARI_INSCRIPT, 0},
		{KBD_MALTESE_47_KEY, 0},
		{KBD_NORWEGIAN_WITH_SAMI, 0},
		{KBD_KAZAKH, 0},
		{KBD_KYRGYZ_CYRILLIC, 0},
		{KBD_TATAR, 0},
		{KBD_BENGALI, 0},
		{KBD_PUNJABI, 0},
		{KBD_GUJARATI, 0},
		{KBD_TAMIL, 0},
		{KBD_TELUGU, 0},
		{KBD_KANNADA, 0},
		{KBD_MALAYALAM, 0},
		{KBD_MARATHI, 0},
		{KBD_MONGOLIAN_CYRILLIC, 0},
		{KBD_UNITED_KINGDOM_EXTENDED, 0},
		{KBD_SYRIAC, 0},
		{KBD_NEPALI, 0},
		{KBD_PASHTO, 0},
		{KBD_DIVEHI_PHONETIC, 0},
		{KBD_LUXEMBOURGISH, 0},
		{KBD_MAORI, 0},
		{KBD_CHINESE_SIMPLIFIED_US, 0},
		{KBD_SWISS_GERMAN, 0},
		{KBD_UNITED_KINGDOM, "gb"},
		{KBD_LATIN_AMERICAN, "latam"},
		{KBD_BELGIAN_FRENCH, "be"},
		{KBD_BELGIAN_PERIOD, "be"},
		{KBD_PORTUGUESE, 0},
		{KBD_SERBIAN_LATIN, 0},
		{KBD_AZERI_CYRILLIC, 0},
		{KBD_SWEDISH_WITH_SAMI, 0},
		{KBD_UZBEK_CYRILLIC, 0},
		{KBD_INUKTITUT_LATIN, 0},
		{KBD_CANADIAN_FRENCH_LEGACY, "fr-legacy"},
		{KBD_SERBIAN_CYRILLIC, 0},
		{KBD_CANADIAN_FRENCH, 0},
		{KBD_SWISS_FRENCH, "ch"},
		{KBD_BOSNIAN, "unicode"},
		{KBD_IRISH, 0},
		{KBD_BOSNIAN_CYRILLIC, 0},

		{0x0000000, DEFAULT_KBD_LANG} // default
};

static void sendKey(QWindow *tlw, ulong timestamp, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers, quint32 nativeScanCode, 
			quint32 nativeVirtualKey, quint32 nativeModifiers, const QString& text = QString(), bool autorep = false, ushort count = 1) {
	QPlatformInputContext *inputContext = QGuiApplicationPrivate::platformIntegration()->inputContext();
	bool filtered = false;

	if (inputContext) {
		QKeyEvent event(type, key, modifiers, nativeScanCode, nativeVirtualKey, nativeModifiers, text, autorep, count);
		event.setTimestamp(timestamp);
		filtered = inputContext->filterEvent(&event);
	}
	
	if (!filtered) {
		QWindowSystemInterface::handleExtendedKeyEvent(tlw, timestamp, type, key, modifiers, nativeScanCode, nativeVirtualKey, nativeModifiers,
					text, autorep, count);
	}
}

static QStringList MODIFIERS = QStringList() << "Shift" << "Control" << "Alt" << "ISO_Level3" << "Super" << "Mod1" << "Mod4";

//
// these part was adapted from QtWayland, the original code can be retrieved
// from the git repository https://qt.gitorious.org/qt/qtwayland, files are:
//	* src/plugins/platforms/wayland_common/qwaylandinputdevice.cpp
//	* src/plugins/platforms/wayland_common/qwaylandkey.cpp
//
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

static QString keysymToUnicode(xkb_keysym_t sym) {
	QByteArray chars;
	int bytes;
	chars.resize(7);

    bytes = xkb_keysym_to_utf8(sym, chars.data(), chars.size());
    if (bytes == -1)
        qWarning("QFreeRdp::keysymToUnicode - buffer too small");
    chars.resize(bytes-1);

    return QString::fromUtf8(chars);
}


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

	XK_dead_grave,				Qt::Key_Dead_Grave,
	XK_dead_acute,				Qt::Key_Dead_Acute,
	XK_dead_circumflex,			Qt::Key_Dead_Circumflex,
	XK_dead_tilde,				Qt::Key_Dead_Tilde,
	XK_dead_macron,				Qt::Key_Dead_Macron,
	XK_dead_breve,				Qt::Key_Dead_Breve,
	XK_dead_abovedot, 			Qt::Key_Dead_Abovedot,
	XK_dead_diaeresis,			Qt::Key_Dead_Diaeresis,
	XK_dead_abovering,			Qt::Key_Dead_Abovering,
	XK_dead_doubleacute,		Qt::Key_Dead_Doubleacute,
	XK_dead_caron,				Qt::Key_Dead_Caron,
	XK_dead_cedilla,			Qt::Key_Dead_Cedilla,
	XK_dead_ogonek,				Qt::Key_Dead_Ogonek,
	XK_dead_iota,				Qt::Key_Dead_Iota,
	XK_dead_voiced_sound,		Qt::Key_Dead_Voiced_Sound,
	XK_dead_semivoiced_sound,	Qt::Key_Dead_Semivoiced_Sound,
	XK_dead_belowdot,			Qt::Key_Dead_Belowdot,
	XK_dead_hook,				Qt::Key_Dead_Hook,
	XK_dead_horn,				Qt::Key_Dead_Horn,
    0,                          0
};

static int keysymToQtKey(xkb_keysym_t key)
{
    int code = 0;
    int i = 0;
    while (KeyTbl[i]) {
        if (key == KeyTbl[i]) {
            code = (int)KeyTbl[i+1];
            break;
        }
        i += 2;
    }

    return code;
}

static int keysymToQtKey(xkb_keysym_t keysym, Qt::KeyboardModifiers &modifiers, const QString &text)
{
    int code = 0;

    if (keysym >= XKB_KEY_F1 && keysym <= XKB_KEY_F35) {
        code =  Qt::Key_F1 + (int(keysym) - XKB_KEY_F1);
    } else if (keysym >= XKB_KEY_KP_Space && keysym <= XKB_KEY_KP_9) {
        if (keysym >= XKB_KEY_KP_0) {
            // numeric keypad keys
            code = Qt::Key_0 + ((int)keysym - XKB_KEY_KP_0);
        } else {
            code = keysymToQtKey(keysym);
        }
        modifiers |= Qt::KeypadModifier;
    } else if (text.length() == 1 && text.unicode()->unicode() > 0x1f
        && text.unicode()->unicode() != 0x7f
        && !(keysym >= XKB_KEY_dead_grave && keysym <= XKB_KEY_dead_currency)) {
        code = text.unicode()->toUpper().unicode();
    } else {
        // any other keys
        code = keysymToQtKey(keysym);
    }

    return code;
}

void initCustomKeyboard(freerdp_peer* client, struct xkb_rule_names *xkbRuleNames) {
	rdpSettings *settings = client->context->settings;

	// check if keyboard must be emulated like MS Windows keyboard
	if (settings->OsMajorType != OSMAJORTYPE_WINDOWS) {
		return;
	}
	qDebug("Using windows layout: %s", xkbRuleNames->layout);
	if (xkbRuleNames->layout == QString("fr")) {
		xkbRuleNames->layout = RUBYCAT_DEFAULT_WINDOWS_FR_LAYOUT;
	} else if (xkbRuleNames->layout == QString("latam")) {
		xkbRuleNames->layout = RUBYCAT_DEFAULT_WINDOWS_LATAM_LAYOUT;
	} else if (xkbRuleNames->layout == QString("gb")) {
		xkbRuleNames->layout = RUBYCAT_DEFAULT_WINDOWS_GB_LAYOUT;
	} else if (xkbRuleNames->layout == QString("be")) {
		xkbRuleNames->layout = RUBYCAT_DEFAULT_WINDOWS_BE_LAYOUT;
	}
}

#endif



QFreeRdpPeer::QFreeRdpPeer(QFreeRdpPlatform *platform, freerdp_peer* client) :
		mPlatform(platform),
		mClient(client),
		mBogusCheckFileDescriptor(0),
		mLastButtons(Qt::NoButton),
		mKeyTime(0),
		mSurfaceOutputModeEnabled(false),
		mNsCodecSupported(false),
		mCompositor(platform->getScreen()),
		mVcm(nullptr),
		mClipboard(nullptr)
#ifndef NO_XKB_SUPPORT
		, mXkbContext(0)
		, mXkbKeymap(0)
		, mXkbState(0)
		, mCapsLockModIndex(0)
		, mNumLockModIndex(0)
		, mScrollLockModIndex(0)
#endif
{
	// init Kbd modifiers
	foreach(QString modifier, MODIFIERS) {
		mKbdStateModifiers.insert(modifier, false);
	}
}

void QFreeRdpPeer::dropSocketNotifier(QSocketNotifier *notifier) {
	QAbstractEventDispatcher *dispatcher = mPlatform->getDispatcher();
	if(notifier) {
		notifier->setEnabled(false);
		disconnect(notifier, SIGNAL(activated(int)), this, SLOT(incomingBytes(int)) );
		dispatcher->unregisterSocketNotifier(notifier);
		delete notifier;
	}
}

QFreeRdpPeer::~QFreeRdpPeer() {
	RdpPeerContext *peerCtx = (RdpPeerContext *)mClient->context;

	dropSocketNotifier(peerCtx->event);
	dropSocketNotifier(peerCtx->channelEvent);

	if (mClipboard)
		delete mClipboard;

	if (mVcm)
		WTSCloseServer(mVcm);
	mVcm = NULL;

	mClient->Close(mClient);

	freerdp_peer_context_free(mClient);
	freerdp_peer_free(mClient);
	mPlatform->unregisterPeer(this);
}

BOOL QFreeRdpPeer::xf_mouseEvent(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y) {
	RdpPeerContext *peerContext = (RdpPeerContext *)input->context;
	QFreeRdpPeer *peer = peerContext->rdpPeer;
	QFreeRdpWindowManager *windowManager = peer->mPlatform->mWindowManager;
	QWindow *window = windowManager->getWindowAt(QPoint(x, y));

	if(!window)
		return TRUE;

	int wheelDelta;
	//qDebug("%s: dest=%d flags=0x%x buttons=0x%x", __func__, window->winId(), flags, peer->mLastButtons);
	Qt::KeyboardModifiers modifiers = Qt::NoModifier;

	QPoint wTopLeft = window->geometry().topLeft();
	QPoint localCoord(x - wTopLeft.x(), y - wTopLeft.y());
	QPoint pos(x, y);

	if (flags & PTR_FLAGS_WHEEL) {
		pos = peer->mLastMousePos;
		localCoord = pos - wTopLeft;
		wheelDelta = flags & 0xff;
		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			wheelDelta = -wheelDelta;

		QPoint angleDelta;
		angleDelta.setY(wheelDelta);
		QWindowSystemInterface::handleWheelEvent(window, localCoord, pos,
				QPoint(), angleDelta, modifiers);
	} else {
		if (peer->mLastMousePos != pos) {
			// first move the mouse
			QWindowSystemInterface::handleMouseEvent(window, localCoord, pos, peer->mLastButtons, Qt::MouseButton::NoButton,
					QEvent::Type::MouseMove, modifiers);
			peer->mLastMousePos = pos;
		}
		// then click each button if necessary
		for (int i = 0; i < 3; i++) {
			int button;
			Qt::MouseButton qtButton;
			switch (i) {
				case 0:
					button = PTR_FLAGS_BUTTON1;
					qtButton = Qt::MouseButton::LeftButton;
					break;
				case 1:
					button = PTR_FLAGS_BUTTON2;
					qtButton = Qt::MouseButton::RightButton;
					break;
				case 2:
					button = PTR_FLAGS_BUTTON3;
					qtButton = Qt::MouseButton::MiddleButton;
					break;
				default: 
					qFatal("unreachable: fourth button clicked");
			}
			if (flags & button) {
				QEvent::Type eventtype;
				if (flags & PTR_FLAGS_DOWN) {
					peer->mLastButtons |= qtButton;
					eventtype = QEvent::MouseButtonPress;
				} else {
					peer->mLastButtons &= ~qtButton;
					eventtype = QEvent::MouseButtonRelease;
				}
				QWindowSystemInterface::handleMouseEvent(window,
						localCoord,
						pos,
						peer->mLastButtons,
						qtButton,
						eventtype,
						modifiers
				);
				
			}
		}		
	}

	if(peer->mLastButtons)
		windowManager->setActiveWindow((QFreeRdpWindow *)window->handle());

	return TRUE;
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

	/* sends a full refresh */
	// TODO: drop the full refresh ?
	QRect refreshRect(0, 0, client->context->settings->DesktopWidth, client->context->settings->DesktopHeight);

	rdpPeer->repaint(QRegion(refreshRect));
	rdpPeer->updateModifiersState(flags & KBD_SYNC_CAPS_LOCK, flags & KBD_SYNC_NUM_LOCK,
			flags & KBD_SYNC_SCROLL_LOCK,
			flags & KBD_SYNC_KANA_LOCK);

	return TRUE;
}


BOOL QFreeRdpPeer::xf_input_keyboard_event(rdpInput* input, UINT16 flags, UINT8 code)
{
	RdpPeerContext *peerCtx = (RdpPeerContext *)input->context;
	QFreeRdpPeer *rdpPeer = peerCtx->rdpPeer;
	rdpSettings *settings = rdpPeer->mClient->context->settings;
	DWORD virtualScanCode = code;

	if(flags & KBD_FLAGS_EXTENDED)
		virtualScanCode |= KBDEXT;

	uint32_t vk_code = GetVirtualKeyCodeFromVirtualScanCode(virtualScanCode, settings->KeyboardSubType);
	rdpPeer->handleVirtualKeycode(flags, vk_code);
	return TRUE;
}

BOOL QFreeRdpPeer::xf_input_unicode_keyboard_event(rdpInput* /*input*/, UINT16 /*flags*/, UINT16 /*code*/)
{
	//qDebug("Client sent a unicode keyboard event (flags:0x%X code:0x%X)\n", flags, code);
	return TRUE;
}

BOOL QFreeRdpPeer::xf_refresh_rect(rdpContext *context, BYTE count, const RECTANGLE_16* areas) {
	qDebug("QFreeRdpPeer::%s()", __func__);

	// get RDP peer
	RdpPeerContext *peerCtx = (RdpPeerContext *)context;
	QFreeRdpPeer *rdpPeer = peerCtx->rdpPeer;

	// repaint rect areas
	for(int i = 0; i < count; i++) {
		RECTANGLE_16 rect = areas[i];
		QRect refreshRect(rect.left, rect.top, rect.right, rect.bottom);
		rdpPeer->repaint(QRegion(refreshRect));
	}

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


/* taken from 2.2.7.1.6 Input Capability Set (TS_INPUT_CAPABILITYSET) */
static const char *rdp_keyboard_types[] = {
	"",	/* 0: unused */
	"", /* 1: IBM PC/XT or compatible (83-key) keyboard */
	"", /* 2: Olivetti "ICO" (102-key) keyboard */
	"", /* 3: IBM PC/AT (84-key) or similar keyboard */
	"pc102",/* 4: IBM enhanced (101- or 102-key) keyboard */
	"", /* 5: Nokia 1050 and similar keyboards */
	"",	/* 6: Nokia 9140 and similar keyboards */
	"jp106"	/* 7: Japanese keyboard */
};

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

		// // If RDP client is a windows client -> Optimize mode
		// if (settings->OsMajorType == OSMAJORTYPE_WINDOWS) {
		// 	return configureOptimizeMode(settings);
		// }
		
		// else -> legacy mode
		return configureDisplayLegacyMode(settings);
	}

	return FALSE;
}

BOOL QFreeRdpPeer::xf_peer_post_connect(freerdp_peer* client) {
#ifndef NO_XKB_SUPPORT
	struct xkb_rule_names xkbRuleNames;
#endif

	RdpPeerContext *ctx = (RdpPeerContext *)client->context;
	QFreeRdpPeer *rdpPeer = ctx->rdpPeer;
	rdpSettings *settings = client->context->settings;

	if (!rdpPeer->detectDisplaySettings(client)) {
		// can't detect display settings
		qWarning("Can not detect correctly settings");
		return FALSE;
	}
	

#ifndef NO_XKB_SUPPORT
	memset(&xkbRuleNames, 0, sizeof(xkbRuleNames));
	xkbRuleNames.rules = "evdev";
	if(settings->KeyboardType <= 7)
		xkbRuleNames.model = rdp_keyboard_types[settings->KeyboardType];
	for(int i = 0; rdp_keyboards[i].rdpLayoutCode; i++) {
		if(rdp_keyboards[i].rdpLayoutCode == settings->KeyboardLayout) {
			xkbRuleNames.layout = rdp_keyboards[i].xkbLayout;
			break;
		}
	}

	if(!xkbRuleNames.layout) {
		qWarning("don't have a rule to match keyboard layout 0x%x, set keyboard to default layout %s",
				settings->KeyboardLayout, DEFAULT_KBD_LANG);
		xkbRuleNames.layout = DEFAULT_KBD_LANG;
	} else {
		qWarning("settings->KeyboardLayout %s",
				xkbRuleNames.layout);
	}

	initCustomKeyboard(client, &xkbRuleNames);

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

	rdpPeer->mXkbKeymap = xkb_map_new_from_names(rdpPeer->mXkbContext, &xkbRuleNames,
			(xkb_map_compile_flags)0);
	if(rdpPeer->mXkbKeymap) {
		rdpPeer->mXkbState = xkb_state_new(rdpPeer->mXkbKeymap);

		rdpPeer->mCapsLockModIndex = xkb_map_mod_get_index(rdpPeer->mXkbKeymap, XKB_MOD_NAME_CAPS);
		rdpPeer->mNumLockModIndex = xkb_map_mod_get_index(rdpPeer->mXkbKeymap, "Mod2");
		rdpPeer->mScrollLockModIndex = xkb_map_mod_get_index(rdpPeer->mXkbKeymap, "ScrollLock");
	}
#endif

	// display initialization
	rdpPeer->init_display(client);

	if (WTSVirtualChannelManagerIsChannelJoined(rdpPeer->mVcm, CLIPRDR_SVC_CHANNEL_NAME))
	{
		qDebug() << "instanciating clipboard component";

		rdpPeer->mClipboard = new QFreerdpPeerClipboard(rdpPeer, rdpPeer->mVcm);
		if (!rdpPeer->mClipboard->start()) {
			qDebug() << "error starting clipboard";
			return FALSE;
		}
		rdpPeer->mPlatform->mClipboard->registerPeer(rdpPeer->mClipboard);
	}

	return TRUE;
}

void QFreeRdpPeer::init_display(freerdp_peer* client) {
	// get context
	RdpPeerContext *ctx = (RdpPeerContext *)client->context;
	QFreeRdpPeer *rdpPeer = ctx->rdpPeer;
	rdpSettings* settings = client->context->settings;

	// set screen geometry 
	QFreeRdpScreen *screen = rdpPeer->mPlatform->getScreen();
	//TODO: see the user's monitor layout
	QRect currentGeometry = screen->geometry();
	QRect peerGeometry(0, 0, settings->DesktopWidth, settings->DesktopHeight);
	if(currentGeometry != peerGeometry)
		screen->setGeometry(peerGeometry);
	rdpPeer->mFlags |= PEER_ACTIVATED;

	mCompositor.reset(settings->DesktopWidth, settings->DesktopHeight);

	// default : show mouse
	POINTER_SYSTEM_UPDATE pointer_system;
	pointer_system.type = SYSPTR_DEFAULT;
	client->context->update->pointer->PointerSystem(client->context, &pointer_system);

	// full refresh
	const QImage *src = screen->getScreenBits();
	if(src) {
		rdpPeer->repaintWithCompositor(QRegion(peerGeometry));
	}
}

BOOL QFreeRdpPeer::xf_peer_activate(freerdp_peer * /*client*/) {
	return TRUE;
}

bool QFreeRdpPeer::init() {
	mClient->ContextSize = sizeof(RdpPeerContext);
	mClient->ContextNew = (psPeerContextNew)rdp_peer_context_new;
	mClient->ContextFree = (psPeerContextFree)rdp_peer_context_free;
	freerdp_peer_context_new(mClient);

	RdpPeerContext *peerCtx = (RdpPeerContext *)mClient->context;
	peerCtx->rdpPeer = this;

	rdpSettings	*settings;
	settings = mClient->context->settings;
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

	mClient->Initialize(mClient);

	mVcm = WTSOpenServerA((LPSTR)mClient->context);
	if (!mVcm) {
		qDebug() << "unable to retrieve client VCM\n";
		return false;
	}

	peerCtx->event = new QSocketNotifier(mClient->sockfd, QSocketNotifier::Read);
	connect(peerCtx->event, SIGNAL(activated(int)), this, SLOT(incomingBytes(int)) );

	HANDLE vcmHandle = WTSVirtualChannelManagerGetEventHandle(mVcm);
	if (vcmHandle)
	{
		peerCtx->channelEvent = new QSocketNotifier(GetEventFileDescriptor(vcmHandle), QSocketNotifier::Read);
		connect(peerCtx->channelEvent, SIGNAL(activated(int)), this, SLOT(channelTraffic(int)) );
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

void QFreeRdpPeer::channelTraffic(int) {
	if (!WTSVirtualChannelManagerCheckFileDescriptor(mVcm)) {
		qDebug() << "error treating channels";
		deleteLater();
	}
}

void QFreeRdpPeer::repaintWithCompositor(const QRegion &region) {
	if(!mFlags.testFlag(PEER_ACTIVATED))
		return;
	if(mFlags.testFlag(PEER_OUTPUT_DISABLED))
		return;

	QRegion dirty = mCompositor.qtToRdpDirtyRegion(region);
	repaint_raw(dirty);
}

void QFreeRdpPeer::repaint(const QRegion &region) {
	if(!mFlags.testFlag(PEER_ACTIVATED))
		return;
	if(mFlags.testFlag(PEER_OUTPUT_DISABLED))
		return;

	repaint_raw(region);
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

#if 0
void qt_fillcolor(BYTE *dest, int count, UINT32 color) {
	UINT32 *d = (UINT32 *)dest;
	for(int i = 0; i < count; i++, d++)
		*d = color;

}
#endif

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


void QFreeRdpPeer::updateModifiersState(bool capsLock, bool numLock, bool scrollLock, bool kanaLock) {
#ifndef NO_XKB_SUPPORT
	Q_UNUSED(kanaLock);

	if (mXkbState == NULL)
		return;

	uint32_t mods_depressed, mods_latched, mods_locked, group;
	int numMask, capsMask, scrollMask;

	mods_depressed = xkb_state_serialize_mods(mXkbState, xkb_state_component(XKB_STATE_DEPRESSED));
	mods_latched = xkb_state_serialize_mods(mXkbState, xkb_state_component(XKB_STATE_LATCHED));
	mods_locked = xkb_state_serialize_mods(mXkbState, xkb_state_component(XKB_STATE_LOCKED));
	group = xkb_state_serialize_group(mXkbState, xkb_state_component(XKB_STATE_EFFECTIVE));

	numMask = (1 << mNumLockModIndex);
	capsMask = (1 << mCapsLockModIndex);
	scrollMask = (1 << mScrollLockModIndex);

	mods_locked = capsLock ? (mods_locked | capsMask) : (mods_locked & ~capsMask);
	mods_locked = numLock ? (mods_locked | numMask) : (mods_locked & !numLock);
	mods_locked = scrollLock ? (mods_locked | scrollMask) : (mods_locked & ~scrollMask);

	xkb_state_update_mask(mXkbState, mods_depressed, mods_latched, mods_locked, 0, 0, group);
#else
	Q_UNUSED(capsLock);
	Q_UNUSED(numLock);
	Q_UNUSED(scrollLock);
	Q_UNUSED(kanaLock);
#endif
}


void QFreeRdpPeer::handleVirtualKeycode(quint32 flags, quint32 vk_code) {
	// check XkbState
	if (mXkbState == NULL) {
		qWarning("Keyboard is not initialized. Received : %u", vk_code);
		return;
	}

	// check KBDEXT
	if(flags & KBD_FLAGS_EXTENDED)
		vk_code |= KBDEXT;

	// get scan code
	quint32 scancode = GetKeycodeFromVirtualKeyCode(vk_code, KEYCODE_TYPE_EVDEV);

	// check if key is down or up
	bool isDown = !(flags & KBD_FLAGS_RELEASE);

#ifndef NO_XKB_SUPPORT
	// get xkb symbol
	xkb_keysym_t xsym = getXkbSymbol(scancode, (isDown ? XKB_KEY_DOWN : XKB_KEY_UP));
	if (xsym == XKB_KEY_NoSymbol) {
		return;
	}

	//qWarning("%s: vkCode=0x%x scanCode=0x%x isDown=%x", __func__, vk_code, scancode, isDown);

	// check if windows has focus
	QFreeRdpWindow *focusWindow = mPlatform->mWindowManager->getActiveWindow();
	if(!focusWindow) {
		qWarning("%s: no windows has the focus", __func__);
		return;
	}

	// update Qt keyboard state
	Qt::KeyboardModifiers modifiers = translateModifiers(mXkbState);
	QEvent::Type type = isDown ? QEvent::KeyPress : QEvent::KeyRelease;

	QString text = keysymToUnicode(xsym);
	int count = text.size();
    text.truncate(count);

	uint32_t qtsym = keysymToQtKey(xsym, modifiers, text);

	//qWarning("%s: xsym=0x%x qtsym=0x%x modifiers=0x%x text=%s, isDown=%x", __func__, xsym, qtsym, (int)modifiers, text.toUtf8().constData(), isDown);

	// send key
	sendKey(focusWindow->window(), ++mKeyTime, type, qtsym, modifiers, scancode, xsym, 0,text);

#endif

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

bool QFreeRdpPeer::setPointer(const POINTER_LARGE_UPDATE *largePointer, Qt::CursorShape newShape)
{
	bool isNew;
	UINT16 cacheIndex = getCursorCacheIndex(newShape, isNew);
	rdpPointerUpdate* pointer = mClient->context->update->pointer;
	if (isNew) {
		POINTER_LARGE_UPDATE lpointer = *largePointer;
		lpointer.cacheIndex = cacheIndex;
		return pointer->PointerLarge(mClient->context, &lpointer);
	}

	POINTER_CACHED_UPDATE update;
	update.cacheIndex = cacheIndex;
	return pointer->PointerCached(mClient->context, &update);
}

UINT16 QFreeRdpPeer::getCursorCacheIndex(Qt::CursorShape shape, bool &isNew)
{
	auto it = mCursorCache.find(shape);
	if (it != mCursorCache.end()) {
		it->lastUse = GetTickCount64();
		isNew = false;
		return it->cacheIndex;
	}

	isNew = true;
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
	const QImage *src = mPlatform->getScreen()->getScreenBits();

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

	const QImage *src = mPlatform->getScreen()->getScreenBits();
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

xkb_keysym_t QFreeRdpPeer::getXkbSymbol(const quint32 &scanCode, const bool &isDown) {

	// get key symbol
	xkb_keysym_t sym = xkb_state_key_get_one_sym(mXkbState, scanCode);

	// check key symbol name
	const uint32_t sizeSymbolName = 64;
	char buffer[sizeSymbolName];
	int xkbStatus = xkb_keysym_get_name(sym, buffer, sizeSymbolName);
	if (xkbStatus == -1) {
		return sym;
	}

	// check if symbol is a known modifier
	foreach(QString modifierName, mKbdStateModifiers.keys()) {
		if (!QString(buffer).startsWith(modifierName)) {
			// not this modifier
			continue;
		}

		if (isDown && mKbdStateModifiers[modifierName]) {
			// key is pressed and key was already enabled -> return sym
			return sym;
		}

		// update state
		mKbdStateModifiers[modifierName] = isDown;
		break;
	}

	// update xkb state
	xkb_state_update_key(mXkbState, scanCode, (isDown ? XKB_KEY_DOWN : XKB_KEY_UP));

	// return symbol
	return sym;
}

