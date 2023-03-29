# FreeRDP Qt platform plugin

## Application details

This is a Qt5 platform plugin to export application through RDP without modifying
the original code. 

## Requirements

* Qt5 development librairies (>= 5.12.8)
* rubycat-libfreerdp (>= 2.4.1)

## Set up dev environment
```
$ export INSTALL_PATH=/your/preferred/install/path
$ export PKG_CONFIG_PATH=${INSTALL_PATH}/usr/lib/pkgconfig:${INSTALL_PATH}/usr/lib/x86_64-linux-gnu/pkgconfig
$ export LD_LIBRARY_PATH=${INSTALL_PATH}/usr/lib/
```

## Install requirements (for Ubuntu 20.04)

### system libraries
    apt install qtbase5-private-dev libxkbcommon-dev libglib2.0-dev libxrender-dev libudev-dev libfontconfig1-dev libfreetype6-dev

### rubycat-libfreerdp
	see freerdp (or librcrdp) project

## Installation

### Get code
    git clone git@gitlab.rd.lan:developpers/libqfreerdp.git
    cd libqfreerdp

### Compilation
    qmake PREFIX=${INSTALL_PATH}/usr && make -j4 all

### Keyboard layouts
    Install windows specific layouts
    mkdir -p $HOME/.xkb/symbols/ && cp -rf xkbsymbols/* $HOME/.xkb/symbols/
    
## Using

To use the platform plugin you need to specify the path where QT can find plugins:
```
$ export QT_PLUGIN_PATH=${INSTALL_PATH}/usr/lib/
```

Now launch any Qt5 program (you'll find many in the examples directory) with the
-platform freerdp parameter, for example:
```
$ ./menus -platform freerdp
```
