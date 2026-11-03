#ifndef __QFREERDPRENDERNODESWINDOW_H__
#define __QFREERDPRENDERNODESWINDOW_H__

#include <qpa/qplatformwindow.h>

#include <QMutex>
#include <QImage>
#include <QElapsedTimer>

#include <gbm.h>
#include <EGL/egl.h>

#include "qfreerdpwindowmanager.h"
#include "qfreerdpwindow.h"

class QFreeRdpEglPlatformContext;
class QOpenGLFunctions;

/**
 * @brief a window rendered with GL in a GBM surface
 *
 * Threading: the EGL/GBM surfaces are only touched from the thread that makes a
 * context current on the window (prepareSurface() / grabContent()), the main thread
 * only records the wanted size (setGeometry()) and reads the last grabbed frame
 * (windowContent()). Both sides exchange data under mLock.
 */
class QFreeRdpRenderNodesWindow : public QFreeRdpWindow
{
public:
	QFreeRdpRenderNodesWindow(QWindow *window, QFreeRdpPlatform *platform);

	/** dtor */
	virtual ~QFreeRdpRenderNodesWindow();

	/** @return the associated EGL surface */
	EGLSurface surface() const;

	void setGeometry(const QRect &rect) override;

	const QImage *windowContent() override;

	/** ensures the EGL surface exists, has the right size and is compatible
	 * with the given context. Must be called before the context is made current.
	 * @return if the operation was successful
	 */
	bool prepareSurface(QFreeRdpEglPlatformContext *context);

	/** reads the content of the current surface, must be called with the
	 * window's surface current, before swapping buffers */
	void grabContent(QOpenGLFunctions *f);

	/** consumes the front buffer after a swap (we never scan it out, the content
	 * has been grabbed before) so that GBM can recycle it */
	void releaseFrontBuffer();

	/** called in the main thread when a new frame has been grabbed */
	void notifyFrameReady();

	/** blocks so that the frame rate doesn't exceed fps */
	void throttle(int fps);

protected:
	void destroyEglItems();

protected:
	/* render side */
	EGLDisplay mDisplay;
	EGLConfig mConfig;
	struct gbm_surface *mGbmSurface;
	EGLSurface mSurface;
	QSize mSurfaceSize;
	QElapsedTimer mLastSwap;

	/* shared between main and render side */
	QMutex mLock;
	QSize mWantedSize;
	QImage mFrontImage;

	/* main thread side */
	QImage mDisplayImage;
	bool mGotFirstFrame;
};


#endif /* __QFREERDPRENDERNODESWINDOW_H_ */
