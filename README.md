# FreeRDP Qt platform plugin

## Application details

This is a Qt5 platform plugin to export application through RDP without modifying
the original code.

## Requirements

* Qt5 development librairies (>= 5.12.8)
* rubycat-libfreerdp (>= 2.100.20230321)

## Set up dev environment
```
$ export INSTALL_PATH=/your/preferred/install/path
$ export PKG_CONFIG_PATH=${INSTALL_PATH}/usr/lib/pkgconfig:${INSTALL_PATH}/usr/lib/x86_64-linux-gnu/pkgconfig
$ export LD_LIBRARY_PATH=${INSTALL_PATH}/usr/lib/
```

## Install requirements (for Ubuntu 20.04)

### system libraries
    apt install qtbase5-private-dev libxkbcommon-dev libglib2.0-dev libxrender-dev libudev-dev libfontconfig1-dev libfreetype6-dev

### rubycat-libfreerdp
	see freerdp project

## Installation

### Compilation
    meson setup build -Dprefix=${INSTALL_PATH}/usr -Dglobal_install=false
    ninja -C build install


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

You can also provide options to the freerdp platform:
```
$ ./menus -platform freerdp:port=3389:address=127.0.0.1
```

### Platform parameters list

| Parameter     | Example                   | Default           | Description |
| ------------- | ------------------------- | ----------------- | --- |
| `address`     | `:address=127.0.0.1`      | `0.0.0.0`         | Bind (listen) IP address for the RDP server |
| `port`        | `:port=3392`              | `3389`            | Listening port for the RDP server |
| `socket`      | `:socket=`                |                   | Fixed socket |
| `width`       | `:width=1024`             | `800`             | Initial screen width, in pixels |
| `height`      | `:height=768`             | `600`             | Initial screen height, in pixels |
| `cert`        | `:cert=/path/to/cert.pem` | `cert.pem`        | Path to TLS certificate |
| `key`         | `:key=/path/to/key.key`   | `cert.key`        | Path to TLS key |
| `secrets`     | `:secrets=`               |                   | Path to secrets file |
| `fg-color`    | `:fg-color=#ebbdb2`       | `white`           | Foreground color for window decorations, accepts hex-formatted colors and colors from https://doc.qt.io/qt-5/qcolor.html#setNamedColor |
| `bg-color`    | `:bg-color=#282828`       | `black`           | Background color for window decorations, accepts hex-formatted colors and colors from https://doc.qt.io/qt-5/qcolor.html#setNamedColor |
| `font`        | `:font=Oswald`            | `time`            | Font name for window titles |
| `fps`         | `:fps=60`                 | `24`              | Target internal rendering framerate |
| `mode`        | `:mode=optimize`          | `autodetect`      | Display modes. Values: `legacy\|autodetect\|optimize` |
| `noegfx`      | `:noegfx`                 | egfx enabled      | Flag to disable egfx rendering |
| `noclipboard` | `:noclipboard`            | clipboard enabled | Flag to disable clipboard access |
