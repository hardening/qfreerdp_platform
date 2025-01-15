#include <freerdp/freerdp.h>
#include <freerdp/peer.h>
#include <freerdp/settings.h>
#include <freerdp/types.h>
#include <freerdp/version.h>

#include "qfreerdpplatform.h"
#include "qpa/qplatforminputcontext.h"
#include "qpa/qplatformintegration.h"
#include "winpr/input.h"
#include <QDebug>
#include <QString>
#include <QtGui/private/qguiapplication_p.h>

#ifndef NO_XKB_SUPPORT
#include <X11/keysym.h>
#endif

#include "qfreerdppeerkeyboard.h"
#include "qfreerdpwindow.h"

#include "kbdtrans.inc"

static void initCustomXkbLayout(rdpSettings *rdpSettings,
                                struct xkb_rule_names *xkbRuleNames) {
  // check if keyboard must be emulated like MS Windows keyboard
  if (freerdp_settings_get_uint32(rdpSettings, FreeRDP_OsMajorType) != OSMAJORTYPE_WINDOWS)
    return;

  qDebug("Using windows layout: %s", xkbRuleNames->layout);

  struct {
    const char *orig;
    const char *newOne;
  } customLayouts[] = {
    { "fr", RUBYCAT_DEFAULT_WINDOWS_FR_LAYOUT },
    { "latam", RUBYCAT_DEFAULT_WINDOWS_LATAM_LAYOUT },
    { "gb", RUBYCAT_DEFAULT_WINDOWS_GB_LAYOUT },
    { "be", RUBYCAT_DEFAULT_WINDOWS_BE_LAYOUT },
  };

  for (size_t i = 0; i < ARRAYSIZE(customLayouts); i++) {
    if (strcmp(xkbRuleNames->layout, customLayouts[i].orig) == 0) {
      xkbRuleNames->layout = customLayouts[i].newOne;
      // Using our custom rules files (xkb/rules/qfreerdp) to make the custom
      // type needed by our layouts accessible.
      xkbRuleNames->rules = "qfreerdp";
      break;
    }
  }
}

static void sendKeyEvent(QWindow *tlw, ulong timestamp, QEvent::Type type,
                         int key, Qt::KeyboardModifiers modifiers,
                         quint32 nativeScanCode, quint32 nativeVirtualKey,
                         quint32 nativeModifiers,
                         const QString &text = QString(), bool autorep = false,
                         ushort count = 1) {
  QPlatformInputContext *inputContext =
      QGuiApplicationPrivate::platformIntegration()->inputContext();
  bool filtered = false;

  if (inputContext) {
    QKeyEvent event(type, key, modifiers, nativeScanCode, nativeVirtualKey,
                    nativeModifiers, text, autorep, count);
    event.setTimestamp(timestamp);
    filtered = inputContext->filterEvent(&event);
  }

  if (!filtered) {
    QWindowSystemInterface::handleExtendedKeyEvent(
        tlw, timestamp, type, key, modifiers, nativeScanCode, nativeVirtualKey,
        nativeModifiers, text, autorep, count);
  }
}

/* Some Qt apps (qtwebengine-based ones in particular) have specialized native
 * key event handling code will only work correctly on a few well known Qt
 * platforms (eg. xcb, cocoa, windows, etc.). And when confronted with any
 * other platform, will fall back to guessing the keycode from the
 * `Qt::Key`[0].
 *
 * This will cause issues when connecting to qfreerdp with any keyboard layout
 * that isn't the expected US (qwerty), as `Qt::Key` values are layout
 * dependent.
 *
 * For (a lot) more detail, see nyanpasu64's excellent deep dive [1] on the
 * subject, and the related bug on qtwebengine's bugtracker [2].
 *
 * Here, in order to try working around this issue, we always generate
 * `Qt::Key` from the xkb keycode (representative of the physical layout),
 * instead of the xkb key symbol (keysym) (computed based on the current active
 * layout).
 *
 * [0] -
 * https://github.com/qt/qtwebengine/blob/6.4.2/src/core/web_event_factory.cpp#L1670
 * [1] -
 * https://forum.qt.io/topic/116544/how-is-qkeyevent-supposed-to-be-used-for-games/7?_=1734024377784&lang=en-US
 * [2] - https://bugreports.qt.io/browse/QTBUG-85660
 * */
