#pragma once

#include <freerdp/locale/keyboard.h>
#include <Qt>
#include <X11/keysym.h>

#define DEFAULT_KBD_LANG "us"
#define RUBYCAT_DEFAULT_WINDOWS_FR_LAYOUT "rubycat_fr_windows"
#define RUBYCAT_DEFAULT_WINDOWS_GB_LAYOUT "rubycat_gb_windows"
#define RUBYCAT_DEFAULT_WINDOWS_LATAM_LAYOUT "rubycat_latam_windows"
#define RUBYCAT_DEFAULT_WINDOWS_BE_LAYOUT "rubycat_be_windows"


/* taken from 2.2.7.1.6 Input Capability Set (TS_INPUT_CAPABILITYSET) */
static const char *KBD_TYPE_RDP_TO_XKB[] = {
	"",	/* 0: unused */
	"", /* 1: IBM PC/XT or compatible (83-key) keyboard */
	"", /* 2: Olivetti "ICO" (102-key) keyboard */
	"", /* 3: IBM PC/AT (84-key) or similar keyboard */
	"pc102",/* 4: IBM enhanced (101- or 102-key) keyboard */
	"", /* 5: Nokia 1050 and similar keyboards */
	"",	/* 6: Nokia 9140 and similar keyboards */
	"jp106"	/* 7: Japanese keyboard */
};


struct rdp_to_xkb_keyboard_layout {
	UINT32 rdpLayoutCode;
	const char *xkbLayout;
	const char *locale_name;
};

/* table reversed from
 	 https://github.com/awakecoding/FreeRDP/blob/master/libfreerdp/locale/xkb_layout_ids.c#L811 */
