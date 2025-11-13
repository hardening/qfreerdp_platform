/**
 * Copyright © 2023 Hardening <rdp.effort@gmail.com>
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

#pragma once

#include <QObject>
#include <QRegion>
#include <QFont>
#include <QColor>

QT_BEGIN_NAMESPACE

class QPainter;

/** @brief window manager theme */
struct WmTheme {
	QColor frontColor;
	QColor backColor;
	QFont font;
};


/** @brief base class for window manager widgets */
class WmWidget : public QObject {
	Q_OBJECT
public:
	/** @brief type of dragging */
	typedef enum {
		DRAGGING_NONE,				/*!< no dragging in progress */
		DRAGGING_MOVE,				/*!< moving a window */
		DRAGGING_RESIZE_TOP, 		/*!< resizing dragging the top border of the window */
		DRAGGING_RESIZE_TOP_LEFT, 	/*!< resizing dragging the top-left corner of window */
		DRAGGING_RESIZE_TOP_RIGHT,	/*!< resizing dragging the top-right corner of window */
		DRAGGING_RESIZE_BOTTOM,		/*!< resizing dragging the bottom border of the window */
		DRAGGING_RESIZE_BOTTOM_LEFT,/*!< resizing dragging the bottom-left corner of window */
		DRAGGING_RESIZE_BOTTOM_RIGHT,/*!< resizing dragging the bottom-right corner of window */
		DRAGGING_RESIZE_LEFT,		/*!< resizing dragging the left border of the window */
		DRAGGING_RESIZE_RIGHT,		/*!< resizing dragging the right border of the window */
	} DraggingType;

public:
	WmWidget(const WmTheme& theme, WmWidget *parent = nullptr);
	virtual ~WmWidget() = default;

	WmWidget* parent() const;
	void setParent(WmWidget* parent);

	QPoint localPosition() const;
	void setLocalPosition(const QPoint &pos);

	QRect localGeometry() const;

	virtual QRect geometry();
	QPoint position() const;
	QSize size() const;
	QSize minSize() const;
	QSize maxSize() const;

	void setSize(const QSize &size);

	/** Callbacks that widgets may re-implement
	 * @{ */
	virtual void handleEnter(const QPoint &pos);
	virtual void handleLeave();
	virtual void handleMouse(const QPoint &pos, Qt::MouseButtons buttons);
	virtual void handleResize();
	virtual void repaint(QPainter &painter, const QPoint &pos);
	virtual void handleChildDirty(WmWidget* child, const QRegion &dirty);
	/** @} */

signals:
	void startDrag(WmWidget::DraggingType draggingType);

protected:
	WmWidget *mParent;
	QPoint mPos;
	QSize mMinimumSize;
	QSize mMaximumSize;
	QSize mSize;
	const WmTheme& mTheme;
};

QT_END_NAMESPACE