static Qt::Key xkbKeycodeToUsLayoutQtKey(xkb_keycode_t keycode, bool isKeyPad) {
  if (keycode > 0xff)
    return Qt::Key_unknown;

  QtKeyboardLayoutLine code = XKB_KEYCODE_TO_QT_KEY[keycode & 0xff];
  return isKeyPad ? code.keypad : code.noMod;
}

static uint32_t xkbUnprintableKeysymToQtKey(xkb_keysym_t key) {
  for (uint32_t i = 0; XKB_KEYSYM_TO_QT_KEY[i]; i += 2)
    if (key == XKB_KEYSYM_TO_QT_KEY[i])
      return XKB_KEYSYM_TO_QT_KEY[i + 1];

  // We do not return Qt::Key_unknown here as the QKeyEvent constructor
  // explicitely expects 0 when the key isn't known:
  // https://doc.qt.io/qt-6/qkeyevent.html#QKeyEvent
  return 0;
}

static Qt::Key xkbKeysymToQtKey(xkb_keysym_t keysym,
                                Qt::KeyboardModifiers &modifiers,
                                const QString &text) {
  uint32_t code = 0;

  if (keysym >= XKB_KEY_F1 && keysym <= XKB_KEY_F35) {
    code = Qt::Key_F1 + (uint32_t(keysym) - XKB_KEY_F1);
  } else if (keysym >= XKB_KEY_KP_Space && keysym <= XKB_KEY_KP_9) {
    if (keysym >= XKB_KEY_KP_0) {
      // numeric keypad keys
      code = Qt::Key_0 + ((uint32_t)keysym - XKB_KEY_KP_0);
    } else {
      code = xkbUnprintableKeysymToQtKey(keysym);
    }
    modifiers |= Qt::KeypadModifier;
  } else if (text.length() == 1 && text.unicode()->unicode() > 0x1f &&
             text.unicode()->unicode() != 0x7f &&
             !(keysym >= XKB_KEY_dead_grave &&
               keysym <= XKB_KEY_dead_currency)) {
    code = text.unicode()->toUpper().unicode();
  } else {
    // any other keys
    code = xkbUnprintableKeysymToQtKey(keysym);
  }

  return static_cast<Qt::Key>(code);
}

static QString xkbKeysymToUnicode(xkb_keysym_t keysym) {
  QByteArray chars;
  int bytes;
  chars.resize(7);

  bytes = xkb_keysym_to_utf8(keysym, chars.data(), chars.size());
  if (bytes == -1)
    qWarning("QFreeRdp::keysymToUnicode - buffer too small");
  chars.resize(bytes - 1);

  return QString::fromUtf8(chars);
}

QFreeRdpPeerKeyboard::QFreeRdpPeerKeyboard(QFreeRdpPlatformConfig* platformConfig):
  mKeyCounter(0), mPlatformConfig(platformConfig), mXkbState(nullptr),
  mXkbKeymap(nullptr), mXkbContext(nullptr), mXkbComposeState(nullptr),
  mXkbComposeTable(nullptr), mXkbModIndices{} {}

QFreeRdpPeerKeyboard::~QFreeRdpPeerKeyboard() {
#ifndef NO_XKB_SUPPORT
  if (mXkbState) xkb_state_unref(mXkbState);
  if (mXkbContext) xkb_context_unref(mXkbContext);
  if (mXkbComposeTable) xkb_compose_table_unref(mXkbComposeTable);
  if (mXkbComposeState) xkb_compose_state_unref(mXkbComposeState);
  if (mXkbKeymap) xkb_keymap_unref(mXkbKeymap);
#endif
}

