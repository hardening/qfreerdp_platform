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

#include <wmwidgets/wmwidget.h>

QT_BEGIN_NAMESPACE

/** @brief size / resize policy */
typedef enum {
	WMWIDGET_SIZEPOLICY_FIXED,
	WMWIDGET_SIZEPOLICY_EXPAND
} WmWidgetSizePolicy;

/** @brief a widget in a container */
typedef struct {
	WmWidgetSizePolicy policy;
	WmWidget* widget;
} WmContainerItem;

/** @brief an horizontal container */
class WmHContainer : public WmWidget {
public:
	WmHContainer(const WmTheme& theme, WmWidget *parent = nullptr);
	~WmHContainer();
	void push(WmWidget* item, WmWidgetSizePolicy policy = WMWIDGET_SIZEPOLICY_FIXED);

	/**  @overload of WmWidget
	 * @{ */
	void handleEnter(const QPoint &pos) override;
	void handleLeave() override;
	void handleMouse(const QPoint &pos, Qt::MouseButtons buttons) override;
	void handleResize() override;
	void repaint(QPainter &painter, const QPoint &pos) override;
	/** @} */


protected:
	void computeMinimums();
	void recomputeSizesAndPos();

protected:
	QList<WmContainerItem> mItems;
	WmWidget *mEntered;
};

QT_END_NAMESPACE
