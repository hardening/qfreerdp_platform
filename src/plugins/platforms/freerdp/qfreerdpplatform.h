/**
 * Copyright © 2013-2023 David Fort <contact@hardening-consulting.com>
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
#include <QtGui/qpa/qplatforminputcontextfactory_p.h>

#include <freerdp/listener.h>
#include <freerdp/pointer.h>

QT_BEGIN_NAMESPACE

class QFreeRdpListener;
class QFreeRdpPeer;
struct QFreeRdpPlatformConfig;
class QFreeRdpScreen;
class QFreeRdpWindow;
class QFreeRdpBackingStore;
class QFreeRdpWindowManager;
class QFreeRdpClipboard;

enum DisplayMode {
	UNKNOWN = 0,
	LEGACY = 1,
	AUTODETECT = 2,
	OPTIMIZE = 3
};

typedef enum {
	ICON_RESOURCE_CLOSE_BUTTON
} IconResourceType;

/** @brief image resources associated with an icon button*/
struct IconResource {
	~IconResource();

	QImage *normalIcon;
	QImage *overIcon;
};

/** @brief configuration for FreeRdpPlatform */
struct QFreeRdpPlatformConfig {
	/**
	 * @param params list of parameters
	 */
	QFreeRdpPlatformConfig(const QStringList &params);

	~QFreeRdpPlatformConfig();

	char *bind_address;
	int port;
	int fixed_socket;

	char *server_cert;
	char *server_key;
	char *rdp_key;
	bool tls_enabled;
	int fps;
	bool clipboard_enabled;
	bool egfx_enabled;
	char *secrets_file;

	QSize screenSz;
	DisplayMode displayMode;
};


/**
 * @brief
 */
class QFreeRdpPlatform : public QObject, public QPlatformIntegration {
	friend class QFreeRdpScreen;
	friend class QFreeRdpCursor;
	friend class QFreeRdpBackingStore;
	friend class QFreeRdpWindow;
	friend class QFreeRdpPeer;
	friend class QFreeRdpListener;

	Q_OBJECT
public:
	/**
	 * @param dispatcher
	 */
	QFreeRdpPlatform(const QStringList& paramList);

	virtual ~QFreeRdpPlatform();

    /** @overload QPlatformIntegration
     * @{ */
    virtual bool hasCapability(QPlatformIntegration::Capability cap) const;
    virtual QPlatformWindow *createPlatformWindow(QWindow *window) const;
    virtual QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const;
    virtual QPlatformFontDatabase *fontDatabase() const;
    virtual QStringList themeNames() const;
    virtual QPlatformTheme *createPlatformTheme(const QString &name) const;
    virtual QPlatformNativeInterface *nativeInterface()const;
    virtual QPlatformInputContext *inputContext() const;
    virtual QPlatformClipboard *clipboard() const;
    virtual void initialize();

#if QT_VERSION < 0x050200
    virtual QAbstractEventDispatcher *guiThreadEventDispatcher() const;
#else
    virtual QAbstractEventDispatcher *createEventDispatcher() const;
#endif
    /** @} */


	/** @return the main screen */
	QFreeRdpScreen *getScreen() { return mScreen; }

	/** @return */
	QFreeRdpClipboard *rdpClipboard() const { return mClipboard; }

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
	QAbstractEventDispatcher *getDispatcher() { return mEventDispatcher; }

	void configureClient(rdpSettings *settings);

	DisplayMode getDisplayMode();

	void setBlankCursor();
	void setPointer(const POINTER_LARGE_UPDATE *pointer, Qt::CursorShape newShape);

	const IconResource *getIconResource(IconResourceType rtype);

protected:
	bool loadResources();

protected:
    QPlatformFontDatabase *mFontDb;
    QAbstractEventDispatcher *mEventDispatcher;
    QPlatformNativeInterface *mNativeInterface;
    QScopedPointer<QPlatformInputContext> mInputContext;
    QFreeRdpClipboard *mClipboard;

    QFreeRdpPlatformConfig *mConfig;
    QFreeRdpScreen *mScreen;
    QFreeRdpWindowManager *mWindowManager;
	QFreeRdpListener *mListener;
	bool mResourcesLoaded;
	QMap<IconResourceType, IconResource*> mResources;
	QList<QFreeRdpPeer *> mPeers;
};
QT_END_NAMESPACE


#endif /* __QFREERDP_H___ */