void QFreeRdpPeerKeyboard::handleRdpScancode(uint8_t scancode, uint16_t flags,
                                             uint32_t rdpKbdType,
                                             QFreeRdpWindow *focusWindow) {
  DWORD virtualScanCode = scancode;

  if (flags & KBD_FLAGS_EXTENDED)
    virtualScanCode |= KBDEXT;

  uint32_t vk_code =
      GetVirtualKeyCodeFromVirtualScanCode(virtualScanCode, rdpKbdType);

  // check XkbState
  if (mXkbState == NULL) {
    qWarning("Keyboard is not initialized. Received : %u", vk_code);
    return;
  }

  // check KBDEXT
  if (flags & KBD_FLAGS_EXTENDED)
    vk_code |= KBDEXT;

  // check if key is down or up
  bool isDown = !(flags & KBD_FLAGS_RELEASE);

#ifndef NO_XKB_SUPPORT
  // get scan code
#if FREERDP_VERSION_MAJOR == 3 && FREERDP_VERSION_MINOR >= 1 ||                \
    FREERDP_VERSION_MAJOR > 3
  xkb_keycode_t xkbKeycode =
      GetKeycodeFromVirtualKeyCode(vk_code, WINPR_KEYCODE_TYPE_XKB);
#else
  xkb_keycode_t xkbKeycode =
      GetKeycodeFromVirtualKeyCode(vk_code, KEYCODE_TYPE_EVDEV);
#endif // FREERDP_VERSION

  // get xkb symbol
  xkb_keysym_t xkbKeysym =
      getXkbSymbol(xkbKeycode, (isDown ? XKB_KEY_DOWN : XKB_KEY_UP));
  if (xkbKeysym == XKB_KEY_NoSymbol) {
    return;
  }
  QString text;
  xkb_compose_status status;
  if (mXkbComposeState && isDown) {
    xkb_compose_state_feed(mXkbComposeState, xkbKeysym);
    status = xkb_compose_state_get_status(mXkbComposeState);
  } else {
    status = XKB_COMPOSE_NOTHING;
  }
  switch (status) {
  case XKB_COMPOSE_NOTHING:
    // composition does not affect this event
    text = xkbKeysymToUnicode(xkbKeysym);
    break;
    ;
  case XKB_COMPOSE_CANCELLED: // sequence has no associated event
    xkb_compose_state_reset(mXkbComposeState);
    /* eat this event */
    return;
  case XKB_COMPOSE_COMPOSING: // waiting for next key
    /* eat this event */
    return;

  case XKB_COMPOSE_COMPOSED:
    char buf[32];
    xkb_compose_state_get_utf8(mXkbComposeState, buf, sizeof(buf));
    text = QString::fromUtf8(buf);
    qDebug() << "composed " << text;

    xkb_compose_state_reset(mXkbComposeState);
  }

  // check if a window has focus
  if (!focusWindow) {
    qWarning("%s: no windows has the focus", __func__);
    return;
  }

  // Get current modifiers state from xkb
  Qt::KeyboardModifiers qtModifiers = getQtModsForXkbCode(xkbKeycode);
  QEvent::Type eventType = isDown ? QEvent::KeyPress : QEvent::KeyRelease;

  // FIXME: What's the point here? oO
  int count = text.size();
  text.truncate(count);

  Qt::Key qtKey;
  if (mPlatformConfig->force_qwerty_events)
    qtKey =
        xkbKeycodeToUsLayoutQtKey(xkbKeycode, qtModifiers & Qt::KeypadModifier);
  else
    qtKey = xkbKeysymToQtKey(xkbKeysym, qtModifiers, text);

  // Uncomment for a keylogger :P
  // qDebug("%s: vkCode=0x%x xkb_keycode=0x%x flags=0x%x xsym=0x%x qtsym=0x%x "
  //        "modifiers=0x%x text=%s, isDown=%x",
  //        __func__, vk_code, xkbKeycode, flags, xkbKeysym, qtKey,
  //        (int)qtModifiers, text.toUtf8().constData(), isDown);

  // send key
  sendKeyEvent(focusWindow->window(), ++mKeyCounter, eventType, qtKey,
               qtModifiers, xkbKeycode, xkbKeysym, 0, text);

#endif
}

