
#include "qfreerdprendernodeswindow.h"
#include "qfreerdpplatform.h"
#include "qfreerdpeglplatformcontext.h"
#include "qfreerdpwindowmanager.h"

#include <QtGui/QScreen>
#include <QtGui/QImage>
#include <QtGui/QOpenGLFunctions>
#include <QMutexLocker>
#include <QThread>

#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif

static const QImage::Format imgFormat = QImage::Format_RGBA8888_Premultiplied;

QFreeRdpRenderNodesWindow::QFreeRdpRenderNodesWindow(QWindow *w, QFreeRdpPlatform *platform)
: QFreeRdpWindow(w, platform)
, mDisplay(EGL_NO_DISPLAY)
, mConfig(nullptr)
, mGbmSurface(nullptr)
, mSurface(EGL_NO_SURFACE)
, mWantedSize(geometry().size())
, mGotFirstFrame(false)
{
}

QFreeRdpRenderNodesWindow::~QFreeRdpRenderNodesWindow() {
	destroyEglItems();
}

EGLSurface QFreeRdpRenderNodesWindow::surface() const {
	return mSurface;
}

void QFreeRdpRenderNodesWindow::setGeometry(const QRect &cgeom) {
	QRect geom = cgeom;
	if (geom.width() <= 0)
		geom.setWidth(1);
	if (geom.height() <= 0)
		geom.setHeight(1);

	{
		/* the surface itself is resized in prepareSurface(), from the render thread */
		QMutexLocker locker(&mLock);
		mWantedSize = geom.size();
	}

	QFreeRdpWindow::setGeometry(geom);
}

void QFreeRdpRenderNodesWindow::destroyEglItems() {
	if (mSurface != EGL_NO_SURFACE) {
		/* if the surface is current in this thread, release it first so that it's
		 * really destroyed now (and not when unbound, after its gbm surface is gone) */
		if (eglGetCurrentSurface(EGL_DRAW) == mSurface || eglGetCurrentSurface(EGL_READ) == mSurface)
			eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

		eglDestroySurface(mDisplay, mSurface);
		mSurface = EGL_NO_SURFACE;
	}

	/* the gbm surface must outlive the EGL surface */
	if (mGbmSurface) {
		gbm_surface_destroy(mGbmSurface);
		mGbmSurface = nullptr;
	}

	mSurfaceSize = QSize();
}

bool QFreeRdpRenderNodesWindow::prepareSurface(QFreeRdpEglPlatformContext *context) {
	QSize wanted;
	{
		QMutexLocker locker(&mLock);
		wanted = mWantedSize.expandedTo(QSize(1, 1));
	}

	EGLDisplay display = context->eglDisplay();
	EGLConfig config = context->eglConfig();
	if (mSurface != EGL_NO_SURFACE && mSurfaceSize == wanted && mDisplay == display && mConfig == config)
		return true;

	destroyEglItems();

	/* use the format that the EGL config expects for native windows */
	EGLint visualId = 0;
	if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &visualId) || !visualId)
		visualId = GBM_FORMAT_XRGB8888;

	mGbmSurface = gbm_surface_create(context->gbm(), wanted.width(), wanted.height(), (uint32_t)visualId,
			GBM_BO_USE_RENDERING);
	if (!mGbmSurface) {
		qWarning("QFreeRdpRenderNodesWindow(%d): unable to create a %dx%d GBM surface (format 0x%x)", (int)mWinId,
				wanted.width(), wanted.height(), visualId);
		return false;
	}

	mSurface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)mGbmSurface, NULL);
	if (mSurface == EGL_NO_SURFACE) {
		qWarning("QFreeRdpRenderNodesWindow(%d): unable to create an EGL window surface (error 0x%x)",
				(int)mWinId, eglGetError());
		gbm_surface_destroy(mGbmSurface);
		mGbmSurface = nullptr;
		return false;
	}

	mDisplay = display;
	mConfig = config;
	mSurfaceSize = wanted;
	return true;
}

void QFreeRdpRenderNodesWindow::grabContent(QOpenGLFunctions *f) {
	if (mSurface == EGL_NO_SURFACE || mSurfaceSize.isEmpty())
		return;

	QImage img(mSurfaceSize, imgFormat);
	if (img.isNull())
		return;

	GLint prevFbo = 0, prevAlign = 4;
	f->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
	f->glGetIntegerv(GL_PACK_ALIGNMENT, &prevAlign);
	if (prevFbo)
		f->glBindFramebuffer(GL_FRAMEBUFFER, 0);
	f->glPixelStorei(GL_PACK_ALIGNMENT, 4);

	/* the image has the same size as the surface, so no overflow is possible */
	f->glReadPixels(0, 0, img.width(), img.height(), GL_RGBA, GL_UNSIGNED_BYTE, img.bits());

	f->glPixelStorei(GL_PACK_ALIGNMENT, prevAlign);
	if (prevFbo)
		f->glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);

	img = img.mirrored(false, true);

	QMutexLocker locker(&mLock);
	mFrontImage = std::move(img);
}

const QImage *QFreeRdpRenderNodesWindow::windowContent() {
	/* QImage is implicitly shared (atomic refcount), the render thread will only
	 * replace mFrontImage, never write in it */
	{
		QMutexLocker locker(&mLock);
		mDisplayImage = mFrontImage;
	}

	return mDisplayImage.isNull() ? nullptr : &mDisplayImage;
}

void QFreeRdpRenderNodesWindow::throttle(int fps) {
	if (fps <= 0)
		return;

	const qint64 frameNs = 1000000000LL / fps;
	if (mLastSwap.isValid()) {
		qint64 remaining = frameNs - mLastSwap.nsecsElapsed();
		if (remaining > 0 && remaining <= frameNs)
			QThread::usleep(remaining / 1000);
	}
	mLastSwap.start();
}

void QFreeRdpRenderNodesWindow::notifyFrameReady() {
	if (!mGotFirstFrame) {
		/* the window manager skips windows without content (decorations included),
		 * so repaint the decorations too when the first frame arrives */
		mGotFirstFrame = true;
		notifyDirty(QRegion(outerWindowGeometry()));
		return;
	}

	notifyDirty(QRegion(geometry()));
}

void QFreeRdpRenderNodesWindow::releaseFrontBuffer() {
	if (!mGbmSurface)
		return;

	struct gbm_bo *bo = gbm_surface_lock_front_buffer(mGbmSurface);
	if (bo)
		gbm_surface_release_buffer(mGbmSurface, bo);
}
