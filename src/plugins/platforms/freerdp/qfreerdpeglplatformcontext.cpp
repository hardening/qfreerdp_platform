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

#include "qfreerdpplatform.h"
#include "qfreerdpeglplatformcontext.h"
#include "qfreerdprendernodeswindow.h"
#include "qfreerdpwindowmanager.h"

#include <fcntl.h>
#include <unistd.h>
#include <libudev.h>
#include <errno.h>
#include <string.h>

#include <QOpenGLContext>
#include <QPointer>
#include <QWindow>
#include <qdebug.h>

#include <EGL/eglext.h>

static QByteArray findRenderNode() {
	QByteArray ret;
	struct udev *udev = udev_new();
	if (!udev)
		return ret;

	struct udev_enumerate *e = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(e, "drm");
	udev_enumerate_add_match_sysname(e, "renderD[0-9]*");
	udev_enumerate_scan_devices(e);

	struct udev_list_entry *entry;
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		const char *path = udev_list_entry_get_name(entry);
		struct udev_device *device = udev_device_new_from_syspath(udev, path);
		if (!device)
			continue;

		const char *filename = udev_device_get_devnode(device);
		int fd = filename ? open(filename, O_RDWR | O_CLOEXEC) : -1;
		if (fd < 0) {
			if (filename)
				qDebug("unable to open %s: %s", filename, strerror(errno));
			udev_device_unref(device);
			continue;
		}

		close(fd);
		ret = filename;
		udev_device_unref(device);
		break;
	}

	udev_enumerate_unref(e);
	udev_unref(udev);
	return ret;
}

const QByteArray &QFreeRdpEglPlatformContext::renderNodePath() {
	/* thread safe initialization (contexts may be created from render threads) */
	static const QByteArray path = findRenderNode();
	return path;
}

bool QFreeRdpEglPlatformContext::haveRenderNodesSupport() {
	return !renderNodePath().isEmpty();
}


QFreeRdpEglDevice *QFreeRdpEglDevice::create() {
	if (!QFreeRdpEglPlatformContext::haveRenderNodesSupport()) {
		qWarning("no render nodes support");
		return nullptr;
	}

	QFreeRdpEglDevice *ret = new QFreeRdpEglDevice();
	ret->drmFd = open(QFreeRdpEglPlatformContext::renderNodePath().constData(), O_RDWR | O_CLOEXEC);
	if (ret->drmFd < 0) {
		qWarning("unable to open renderNodes device");
		delete ret;
		return nullptr;
	}

	ret->gbm = gbm_create_device(ret->drmFd);
	if (!ret->gbm) {
		qWarning("unable to create gbm device");
		delete ret;
		return nullptr;
	}

#if defined(EGL_MESA_platform_gbm) && defined(EGL_VERSION_1_5)
	ret->display = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, ret->gbm, NULL);
#elif defined(EGL_MESA_platform_gbm)
	PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
			(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
	if (getPlatformDisplay)
		ret->display = getPlatformDisplay(EGL_PLATFORM_GBM_MESA, ret->gbm, NULL);
#else
	ret->display = eglGetDisplay((EGLNativeDisplayType)ret->gbm);
#endif
	if (ret->display == EGL_NO_DISPLAY) {
		qWarning("unable to open an EGL display");
		delete ret;
		return nullptr;
	}

	EGLint major, minor;
	if (!eglInitialize(ret->display, &major, &minor)) {
		qWarning("error initializing EGL display");
		ret->display = EGL_NO_DISPLAY;
		delete ret;
		return nullptr;
	}

	qDebug("EGL version %d.%d on %s", major, minor, QFreeRdpEglPlatformContext::renderNodePath().constData());
	return ret;
}

QFreeRdpEglDevice::~QFreeRdpEglDevice() {
	if (display != EGL_NO_DISPLAY)
		eglTerminate(display);
	if (gbm)
		gbm_device_destroy(gbm);
	if (drmFd >= 0)
		close(drmFd);
}


QFreeRdpEglPlatformContext::QFreeRdpEglPlatformContext(QFreeRdpPlatform *platform,
			const QSurfaceFormat &format,
			QPlatformOpenGLContext *share,
			QFreeRdpEglDevice *device)
: QEGLPlatformContext(format, share, device->display)
, mPlatform(platform)
, mDevice(device)
{
}

QFreeRdpEglPlatformContext::~QFreeRdpEglPlatformContext() {
}

struct gbm_device *QFreeRdpEglPlatformContext::gbm() const {
	return mDevice->gbm;
}

QFreeRdpRenderNodesWindow *QFreeRdpEglPlatformContext::glWindow(QPlatformSurface *surface) {
	if (!surface || surface->surface()->surfaceClass() != QSurface::Window)
		return nullptr;
	return dynamic_cast<QFreeRdpRenderNodesWindow *>(surface);
}

EGLSurface QFreeRdpEglPlatformContext::eglSurfaceForPlatformSurface(QPlatformSurface *surface) {
	QFreeRdpRenderNodesWindow *win = glWindow(surface);
	return win ? win->surface() : EGL_NO_SURFACE;
}

bool QFreeRdpEglPlatformContext::makeCurrent(QPlatformSurface *surface) {
	QFreeRdpRenderNodesWindow *window = glWindow(surface);
	if (!window) {
		qWarning("QFreeRdpEglPlatformContext::makeCurrent: unsupported surface %p", (void*)surface);
		return false;
	}

	/* (re)creates the EGL surface if needed, this is done here so that it
	 * happens in the thread that owns the context */
	if (!window->prepareSurface(this))
		return false;

	return QEGLPlatformContext::makeCurrent(surface);
}

void QFreeRdpEglPlatformContext::swapBuffers(QPlatformSurface *surface) {
	QFreeRdpRenderNodesWindow *window = glWindow(surface);
	if (!window)
		return;

	/* the window's surface is bound right now: grab the frame before the swap */
	if (QOpenGLContext::currentContext() == context())
		window->grabContent(context()->functions());

	QEGLPlatformContext::swapBuffers(surface);
	window->releaseFrontBuffer();

	/* GBM surfaces have no vblank: emulate vsync at the RDP frame rate, otherwise
	 * clients render (and we read back) thousands of useless frames per second */
	if (format().swapInterval() > 0)
		window->throttle(mPlatform->config()->fps / format().swapInterval());

	/* we may be in a render thread, notify the window manager from the main thread */
	QPointer<QWindow> qwindow = window->window();
	QMetaObject::invokeMethod(mPlatform->mWindowManager, [qwindow]() {
		if (!qwindow)
			return;

		QFreeRdpRenderNodesWindow *pwindow = dynamic_cast<QFreeRdpRenderNodesWindow *>(qwindow->handle());
		if (pwindow)
			pwindow->notifyFrameReady();
	}, Qt::QueuedConnection);
}