static
struct rdp_to_xkb_keyboard_layout RDP_KBD_TO_XKB_LAYOUT_AND_LOCALE[] = {
		{KBD_ARABIC_101,                    "ara",             "ar_MA.UTF-8"},
		{KBD_BULGARIAN,                     "bg",              "bg_BG.UTF-8"},
		{KBD_CHINESE_TRADITIONAL_US,        nullptr,           nullptr},
		{KBD_CZECH,                         "cz",              nullptr},
		{KBD_DANISH,                        "dk",              "da_DK.UTF-8"},
		{KBD_GERMAN,                        "de",              "de_DE.UTF-8"},
		{KBD_GREEK,                         "gr",              nullptr},
		{KBD_US,                            "us",              "en_US.UTF-8"},
		{KBD_SPANISH,                       "es",              "es_ES.UTF-8"},
		{KBD_FINNISH,                       "fi",              "fi_FI.UTF-8"},
		{KBD_FRENCH,                        "fr",              "fr_FR.UTF-8"},
		{KBD_HEBREW,                        "il",              nullptr},
		{KBD_HUNGARIAN,                     "hu",              "hu_HU.UTF-8"},
		{KBD_ICELANDIC,                     "is",              nullptr},
		{KBD_ITALIAN,                       "it",              "it_IT.UTF-8"},
		{KBD_JAPANESE,                      "jp",              nullptr},
		{KBD_KOREAN,                        "kr",              nullptr},
		{KBD_DUTCH,                         "nl",              "nl_NL.UTF-8"},
		{KBD_NORWEGIAN,                     "no",              "no_NO.UTF-8"},
		{KBD_POLISH_PROGRAMMERS,            nullptr,           nullptr},
//		{KBD_PORTUGUESE_BRAZILIAN_ABN0416,  nullptr,           nullptr},
		{KBD_ROMANIAN,                      nullptr,           nullptr},
		{KBD_RUSSIAN,                       "ru",              "ru_RU.UTF-8"},
		{KBD_CROATIAN,                      nullptr,           nullptr},
		{KBD_SLOVAK,                        nullptr,           nullptr},
		{KBD_ALBANIAN,                      nullptr,           nullptr},
		{KBD_SWEDISH,                       nullptr,           nullptr},
		{KBD_THAI_KEDMANEE,                 nullptr,           nullptr},
		{KBD_TURKISH_Q,                     nullptr,           nullptr},
		{KBD_URDU,                          nullptr,           nullptr},
		{KBD_UKRAINIAN,                     nullptr,           nullptr},
		{KBD_BELARUSIAN,                    nullptr,           nullptr},
		{KBD_SLOVENIAN,                     nullptr,           nullptr},
		{KBD_ESTONIAN,                      "ee",              nullptr},
		{KBD_LATVIAN,                       nullptr,           nullptr},
		{KBD_LITHUANIAN_IBM,                nullptr,           nullptr},
		{KBD_FARSI,                         nullptr,           nullptr},
		{KBD_VIETNAMESE,                    nullptr,           nullptr},
		{KBD_ARMENIAN_EASTERN,              nullptr,           nullptr},
		{KBD_AZERI_LATIN,                   nullptr,           nullptr},
		{KBD_FYRO_MACEDONIAN,               nullptr,           nullptr},
		{KBD_GEORGIAN,                      nullptr,           nullptr},
		{KBD_FAEROESE,                      nullptr,           nullptr},
		{KBD_DEVANAGARI_INSCRIPT,           nullptr,           nullptr},
		{KBD_MALTESE_47_KEY,                nullptr,           nullptr},
		{KBD_NORWEGIAN_WITH_SAMI,           nullptr,           nullptr},
		{KBD_KAZAKH,                        nullptr,           nullptr},
		{KBD_KYRGYZ_CYRILLIC,               nullptr,           nullptr},
		{KBD_TATAR,                         nullptr,           nullptr},
		{KBD_BENGALI,                       nullptr,           nullptr},
		{KBD_PUNJABI,                       nullptr,           nullptr},
		{KBD_GUJARATI,                      nullptr,           nullptr},
		{KBD_TAMIL,                         nullptr,           nullptr},
		{KBD_TELUGU,                        nullptr,           nullptr},
		{KBD_KANNADA,                       nullptr,           nullptr},
		{KBD_MALAYALAM,                     nullptr,           nullptr},
		{KBD_MARATHI,                       nullptr,           nullptr},
		{KBD_MONGOLIAN_CYRILLIC,            nullptr,           nullptr},
		{KBD_UNITED_KINGDOM_EXTENDED,       nullptr,           nullptr},
		{KBD_SYRIAC,                        nullptr,           nullptr},
		{KBD_NEPALI,                        nullptr,           nullptr},
		{KBD_PASHTO,                        nullptr,           nullptr},
		{KBD_DIVEHI_PHONETIC,               nullptr,           nullptr},
		{KBD_LUXEMBOURGISH,                 nullptr,           nullptr},
		{KBD_MAORI,                         nullptr,           nullptr},
		{KBD_CHINESE_SIMPLIFIED_US,         nullptr,           nullptr},
		{KBD_SWISS_GERMAN,                  nullptr,           nullptr},
		{KBD_UNITED_KINGDOM,                "gb",              "en_GB.UTF-8"},
		{KBD_LATIN_AMERICAN,                "latam",           "es_MX.UTF-8"},
		{KBD_BELGIAN_FRENCH,                "be",              "fr_BE.UTF-8"},
		{KBD_BELGIAN_PERIOD,                "be",              "fr_BE.UTF-8"},
		{KBD_PORTUGUESE,                    nullptr,           nullptr},
		{KBD_SERBIAN_LATIN,                 nullptr,           nullptr},
		{KBD_AZERI_CYRILLIC,                nullptr,           nullptr},
		{KBD_SWEDISH_WITH_SAMI,             nullptr,           nullptr},
		{KBD_UZBEK_CYRILLIC,                nullptr,           nullptr},
		{KBD_INUKTITUT_LATIN,               nullptr,           nullptr},
		{KBD_CANADIAN_FRENCH_LEGACY,        "fr-legacy",       "fr_CA.UTF-8"},
		{KBD_SERBIAN_CYRILLIC,              nullptr,           nullptr},
		{KBD_CANADIAN_FRENCH,               nullptr,           nullptr},
		{KBD_SWISS_FRENCH,                  "ch",              "fr_CH.UTF-8"},
		{KBD_BOSNIAN,                       "unicode",         nullptr},
		{KBD_IRISH,                         nullptr,           nullptr},
		{KBD_BOSNIAN_CYRILLIC,              nullptr,           nullptr},
};


