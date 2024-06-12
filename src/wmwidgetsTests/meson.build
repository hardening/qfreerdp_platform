sources = [
    'wmwidgetsTests.cpp'
]

resources = qt5_mod.compile_resources(sources: 'wmwidgetsTests.qrc')

wmwidgetsTests = executable('wmwidgetsTests', sources, resources,
    include_directories: ['..'],
    dependencies: [qt5_deps],
    link_with: libwmwidgets
)
