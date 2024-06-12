sources = [
    'main.cpp',
    'qfreerdpplatform.cpp',
    'qfreerdpcompositor.cpp',
    'qfreerdpclipboard.cpp',
    'qfreerdpplatform.cpp',
    'qfreerdplistener.cpp',
    'qfreerdpscreen.cpp',
    'qfreerdpbackingstore.cpp',
    'qfreerdpwindow.cpp',
    'qfreerdppeer.cpp',
    'qfreerdppeerclipboard.cpp',
    'qfreerdpwindowmanager.cpp',
    'qfreerdpwmwidgets.cpp',
    'xcursors/xcursor.cpp',
    'xcursors/rdp-cursor.cpp',
    'xcursors/qfreerdpxcursor.cpp'
]

headers = [
    'qfreerdpcompositor.h',
    'qfreerdpwindow.h',
    'xcursors/cursor-data.h',
    'xcursors/xcursor.h',
    'xcursors/rdp-cursor.h',
    'xcursors/qfreerdpxcursor.h'
]

moc_headers = [
    'main.h',
    'qfreerdpplatform.h',
    'qfreerdplistener.h',
    'qfreerdpclipboard.h',
    'qfreerdpscreen.h',
    'qfreerdpbackingstore.h',
    'qfreerdppeer.h',
    'qfreerdppeerclipboard.h',
    'qfreerdpwindowmanager.h',
    'qfreerdpwmwidgets.h',
]

resources_files = qt5_mod.compile_resources(sources: ['freerdp.qrc'])

private_dirs = [
    qt5_deps.get_variable('QT_INSTALL_ARCHDATA') + '/mkspecs/' + qt5_deps.get_variable('QMAKE_SPEC')
]

moc_files = qt5_mod.compile_moc(
    headers : moc_headers,
    include_directories: [private_dirs],
    dependencies: [qt5_deps]
)

target_install_dir = join_paths(get_option('prefix'), get_option('libdir'), 'qt5', 'plugins', 'platforms')
if get_option('global_install')
    target_install_dir = join_paths(qt5_deps.get_variable('QT_INSTALL_PLUGINS'), 'platforms')
endif

message('installing plugin in @0@'.format(target_install_dir))

qfreerdp = library('qfreerdp', sources + headers, moc_files, resources_files,
    cpp_args: qt_defines + ['-DFREERDP_SETTINGS_INTERNAL_USE'],
    include_directories: ['../../..', private_dirs],
    dependencies: [qt5_deps, glib_deps, freerdp3_deps, xkbcommon_deps, xcursor_deps],
    link_with: libwmwidgets,
    install: true,
    install_dir: target_install_dir
)