static const uint32_t XKB_KEYSYM_TO_QT_KEY[] = {
	XK_Escape,                Qt::Key_Escape,
	XK_Tab,                   Qt::Key_Tab,
	XK_ISO_Left_Tab,          Qt::Key_Backtab,
	XK_BackSpace,             Qt::Key_Backspace,
	XK_Return,                Qt::Key_Return,
	XK_Insert,                Qt::Key_Insert,
	XK_Delete,                Qt::Key_Delete,
	XK_Clear,                 Qt::Key_Delete,
	XK_Pause,                 Qt::Key_Pause,
	XK_Print,                 Qt::Key_Print,

	XK_Home,                  Qt::Key_Home,
	XK_End,                   Qt::Key_End,
	XK_Left,                  Qt::Key_Left,
	XK_Up,                    Qt::Key_Up,
	XK_Right,                 Qt::Key_Right,
	XK_Down,                  Qt::Key_Down,
	XK_Prior,                 Qt::Key_PageUp,
	XK_Next,                  Qt::Key_PageDown,

	XK_Shift_L,               Qt::Key_Shift,
	XK_Shift_R,               Qt::Key_Shift,
	XK_Shift_Lock,            Qt::Key_Shift,
	XK_Control_L,             Qt::Key_Control,
	XK_Control_R,             Qt::Key_Control,
	XK_Meta_L,                Qt::Key_Meta,
	XK_Meta_R,                Qt::Key_Meta,
	XK_Alt_L,                 Qt::Key_Alt,
	XK_Alt_R,                 Qt::Key_Alt,
	XK_Caps_Lock,             Qt::Key_CapsLock,
	XK_Num_Lock,              Qt::Key_NumLock,
	XK_Scroll_Lock,           Qt::Key_ScrollLock,
	XK_Super_L,               Qt::Key_Super_L,
	XK_Super_R,               Qt::Key_Super_R,
	XK_Menu,                  Qt::Key_Menu,
	XK_Hyper_L,               Qt::Key_Hyper_L,
	XK_Hyper_R,               Qt::Key_Hyper_R,
	XK_Help,                  Qt::Key_Help,

	XK_KP_Space,              Qt::Key_Space,
	XK_KP_Tab,                Qt::Key_Tab,
	XK_KP_Enter,              Qt::Key_Enter,
	XK_KP_Home,               Qt::Key_Home,
	XK_KP_Left,               Qt::Key_Left,
	XK_KP_Up,                 Qt::Key_Up,
	XK_KP_Right,              Qt::Key_Right,
	XK_KP_Down,               Qt::Key_Down,
	XK_KP_Prior,              Qt::Key_PageUp,
	XK_KP_Next,               Qt::Key_PageDown,
	XK_KP_End,                Qt::Key_End,
	XK_KP_Begin,              Qt::Key_Clear,
	XK_KP_Insert,             Qt::Key_Insert,
	XK_KP_Delete,             Qt::Key_Delete,
	XK_KP_Equal,              Qt::Key_Equal,
	XK_KP_Multiply,           Qt::Key_Asterisk,
	XK_KP_Add,                Qt::Key_Plus,
	XK_KP_Separator,          Qt::Key_Comma,
	XK_KP_Subtract,           Qt::Key_Minus,
	XK_KP_Decimal,            Qt::Key_Period,
	XK_KP_Divide,             Qt::Key_Slash,

	XK_ISO_Level3_Shift,      Qt::Key_AltGr,
	XK_Multi_key,             Qt::Key_Multi_key,
	XK_Codeinput,             Qt::Key_Codeinput,
	XK_SingleCandidate,       Qt::Key_SingleCandidate,
	XK_MultipleCandidate,     Qt::Key_MultipleCandidate,
	XK_PreviousCandidate,     Qt::Key_PreviousCandidate,

	XK_Mode_switch,           Qt::Key_Mode_switch,
	XK_script_switch,         Qt::Key_Mode_switch,

	XK_dead_grave,            Qt::Key_Dead_Grave,
	XK_dead_acute,            Qt::Key_Dead_Acute,
	XK_dead_circumflex,       Qt::Key_Dead_Circumflex,
	XK_dead_tilde,            Qt::Key_Dead_Tilde,
	XK_dead_macron,           Qt::Key_Dead_Macron,
	XK_dead_breve,            Qt::Key_Dead_Breve,
	XK_dead_abovedot,         Qt::Key_Dead_Abovedot,
	XK_dead_diaeresis,        Qt::Key_Dead_Diaeresis,
	XK_dead_abovering,        Qt::Key_Dead_Abovering,
	XK_dead_doubleacute,      Qt::Key_Dead_Doubleacute,
	XK_dead_caron,            Qt::Key_Dead_Caron,
	XK_dead_cedilla,          Qt::Key_Dead_Cedilla,
	XK_dead_ogonek,           Qt::Key_Dead_Ogonek,
	XK_dead_iota,             Qt::Key_Dead_Iota,
	XK_dead_voiced_sound,     Qt::Key_Dead_Voiced_Sound,
	XK_dead_semivoiced_sound, Qt::Key_Dead_Semivoiced_Sound,
	XK_dead_belowdot,         Qt::Key_Dead_Belowdot,
	XK_dead_hook,             Qt::Key_Dead_Hook,
	XK_dead_horn,             Qt::Key_Dead_Horn,
	0,                        0
};


