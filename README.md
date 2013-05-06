# FreeRDP Qt platform plugin

## Application details

This is a Qt5 platform plugin to export application through RDP without modifying
the original code. 

## Requirements

* Qt5
* FreeRDP 1.1 (for now it's master)

## Installation

You'll need Qt5 installation, you can find precompiled Ubuntu packages in the [Qt5-edgers PPA](https://launchpad.net/~canonical-qt5-edgers/+archive/qt5-proper).

Requirements for FreeRDP can be found [here](https://github.com/FreeRDP/FreeRDP/wiki/Compilation)
The following instructions can be used to build a FreeRDP in a custom location:
> $ export INSTALL_PATH=$HOME/deploy
> $ git clone https://github.com/FreeRDP/FreeRDP.git
> $ cd FreeRDP
> $ cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_PATH -DCMAKE_BUILD_TYPE=Debug -DWITH_SSE2=ON .
> $ make install

Next to build the platform plugin with qt5-edgers packages (make usage of qtchooser):
> $ export PKG_CONFIG_PATH=$INSTALL_PATH/lib/pkgconfig/:$INSTALL_PATH/share/pkgconfig/
> $ qtchooser --qt=qt5 --run-tools=qmake
> $ make

## Using

To use the platform plugin you need to specify the path where QT can find plugins:
> export QT_PLUGIN_PATH=$(PWD)/plugins

Now launch any Qt5 program (you'll find many in the examples directory) with the
-platform freerdp parameter, for example:
> $ ./menus -platform freerdp