bool QFreeRdpPeerKeyboard::setKeymap(rdpSettings *rdpSettings) {
#ifndef NO_XKB_SUPPORT
  const char *locale_name = nullptr;
  struct xkb_rule_names xkbRuleNames = {
      .rules = "evdev",
      .model = NULL,
      .layout = NULL,
      .variant = NULL,
      .options = NULL,
  };

  mXkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!mXkbContext) {
    qWarning("unable to create a xkb_context\n");
    return false;
  }

  uint32_t rdpKbdType = freerdp_settings_get_uint32(rdpSettings, FreeRDP_KeyboardType);
  uint32_t rdpKbdLayout = freerdp_settings_get_uint32(rdpSettings, FreeRDP_KeyboardLayout);

  if (rdpKbdType <= 7 /* WINPR_KBD_TYPE_JAPANESE, max keyboard type */)
    xkbRuleNames.model = KBD_TYPE_RDP_TO_XKB[rdpKbdType];

  for (size_t i = 0; i < ARRAYSIZE(RDP_KBD_TO_XKB_LAYOUT_AND_LOCALE); i++) {
    if (RDP_KBD_TO_XKB_LAYOUT_AND_LOCALE[i].rdpLayoutCode == rdpKbdLayout) {
      xkbRuleNames.layout = RDP_KBD_TO_XKB_LAYOUT_AND_LOCALE[i].xkbLayout;
      locale_name = RDP_KBD_TO_XKB_LAYOUT_AND_LOCALE[i].locale_name;
      break;
    }
  }

  if (!xkbRuleNames.layout) {
    qWarning(
        "don't have a rule to match keyboard layout 0x%x, setting keyboard to "
        "default layout %s",
        rdpKbdLayout, DEFAULT_KBD_LANG);
    xkbRuleNames.layout = DEFAULT_KBD_LANG;
  } else {
    qWarning("settings->KeyboardLayout %s", xkbRuleNames.layout);
  }

  initCustomXkbLayout(rdpSettings, &xkbRuleNames);

  if (!xkbRuleNames.layout) {
    qWarning("don't have a rule to match keyboard layout 0x%x, keyboard will "
             "not work",
             rdpKbdLayout);
    return true;
  }

  mXkbKeymap = xkb_keymap_new_from_names(
      mXkbContext, &xkbRuleNames, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!mXkbKeymap) {
    qWarning("XKB keymap compilation failed. Keyboard will not work");
    return true;
  }

  // Get all the modifier indices now for ease of use later
  mXkbModIndices[MOD_SHIFT] =  xkb_keymap_mod_get_index(mXkbKeymap, XKB_MOD_NAME_SHIFT);
  mXkbModIndices[MOD_CAPS] =   xkb_keymap_mod_get_index(mXkbKeymap, XKB_MOD_NAME_CAPS);
  mXkbModIndices[MOD_CTRL] =   xkb_keymap_mod_get_index(mXkbKeymap, XKB_MOD_NAME_CTRL);
  mXkbModIndices[MOD_ALT] =    xkb_keymap_mod_get_index(mXkbKeymap, XKB_MOD_NAME_ALT);
  mXkbModIndices[MOD_NUM] =    xkb_keymap_mod_get_index(mXkbKeymap, XKB_MOD_NAME_NUM);
  mXkbModIndices[MOD_SUPER] =  xkb_keymap_mod_get_index(mXkbKeymap, "Mod4");
  // FIXME: ScrollLock is a LED name (XKB_LED_NAME_SCROLL), does this really work?
  mXkbModIndices[MOD_SCROLL] = xkb_keymap_mod_get_index(mXkbKeymap, "ScrollLock");

  mXkbState = xkb_state_new(mXkbKeymap);
  if (!mXkbState) {
    qWarning("XKB state creation failed. Keyboard will not work");
    return true;
  }

  if (!locale_name) {
    qWarning() << "missing locale, dead keys will not work";
    return true;
  }

  qDebug("using locale %s", locale_name);
  mXkbComposeTable = xkb_compose_table_new_from_locale(
      mXkbContext, locale_name, XKB_COMPOSE_COMPILE_NO_FLAGS);
  if (!mXkbComposeTable) {
    qWarning() << "failed to load compose table for locale " <<
        locale_name << ", dead keys will not work";
    return true;
  }

  mXkbComposeState = xkb_compose_state_new(
      mXkbComposeTable, XKB_COMPOSE_STATE_NO_FLAGS);
  if (!mXkbComposeState) {
    qWarning() << "failed to create compose state, dead keys will not work";
  }
#endif
  return true;
}

void QFreeRdpPeerKeyboard::updateModifiersState(bool capsLock, bool numLock,
                                                bool scrollLock,
                                                bool kanaLock) {
  Q_UNUSED(kanaLock);

  if (mXkbState == NULL)
    return;

  uint32_t mods_depressed, mods_latched, mods_locked, group;
  uint32_t numMask, capsMask, scrollMask;

  mods_depressed = xkb_state_serialize_mods(
      mXkbState, xkb_state_component(XKB_STATE_DEPRESSED));
  mods_latched = xkb_state_serialize_mods(
      mXkbState, xkb_state_component(XKB_STATE_LATCHED));
  mods_locked = xkb_state_serialize_mods(mXkbState,
                                         xkb_state_component(XKB_STATE_LOCKED));
  group = xkb_state_serialize_group(mXkbState,
                                    xkb_state_component(XKB_STATE_EFFECTIVE));

  numMask = (1 << mXkbModIndices[MOD_NUM]);
  capsMask = (1 << mXkbModIndices[MOD_CAPS]);
  scrollMask = (1 << mXkbModIndices[MOD_SCROLL]);

  mods_locked = capsLock ? (mods_locked | capsMask) : (mods_locked & ~capsMask);
  mods_locked = numLock ? (mods_locked | numMask) : (mods_locked & ~numMask);
  mods_locked =
      scrollLock ? (mods_locked | scrollMask) : (mods_locked & ~scrollMask);

  // FIXME: According to: https://xkbcommon.org/doc/current/group__state.html
  // xkb_state_update_mask shouldn't be used with xkb_state_update_key. Our
  // usecase here might be special enough to be warranted, but this needs
  // further study
  xkb_state_update_mask(mXkbState, mods_depressed, mods_latched, mods_locked, 0,
                        0, group);
}

// This function checks both for active XKB modifiers, but also consumed
// modifiers (already used as shortcuts/combinations inside XKB), as defined
// here:
// - https://xkbcommon.org/doc/current/group__state.html
// - https://github.com/xkbcommon/libxkbcommon/issues/310
//
// If we do not catch those consumed mods, Qt will see them as still active and
// mess with the use of Ctrl+Alt as AltGr from our windows compatibility
// keymaps.
//
// Also, we use XKB_CONSUMED_MODE_GTK in order to avoid ignoring modifiers
// in situations where they should remain available.
// -
// https://xkbcommon.org/doc/current/group__state.html#ga66c3ae7ebaf4ccd60e5dab61dc1c29fb
bool QFreeRdpPeerKeyboard::isXkbModSet(xkbModIndices mod,
                                       xkb_keycode_t keycode) {
  static const xkb_state_component cstate =
      xkb_state_component(XKB_STATE_DEPRESSED | XKB_STATE_LATCHED);
  xkb_mod_index_t modIndex = mXkbModIndices[mod];

  // We handle missing modifiers (return value: -1) as false: modifier
  // inactive/not consumed
  bool isModActive =
      !!xkb_state_mod_index_is_active(mXkbState, modIndex, cstate);
  bool isModConsumed = !!xkb_state_mod_index_is_consumed2(
      mXkbState, keycode, modIndex, XKB_CONSUMED_MODE_GTK);

  return isModActive && !isModConsumed;
}

// this part was originally adapted from QtWayland, the original code can be
// retrieved from the git repository https://qt.gitorious.org/qt/qtwayland,
// files are:
//	* src/plugins/platforms/wayland_common/qwaylandinputdevice.cpp
//	* src/plugins/platforms/wayland_common/qwaylandkey.cpp
Qt::KeyboardModifiers
QFreeRdpPeerKeyboard::getQtModsForXkbCode(xkb_keycode_t keycode) {
  Qt::KeyboardModifiers ret = Qt::NoModifier;

  if (isXkbModSet(MOD_SHIFT, keycode))
    ret |= Qt::ShiftModifier;
  if (isXkbModSet(MOD_CTRL, keycode))
    ret |= Qt::ControlModifier;
  if (isXkbModSet(MOD_ALT, keycode))
    ret |= Qt::AltModifier;
  if (isXkbModSet(MOD_NUM, keycode))
    ret |= Qt::KeypadModifier;
  if (isXkbModSet(MOD_SUPER, keycode))
    ret |= Qt::MetaModifier;

  return ret;
}

xkb_keysym_t QFreeRdpPeerKeyboard::getXkbSymbol(xkb_keycode_t keycode,
                                                bool isDown) {
  // get key symbol
  xkb_keysym_t keysym = xkb_state_key_get_one_sym(mXkbState, keycode);

  /* // Get key symbol name for debug logging
   * const uint32_t sizeSymbolName = 64;
   * char buffer[sizeSymbolName];
   * xkb_keysym_get_name(sym, buffer, sizeSymbolName);
   * qDebug("%s: sym=%d, scanCode=%d, keysym=%s", __func__, sym, scanCode,
   * buffer); */

  // Do not update state on key repeat, as libxkbcommon expects each
  // XKB_KEY_DOWN event to have a matching XKB_KEY_UP, and windows like to
  // send repeat key event when modifiers are held down. If not handled, this
  // results in stuck modifiers.
  // https://xkbcommon.org/doc/current/group__state.html#gac554aa20743a621692c1a744a05e06ce
  if (isDown == mXkbKeycodesDown.value(keycode))
    return keysym;

  // update xkb state
  xkb_state_update_key(mXkbState, keycode,
                       (isDown ? XKB_KEY_DOWN : XKB_KEY_UP));

  // update our own state keeping track of which physical keys are currently
  // held down
  mXkbKeycodesDown.insert(keycode, isDown);

  return keysym;
}
