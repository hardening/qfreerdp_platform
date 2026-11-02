/**
 * Copyright © 2024 David Fort <contact@hardening-consulting.com>
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

#ifndef QFREERDPEGLPLATFORMCONTEXT_H_
#define QFREERDPEGLPLATFORMCONTEXT_H_

#include <qpa/qplatformopenglcontext.h>
#include <gbm.h>
#include <EGL/egl.h>

#if QT_VERSION < QT_VERSION_CHECK(5, 8, 0)
#include <QtPlatformSupport/private/qeglplatformcontext_p.h>
#elif QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
#include <QtGui/private/qeglplatformcontext_p.h>
#else
#include <QtEglSupport/private/qeglplatformcontext_p.h>
#endif

class QFreeRdpRenderNodesWindow;
class QFreeRdpPlatform;

/**
 * @brief the render node / GBM device / EGL display triple.
 *
 * There's a single instance owned by the platform, every GL context and every
 * window surface is created on that display. EGL objects from different
 * displays can't be mixed (shared contexts, surfaces, ...).
 */
struct QFreeRdpEglDevice {
	/** opens the render node and initializes EGL on it
	 * @return the device, nullptr on failure
	 */
	static QFreeRdpEglDevice *create();

	~QFreeRdpEglDevice();

	int drmFd = -1;
	struct gbm_device *gbm = nullptr;
	EGLDisplay display = EGL_NO_DISPLAY;
};

/**
 * @brief an EGL context rendering into GBM surfaces on a render node
 */
class QFreeRdpEglPlatformContext : public QEGLPlatformContext {
public:
	/** Constructor
	 *
	 * @param platform the freerdp platform
	 * @param format surface format
	 * @param share a shared instance
	 * @param device the EGL device (owned by the platform)
	 */
	QFreeRdpEglPlatformContext(QFreeRdpPlatform *platform, const QSurfaceFormat &format,
			QPlatformOpenGLContext *share, QFreeRdpEglDevice *device);

	/** dtor */
	virtual ~QFreeRdpEglPlatformContext();

	/** @return the underlying gbm device */
	struct gbm_device *gbm() const;

	/** @{ */
	bool makeCurrent(QPlatformSurface *surface) override;
	void swapBuffers(QPlatformSurface *surface) override;
	/** @} */

	/** @return if render nodes are supported */
	static bool haveRenderNodesSupport();

	/** @return the path of the render node device, empty if none usable */
	static const QByteArray &renderNodePath();

protected:
	EGLSurface eglSurfaceForPlatformSurface(QPlatformSurface *surface) override;

	/** @return the render nodes window behind surface, nullptr if surface isn't one */
	static QFreeRdpRenderNodesWindow *glWindow(QPlatformSurface *surface);

protected:
	QFreeRdpPlatform *mPlatform;
	QFreeRdpEglDevice *mDevice;
};


#endif /* QFREERDPEGLPLATFORMCONTEXT_H_ */
