/**
 * Copyright © 2023 David Fort <contact@hardening-consulting.com>
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
#ifndef QFREERDPWMWIDGETS_H_
#define QFREERDPWMWIDGETS_H_

#include <QList>
#include <QRect>
#include <QRegion>
#include <QFont>
#include <QColor>
#include <QPainter>
#include <QMap>

#include <wmwidgets/wmwidget.h>
#include <wmwidgets/wmlabel.h>
#include <wmwidgets/wmspacer.h>
#include <wmwidgets/wmcontainer.h>
#include <wmwidgets/wmiconbutton.h>

QT_BEGIN_NAMESPACE

class QImage;
class IconResource;
class QFreeRdpWindow;

/** @brief wmwidget that does window's decoration */
class WmWindowDecoration : public WmWidget {
	Q_OBJECT
public:
	WmWindowDecoration(QFreeRdpWindow *w, const WmTheme &theme, const IconResource *closeIcons,
			WmWidget *parent = nullptr);
	~WmWindowDecoration();

	void setTitle(const QString &title);
	void resizeFromWindow(const QWindow *w);
	QRegion geometryRegion() const;
	void repaint(QPainter &painter, const QRegion &dirtyRegion);

	void handleResize() override;
	void handleMouse(const QPoint &pos, Qt::MouseButtons buttons) override;
	void handleLeave() override;
	void repaint(QPainter &painter, const QPoint &pos) override;
	void handleChildDirty(WmWidget* child, const QRegion &dirty) override;

signals:
	void startDrag(WmWidget::DraggingType dragType, QFreeRdpWindow *w);

public slots:
	void onStartDragging(WmWidget::DraggingType dragType);
	void onCloseClicked();

protected:
	QFreeRdpWindow *mWindow;
	WmSpacer *mTopSpacer;
	WmSpacer *mTopSpacer2;
	WmLabel *mTitle;
	WmIconButton *mCloseButton;
	WmHContainer *mTopContainer;

	WmWidget *mEnteredWidget;
	QRegion mGeometryRegion;
	bool mDirty;
	QImage *mContent;

	typedef struct {
		QRegion r;
		WmWidget::DraggingType action;
	} ResizeAction;
	typedef QList<ResizeAction> ResizeRegions;
	ResizeRegions mResizeRegions;
};

QT_END_NAMESPACE


#endif /* QFREERDPWMWIDGETS_H_ */
