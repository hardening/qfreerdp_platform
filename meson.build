project('qfreerdp', 'cpp',
  version : '1.0.0',
  meson_version : '>= 1.0',
  default_options : [ 'warning_level=1',
                      'buildtype=debugoptimized' ])

cxx = meson.get_compiler('cpp')
qt_defines = []

qt5_deps = dependency('qt5', modules: ['Core', 'Gui', 'Widgets', 'FontDatabaseSupport', 'EventDispatcherSupport', 'ThemeSupport', 'DBus'],
    #method:'qmake',
    private_headers: true
)
qt5_mod = import('qt5')


freerdp3_deps = []
freerdp3_deps += dependency('winpr3')
freerdp3_deps += dependency('freerdp3')
freerdp3_deps += dependency('freerdp-server3')

# dependencies of Qt
freerdp3_deps += dependency('fontconfig')

glib_deps = dependency('glib-2.0')
xkbcommon_deps = dependency('xkbcommon')
xcursor_deps = dependency('xcursor')


with_tests = get_option('tests').enabled()

subdir('src')
subdir('xkb')
