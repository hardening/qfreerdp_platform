sources = [
    'wmcontainer.cpp',
    'wmcontainer.h',
    'wmiconbutton.cpp',
    'wmlabel.cpp',
    'wmlabel.h',
    'wmspacer.cpp',
    'wmspacer.h',
    'wmwidget.cpp'
]

moc_headers = [
    'wmiconbutton.h',
    'wmwidget.h'
]

moc_files = qt_mod.preprocess(moc_headers : moc_headers)

libwmwidgets = static_library('wmwidgets', sources, moc_files,
    include_directories: ['..'],
    dependencies: [qt_deps]
)
