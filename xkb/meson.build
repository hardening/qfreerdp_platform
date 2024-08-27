destInstallDir = get_option('prefix') / get_option('datadir') / 'xkb'

if get_option('global_install')
	destInstallDir = '/usr/share/X11/xkb'
endif

install_data(
	'rules/qfreerdp',
	install_dir: destInstallDir / 'rules',
)

install_data(
	[
		'symbols/rubycat_be_windows',
		'symbols/rubycat_fr_windows',
		'symbols/rubycat_gb_windows',
		'symbols/rubycat_latam_windows',
		'symbols/rubycat_latin'
	],
	install_dir: destInstallDir / 'symbols',
)

install_data(
	'types/qfreerdp_lvl3_ctrlalt',
	install_dir: destInstallDir / 'types',
)