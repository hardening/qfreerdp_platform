/*
 * Copyright © 2013 Hardening <rdp.effort@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __QFREERDP_H___
#define __QFREERDP_H___

#include <QList>
#include <QSocketNotifier>
#include <qpa/qplatformintegration.h>
#include <qabstracteventdispatcher.h>
#include <freerdp/listener.h>

QT_BEGIN_NAMESPACE

class QFreeRdpListener;
class QFreeRdpPeer;
struct QFreeRdpPlatformConfig;
class QFreeRdpScreen;
class QFreeRdpWindow;
class QFreeRdpBackingStore;
class QFreeRdpWindowManager;

enum DisplayMode {
	UNKNOWN = 0,
	LEGACY = 1,
	AUTODETECT = 2,
	OPTIMIZE = 3
};

/**
 *
 */
class QFreeRdpPlatform : public QObject {
	friend class QFreeRdpScreen;
	friend class QFreeRdpBackingStore;
	friend class QFreeRdpWindow;
	friend class QFreeRdpPeer;
	friend class QFreeRdpListener;
public:
	/**
	 * @param dispatcher
	 */
	QFreeRdpPlatform(const QStringList& paramList, QAbstractEventDispatcher *dispatcher);

	virtual ~QFreeRdpPlatform();

	/** @return */
	QFreeRdpScreen *getScreen() { return mScreen; }

	/**
	 * @return listen address
	 */
	char* getListenAddress() const;

	/**
	 * @return listen port
	 */
	int getListenPort() const;

	/** registers a RDP peer
	 * @param peer
	 */
	void registerPeer(QFreeRdpPeer *peer);

	/** unregisters a RDP peer
	 * @param peer
	 */
	void unregisterPeer(QFreeRdpPeer *peer);

	/**
	 * @param region
	 */
	void repaint(const QRegion &region);

	void registerBackingStore(QWindow *w, QFreeRdpBackingStore *back);

	/** @return the event dispatcher */
	QAbstractEventDispatcher *getDispatcher() { return mDispatcher; }

	QPlatformWindow *newWindow(QWindow *window);

	void configureClient(rdpSettings *settings);

	DisplayMode getDisplayMode();

//public:
protected:
	QAbstractEventDispatcher *mDispatcher;
    QFreeRdpPlatformConfig *config;
    QFreeRdpScreen *mScreen;
    QFreeRdpWindowManager *mWindowManager;
	QFreeRdpListener *mListener;
};
QT_END_NAMESPACE


#endif /* __QFREERDP_H___ */
