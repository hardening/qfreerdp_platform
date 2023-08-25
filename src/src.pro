TEMPLATE=subdirs
CONFIG += ordered
SUBDIRS += wmwidgets wmwidgetsTests plugins

plugins.depends = wmwidgets
wmwidgetsTests.depends = wmwidgets