struct QtKeyboardLayoutLine {
	Qt::Key noMod;
	Qt::Key keypad;
};

/* This array was crafted from freerdp's KEYCODE_TO_VKCODE_XKB, manually
 * testing the correspondence between XKB keycodes and Qt::Keys using a
 * standard ISO keyboard, configured to the `us` layout, on a wayland host
 * (libxkbcommon).
 *
 * More obscure keycodes were searched for in /usr/share/X11/xkb/
 * */
static QtKeyboardLayoutLine XKB_KEYCODE_TO_QT_KEY[256] = {
	{Qt::Key_unknown,       Qt::Key_unknown},       /* 0 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* 1 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* 2 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* 3 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* 4 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* 5 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* 6 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* 7 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* 8 */
	{Qt::Key_Escape,        Qt::Key_Escape},        /* <ESC> 9 */
	{Qt::Key_1,             Qt::Key_1},             /* <AE01> 10 */
	{Qt::Key_2,             Qt::Key_2},             /* <AE02> 11 */
	{Qt::Key_3,             Qt::Key_3},             /* <AE03> 12 */
	{Qt::Key_4,             Qt::Key_4},             /* <AE04> 13 */
	{Qt::Key_5,             Qt::Key_5},             /* <AE05> 14 */
	{Qt::Key_6,             Qt::Key_6},             /* <AE06> 15 */
	{Qt::Key_7,             Qt::Key_7},             /* <AE07> 16 */
	{Qt::Key_8,             Qt::Key_8},             /* <AE08> 17 */
	{Qt::Key_9,             Qt::Key_9},             /* <AE09> 18 */
	{Qt::Key_0,             Qt::Key_0},             /* <AE10> 19 */
	{Qt::Key_Minus,         Qt::Key_Minus},         /* <AE11> 20 */
	{Qt::Key_Equal,         Qt::Key_Equal},         /* <AE12> 21 */
	{Qt::Key_Backspace,     Qt::Key_Backspace},     /* <BKSP> 22 */
	{Qt::Key_Tab,           Qt::Key_Tab},           /* <TAB> 23 */
	{Qt::Key_Q,             Qt::Key_Q},             /* <AD01> 24 */
	{Qt::Key_W,             Qt::Key_W},             /* <AD02> 25 */
	{Qt::Key_E,             Qt::Key_E},             /* <AD03> 26 */
	{Qt::Key_R,             Qt::Key_R},             /* <AD04> 27 */
	{Qt::Key_T,             Qt::Key_T},             /* <AD05> 28 */
	{Qt::Key_Y,             Qt::Key_Y},             /* <AD06> 29 */
	{Qt::Key_U,             Qt::Key_U},             /* <AD07> 30 */
	{Qt::Key_I,             Qt::Key_I},             /* <AD08> 31 */
	{Qt::Key_O,             Qt::Key_O},             /* <AD09> 32 */
	{Qt::Key_P,             Qt::Key_P},             /* <AD10> 33 */
	{Qt::Key_BracketLeft,   Qt::Key_BracketLeft},   /* <AD11> 34 */
	{Qt::Key_BraceRight,    Qt::Key_BraceRight},    /* <AD12> 35 */
	{Qt::Key_Return,        Qt::Key_Return},        /* <RTRN> 36 */
	{Qt::Key_Control,       Qt::Key_Control},       // | Qt::ControlModifier   /* <LCTL> 37 */
	{Qt::Key_A,             Qt::Key_A},             /* <AC01> 38 */
	{Qt::Key_S,             Qt::Key_S},             /* <AC02> 39 */
	{Qt::Key_D,             Qt::Key_D},             /* <AC03> 40 */
	{Qt::Key_F,             Qt::Key_F},             /* <AC04> 41 */
	{Qt::Key_G,             Qt::Key_G},             /* <AC05> 42 */
	{Qt::Key_H,             Qt::Key_H},             /* <AC06> 43 */
	{Qt::Key_J,             Qt::Key_J},             /* <AC07> 44 */
	{Qt::Key_K,             Qt::Key_K},             /* <AC08> 45 */
	{Qt::Key_L,             Qt::Key_L},             /* <AC09> 46 */
	{Qt::Key_Semicolon,     Qt::Key_Semicolon},     /* <AC10> 47 */
	{Qt::Key_Apostrophe,    Qt::Key_Apostrophe},    /* <AC11> 48 */
	{Qt::Key_QuoteLeft,     Qt::Key_QuoteLeft},     /* <TLDE> 49 */
	{Qt::Key_Shift,         Qt::Key_Shift},         // | Qt::ShiftModifier     /* <LFSH> 50; Qt doesn't distinguish left and right shift here */
	{Qt::Key_Backslash,     Qt::Key_Backslash},     /* <BKSL> <AC12> 51 */
	{Qt::Key_Z,             Qt::Key_Z},             /* <AB01> 52 */
	{Qt::Key_X,             Qt::Key_X},             /* <AB02> 53 */
	{Qt::Key_C,             Qt::Key_C},             /* <AB03> 54 */
	{Qt::Key_V,             Qt::Key_V},             /* <AB04> 55 */
	{Qt::Key_B,             Qt::Key_B},             /* <AB05> 56 */
	{Qt::Key_N,             Qt::Key_N},             /* <AB06> 57 */
	{Qt::Key_M,             Qt::Key_M},             /* <AB07> 58 */
	{Qt::Key_Comma,         Qt::Key_Comma},         /* <AB08> 59 */
	{Qt::Key_Period,        Qt::Key_Period},        /* <AB09> 60 */
	{Qt::Key_Slash,         Qt::Key_Slash},         /* <AB10> 61 */
	{Qt::Key_Shift,         Qt::Key_Shift},         // | Qt::ShiftModifier     /* <RTSH> 62; Qt doesn't distinguish left and right shift here */
	{Qt::Key_Asterisk,      Qt::Key_Asterisk},      // | Qt::KeypadModifier   /* <KPMU> 63 */
	{Qt::Key_Alt,           Qt::Key_Alt},           // | Qt::AltModifier      /* <LALT> 64 */
	{Qt::Key_Space,         Qt::Key_Space},         /* <SPCE> 65 */
	{Qt::Key_CapsLock,      Qt::Key_CapsLock},      /* <CAPS> 66 */
	{Qt::Key_F1,            Qt::Key_F1},            /* <FK01> 67 */
	{Qt::Key_F2,            Qt::Key_F2},            /* <FK02> 68 */
	{Qt::Key_F3,            Qt::Key_F3},            /* <FK03> 69 */
	{Qt::Key_F4,            Qt::Key_F4},            /* <FK04> 70 */
	{Qt::Key_F5,            Qt::Key_F5},            /* <FK05> 71 */
	{Qt::Key_F6,            Qt::Key_F6},            /* <FK06> 72 */
	{Qt::Key_F7,            Qt::Key_F7},            /* <FK07> 73 */
	{Qt::Key_F8,            Qt::Key_F8},            /* <FK08> 74 */
	{Qt::Key_F9,            Qt::Key_F9},            /* <FK09> 75 */
	{Qt::Key_F10,           Qt::Key_F10},           /* <FK10> 76 */
	{Qt::Key_NumLock,       Qt::Key_NumLock},       /* <NMLK> 77 */
	{Qt::Key_ScrollLock,    Qt::Key_ScrollLock},    /* <SCLK> 78 */

	/* Keypad */
	{Qt::Key_Home,          Qt::Key_7},             /* <KP7> 79 */
	{Qt::Key_Up,            Qt::Key_8},             /* <KP8> 80 */
	{Qt::Key_PageUp,        Qt::Key_9},             /* <KP9> 81 */
	{Qt::Key_Minus,         Qt::Key_Minus},         /* <KPSU> 82 */
	{Qt::Key_Left,          Qt::Key_4},             /* <KP4> 83 */
	{Qt::Key_Clear,         Qt::Key_5},             /* <KP5> 84 */
	{Qt::Key_Right,         Qt::Key_6},             /* <KP6> 85 */
	{Qt::Key_Plus,          Qt::Key_Plus},          /* <KPAD> 86 */
	{Qt::Key_End,           Qt::Key_1},             /* <KP1> 87 */
	{Qt::Key_Down,          Qt::Key_2},             /* <KP2> 88 */
	{Qt::Key_PageDown,      Qt::Key_3},             /* <KP3> 89 */
	{Qt::Key_Insert,        Qt::Key_0},             /* <KP0> 90 */
	{Qt::Key_Delete,        Qt::Key_Period},        /* <KPDL> 91 */

	{Qt::Key_Alt,           Qt::Key_Alt},           /* <LVL3> 92; this keycode might be sent by some AltGr? */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* 93 */

	// FIXME: Qt actually sends Qt::Key_Less when pressing this physical key
	// using a `us` layout. But Qt would also generate the same Qt::Key_Less
	// when pressing Shift+Comma (<AB08>), and so qtwebengine (6.4.2) wouldn't
	// disambiguate[0] and always convert back to VK_OEM_COMMA, and not
	// VK_OEM_102, which is the correct code to emit in order to end up with an
	// IntlBackslash DOM code [1]. In order to generate a "more correct" (but
	// still wrong) keycode, we send Qt::Key_Backslash, which results in
	// VK_OEM_4[2], meaning <BKSL> or <AC12> in XKB parlance.
	//
	// [0] - https://github.com/qt/qtwebengine/blob/6.4.2/src/core/web_event_factory.cpp#L526
	// [1] - https://source.chromium.org/chromium/chromium/src/+/main:ui/events/keycodes/dom_us_layout_data.h;l=528;drc=dd73812bb7bca5b8fb5105127573bcfd4f226a27
	// [2] - https://github.com/qt/qtwebengine/blob/6.4.2/src/core/web_event_factory.cpp#L547
	// {Qt::Key_Less,          Qt::Key_Less},          /* <LSGT> 94 */
	{Qt::Key_Backslash,     Qt::Key_Backslash},     /* <LSGT> 94 */

	{Qt::Key_F11,           Qt::Key_F11},           /* <FK11> 95 */
	{Qt::Key_F12,           Qt::Key_F12},           /* <FK12> 96 */
	{Qt::Key_Backslash,     Qt::Key_Backslash},     /* <AB11> 97; FIXME: I haven't been able to actually confirm this one */
	{Qt::Key_unknown,       Qt::Key_unknown},       // VK_DBE_KATAKANA,      /* <KATA> 98 */
	{Qt::Key_unknown,       Qt::Key_unknown},       // VK_DBE_HIRAGANA,      /* <HIRA> 99 */
	{Qt::Key_unknown,       Qt::Key_unknown},       // VK_CONVERT,           /* <HENK> 100 */
	{Qt::Key_unknown,       Qt::Key_unknown},       // VK_HKTG,              /* <HKTG> 101 */
	{Qt::Key_unknown,       Qt::Key_unknown},       // VK_NONCONVERT,        /* <MUHE> 102 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <JPCM> 103 */
	{Qt::Key_Return,        Qt::Key_Return},        /* <KPEN> 104 */
	{Qt::Key_Control,       Qt::Key_Control},       // | Qt::ControlModifier /* <RCTL> 105 */
	{Qt::Key_Slash,         Qt::Key_Slash},         // | Qt::KeypadModifier  /* <KPDV> 106 */
	{Qt::Key_Print,         Qt::Key_Print},         /* <PRSC> 107 */

	{Qt::Key_AltGr,         Qt::Key_AltGr},         // | Qt::AltModifier  /* <RALT> <ALGR> 108 */

	{Qt::Key_unknown,       Qt::Key_unknown},       /* <LNFD> KEY_LINEFEED 109 */
	{Qt::Key_Home,          Qt::Key_Home},          /* <HOME> 110 */
	{Qt::Key_Up,            Qt::Key_Up},            /* <UP> 111 */
	{Qt::Key_PageUp,        Qt::Key_PageUp},        /* <PGUP> 112 */
	{Qt::Key_Left,          Qt::Key_Left},          /* <LEFT> 113 */
	{Qt::Key_Right,         Qt::Key_Right},         /* <RGHT> 114 */
	{Qt::Key_End,           Qt::Key_End},           /* <END> 115 */
	{Qt::Key_Down,          Qt::Key_Down},          /* <DOWN> 116 */
	{Qt::Key_PageDown,      Qt::Key_PageDown},      /* <PGDN> 117 */
	{Qt::Key_Insert,        Qt::Key_Insert},        /* <INS> 118 */
	{Qt::Key_Delete,        Qt::Key_Delete},        /* <DELE> 119 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I120> KEY_MACRO 120 */
	{Qt::Key_VolumeMute,    Qt::Key_VolumeMute},    /* <MUTE> 121 */
	{Qt::Key_VolumeMute,    Qt::Key_VolumeMute},    /* <VOL-> 122 */
	{Qt::Key_VolumeMute,    Qt::Key_VolumeMute},    /* <VOL+> 123 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <POWR> 124 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <KPEQ> 125 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I126> KEY_KPPLUSMINUS 126 */
	{Qt::Key_Pause,         Qt::Key_Pause},         /* <PAUS> 127 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I128> KEY_SCALE 128 */
	{Qt::Key_Period,        Qt::Key_Period},        /* <I129> <KPPT> KEY_KPCOMMA 129; looks like the normal keypad period but without delete */
	{Qt::Key_Hangul,        Qt::Key_Hangul},        /* <HNGL> 130 */
	{Qt::Key_Hangul_Hanja,  Qt::Key_Hangul_Hanja},  /* <HJCV> 131 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <AE13> 132; FIXME: no idea */
	{Qt::Key_Meta,          Qt::Key_Meta},          // | Qt::MetaModifier             /* <LWIN> <LMTA> 133 */
	{Qt::Key_Meta,          Qt::Key_Meta},          // | Qt::MetaModifier             /* <RWIN> <RMTA> 134 */
	{Qt::Key_Menu,          Qt::Key_Menu},          /* <COMP> <MENU> 135 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <STOP> 136 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <AGAI> 137 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <PROP> 138 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <UNDO> 139 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <FRNT> 140 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <COPY> 141 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <OPEN> 142 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <PAST> 143 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <FIND> 144 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <CUT> 145 */
	{Qt::Key_Help,          Qt::Key_Help},          /* <HELP> 146 */
	{Qt::Key_Menu,          Qt::Key_Menu},          /* <I147> KEY_MENU 147 */
	{Qt::Key_Calculator,    Qt::Key_Calculator},    /* <I148> KEY_CALC 148 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I149> KEY_SETUP 149 */
	{Qt::Key_Sleep,         Qt::Key_Sleep},         /* <I150> KEY_SLEEP 150 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I151> KEY_WAKEUP 151 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I152> KEY_FILE 152 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I153> KEY_SEND 153 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I154> KEY_DELETEFILE 154 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I155> KEY_XFER 155 */
	{Qt::Key_Launch0,       Qt::Key_Launch0},       /* <I156> KEY_PROG1 156; FIXME: actual Key_Launch number unknown */
	{Qt::Key_Launch1,       Qt::Key_Launch1},       /* <I157> KEY_PROG2 157; FIXME: actual Key_Launch number unknown */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I158> KEY_WWW 158 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I159> KEY_MSDOS 159 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I160> KEY_COFFEE 160 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I161> KEY_DIRECTION 161 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I162> KEY_CYCLEWINDOWS 162 */
	{Qt::Key_LaunchMail,    Qt::Key_LaunchMail},    /* <I163> KEY_MAIL 163 */
	{Qt::Key_Favorites,     Qt::Key_Favorites},     /* <I164> KEY_BOOKMARKS 164 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I165> KEY_COMPUTER 165 */
	{Qt::Key_Back,          Qt::Key_Back},          /* <I166> KEY_BACK 166 */
	{Qt::Key_Forward,       Qt::Key_Forward},       /* <I167> KEY_FORWARD 167 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I168> KEY_CLOSECD 168 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I169> KEY_EJECTCD 169 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I170> KEY_EJECTCLOSECD 170 */
	{Qt::Key_MediaNext,     Qt::Key_MediaNext},     /* <I171> KEY_NEXTSONG 171 */
	{Qt::Key_MediaPlay,     Qt::Key_MediaPlay},     /* <I172> KEY_PLAYPAUSE 172 */
	{Qt::Key_MediaPrevious, Qt::Key_MediaPrevious}, /* <I173> KEY_PREVIOUSSONG 173 */
	{Qt::Key_MediaStop,     Qt::Key_MediaStop},     /* <I174> KEY_STOPCD 174 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I175> KEY_RECORD 175 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I176> KEY_REWIND 176 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I177> KEY_PHONE 177 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I178> KEY_ISO 178 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I179> KEY_CONFIG 179 */
	{Qt::Key_HomePage,      Qt::Key_HomePage},      /* <I180> KEY_HOMEPAGE 180 */
	{Qt::Key_Refresh,       Qt::Key_Refresh},       /* <I181> KEY_REFRESH 181 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I182> KEY_EXIT 182 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I183> KEY_MOVE 183 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I184> KEY_EDIT 184 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I185> KEY_SCROLLUP 185 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I186> KEY_SCROLLDOWN 186 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I187> KEY_KPLEFTPAREN 187 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I188> KEY_KPRIGHTPAREN 188 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I189> KEY_NEW 189 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I190> KEY_REDO 190 */
	{Qt::Key_F13,           Qt::Key_F13},           /* <FK13> 191 */
	{Qt::Key_F14,           Qt::Key_F14},           /* <FK14> 192 */
	{Qt::Key_F15,           Qt::Key_F15},           /* <FK15> 193 */
	{Qt::Key_F16,           Qt::Key_F16},           /* <FK16> 194 */
	{Qt::Key_F17,           Qt::Key_F17},           /* <FK17> 195 */
	{Qt::Key_F18,           Qt::Key_F18},           /* <FK18> 196 */
	{Qt::Key_F19,           Qt::Key_F19},           /* <FK19> 197 */
	{Qt::Key_F20,           Qt::Key_F20},           /* <FK20> 198 */
	{Qt::Key_F21,           Qt::Key_F21},           /* <FK21> 199 */
	{Qt::Key_F22,           Qt::Key_F22},           /* <FK22> 200 */
	{Qt::Key_F23,           Qt::Key_F23},           /* <FK23> 201 */
	{Qt::Key_F24,           Qt::Key_F24},           /* <FK24> 202 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <MDSW> 203 */
	{Qt::Key_Alt,           Qt::Key_Alt},           /* <ALT> 204 */
	{Qt::Key_Meta,          Qt::Key_Meta},          /* <META> 205 */
	{Qt::Key_Super_L,       Qt::Key_Super_L},       /* <SUPR> 206; not sure when this one happens instead of <LWIN> */
	{Qt::Key_Hyper_L,       Qt::Key_Hyper_L},       /* <HYPR> 207 */
	{Qt::Key_MediaPlay,     Qt::Key_MediaPlay},     /* <I208> KEY_PLAYCD 208 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I209> KEY_PAUSECD 209 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I210> KEY_PROG3 210 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I211> KEY_PROG4 211 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I212> KEY_DASHBOARD 212 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I213> KEY_SUSPEND 213 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I214> KEY_CLOSE 214 */
	{Qt::Key_MediaPlay,     Qt::Key_MediaPlay},     /* <I215> KEY_PLAY 215 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I216> KEY_FASTFORWARD 216 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I217> KEY_BASSBOOST 217 */
	{Qt::Key_Print,         Qt::Key_Print},         /* <I218> KEY_PRINT 218 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I219> KEY_HP 219 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I220> KEY_CAMERA 220 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I221> KEY_SOUND 221 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I222> KEY_QUESTION 222 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I223> KEY_EMAIL 223 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I224> KEY_CHAT 224 */
	{Qt::Key_Search,        Qt::Key_Search},        /* <I225> KEY_SEARCH 225 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I226> KEY_CONNECT 226 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I227> KEY_FINANCE 227 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I228> KEY_SPORT 228 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I229> KEY_SHOP 229 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I230> KEY_ALTERASE 230 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I231> KEY_CANCEL 231 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I232> KEY_BRIGHTNESSDOWN 232 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I233> KEY_BRIGHTNESSUP 233 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I234> KEY_MEDIA 234 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I235> KEY_SWITCHVIDEOMODE 235 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I236> KEY_KBDILLUMTOGGLE 236 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I237> KEY_KBDILLUMDOWN 237 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I238> KEY_KBDILLUMUP 238 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I239> KEY_SEND 239 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I240> KEY_REPLY 240 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I241> KEY_FORWARDMAIL 241 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I242> KEY_SAVE 242 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I243> KEY_DOCUMENTS 243 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I244> KEY_BATTERY 244 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I245> KEY_BLUETOOTH 245 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I246> KEY_WLAN 246 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I247> KEY_UWB 247 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I248> KEY_UNKNOWN 248 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I249> KEY_VIDEO_NEXT 249 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I250> KEY_VIDEO_PREV 250 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I251> KEY_BRIGHTNESS_CYCLE 251 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I252> KEY_BRIGHTNESS_ZERO 252 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* <I253> KEY_DISPLAY_OFF 253 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* 254 */
	{Qt::Key_unknown,       Qt::Key_unknown},       /* 255 */
};

// vim: set ft=cpp:
