#pragma once

#include <freerdp/settings.h>
#include <QHash>
#include <QObject>

#ifndef NO_XKB_SUPPORT
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon.h>
#endif

QT_BEGIN_NAMESPACE

class QFreeRdpWindow;
class QFreeRdpPlatformConfig;

class QFreeRdpPeerKeyboard : public QObject {
  Q_OBJECT

public:
  QFreeRdpPeerKeyboard(QFreeRdpPlatformConfig* platformConfig);
  ~QFreeRdpPeerKeyboard();

  void handleRdpScancode(uint8_t scancode, uint16_t flags, uint32_t rdpKbdType, QFreeRdpWindow *focusWindow);
  bool setKeymap(rdpSettings *rdpSettings);
  void updateModifiersState(bool capsLock, bool numLock, bool ScrollLock,
                            bool kanaLock);

private:
  enum xkbModIndices {
    MOD_SHIFT,
    MOD_CAPS,
    MOD_CTRL,
    MOD_ALT,
    MOD_NUM,
    MOD_SUPER,  // aka. Meta
    MOD_SCROLL, // ScrollLock
    MAX_MOD,
  };

#ifndef NO_XKB_SUPPORT
  bool isXkbModSet(xkbModIndices mod, xkb_keycode_t keycode);
  Qt::KeyboardModifiers getQtModsForXkbCode(xkb_keysym_t keycode);
  xkb_keysym_t getXkbSymbol(xkb_keycode_t keycode, bool isDown);
#endif

private:
  ulong mKeyCounter;
  QFreeRdpPlatformConfig *mPlatformConfig;

#ifndef NO_XKB_SUPPORT
  // std::unique_ptr<struct xkb_state, decltype(&xkb_state_unref)> mXkbState;
  struct xkb_state *mXkbState;
  struct xkb_keymap *mXkbKeymap;
  struct xkb_context *mXkbContext;
  struct xkb_compose_state *mXkbComposeState;
  struct xkb_compose_table *mXkbComposeTable;

  xkb_mod_index_t mXkbModIndices[MAX_MOD];

  QHash<xkb_keycode_t, bool> mXkbKeycodesDown;
#endif
};

QT_END_NAMESPACE
