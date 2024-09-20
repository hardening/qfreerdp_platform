sources = [
    'wmwidgetsTests.cpp'
]

resources = qt_mod.compile_resources(sources: 'wmwidgetsTests.qrc')

wmwidgetsTests = executable('wmwidgetsTests', sources, resources,
    include_directories: ['..'],
    dependencies: [qt_deps],
    link_with: libwmwidgets
)
