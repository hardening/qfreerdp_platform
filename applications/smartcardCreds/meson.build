
smartcardCreds_mocs = qt_mod.compile_moc(
    headers : ['smartcardCreds.h'],
    include_directories: [qfreerdp_inc],
    dependencies: [qt_deps]
)

smartcardCreds = executable('smartcardCreds', 'smartcardCreds.cpp', 'smartcardCreds.h', smartcardCreds_mocs,
	include_directories: [qfreerdp_inc],
	dependencies: [qt_deps, freerdp3_deps]
)