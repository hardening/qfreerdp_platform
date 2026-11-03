gltest_qt_deps = dependency(qt_ver, modules: ['Core', 'Gui', 'Widgets', 'OpenGL', 'OpenGLWidgets', 'Qml', 'Quick'],
	required: false)

if gltest_qt_deps.found()
	gltest = executable('gltest', 'gltest.cpp',
		dependencies: [gltest_qt_deps],
		install: false
	)
else
	message('Qt Quick / OpenGLWidgets not found, not building gltest')
endif
