project('qfreerdp', 'cpp',
  version : '1.0.0',
  meson_version : '>= 0.63',  # before version 0.63.0 Meson would fail to find Qt 6.1 or later
  default_options : [ 'warning_level=1',
                      'buildtype=debugoptimized' ])

cxx = meson.get_compiler('cpp')

qt_ver = get_option('qt_version')

# Autodetecting Qt version when asked
if qt_ver == 'auto'
    message('Autodetecting Qt version:')
    qt_ver = 'qt6'
    if not dependency(qt_ver, modules: ['Core'], required: false).found()
        qt_ver = 'qt5'
    endif
endif

qt_mod = import(qt_ver)

qt_modules = ['Core', 'Gui', 'Widgets', 'DBus', 'Test']
if qt_ver == 'qt5'
    qt_modules += ['FontDatabaseSupport', 'EventDispatcherSupport', 'ThemeSupport']
endif

qt_deps = dependency(qt_ver, modules: qt_modules,
    method: 'qmake',  # We force qmake here to be able to use get_variable on Qt variables later
    private_headers: true
)


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


# Windows specific keyboard layouts
xkb_data_path = get_option('xkb_data_path')
if xkb_data_path == ''
    xkb_data_path = join_paths(get_option('prefix'), get_option('datadir'), 'X11', 'xkb')
endif

install_subdir('xkb',
    install_dir: xkb_data_path,
    strip_directory: true
)

subdir('src')
