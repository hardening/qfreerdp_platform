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

#include "wmcontainer.h"

#include <QPainter>
#include <QDebug>

WmHContainer::WmHContainer(WmWidget *parent)
: WmWidget(parent)
, mEntered(nullptr)
{
}

WmHContainer::~WmHContainer() {
	foreach(auto item, mItems) {
		delete item.widget;
	}
}

void WmHContainer::push(WmWidget* item, WmWidgetSizePolicy policy) {
	item->setParent(this);
	mItems.push_back({ policy, item });

	handleResize();
}


void WmHContainer::handleEnter(const QPoint &pos) {
	foreach(auto item, mItems) {
		auto widget = item.widget;
		if (widget->localGeometry().contains(pos)) {
			widget->handleEnter(pos - widget->localPosition());
			mEntered = widget;
			break;
		}
	}
}

void WmHContainer::handleLeave() {
	if (mEntered) {
		mEntered->handleLeave();
		mEntered = nullptr;
	}
}

void WmHContainer::handleMouse(const QPoint &pos, Qt::MouseButtons buttons) {
	foreach(auto item, mItems) {
		auto widget = item.widget;
		if (widget->localGeometry().contains(pos)) {
			QPoint localPos = (pos - widget->localPosition());
			if (mEntered != widget) {
				if (mEntered)
					mEntered->handleLeave();
				widget->handleEnter(localPos);
				mEntered = widget;
			}

			widget->handleMouse(localPos, buttons);
			break;
		}
	}
}

static int containerComputeCentered(int containerSize, int containedSize) {
	return (containerSize - containedSize) / 2;
}

void WmHContainer::handleResize() {
	computeMinimums();
	recomputeSizesAndPos();
	handleChildDirty(this, localGeometry());
}

void WmHContainer::recomputeSizesAndPos() {
	int availableForExpand = mSize.width();
	int reserved = mMinimumSize.width();
	int xoffset = 0;

	Q_ASSERT(availableForExpand);

	foreach(auto item, mItems) {
		auto widget = item.widget;
		QPoint wPos = widget->localPosition();
		auto wSize = widget->size();
		int newWidth;
		QPoint newPos;
		QSize newSize = wSize;

		newPos = QPoint(xoffset, containerComputeCentered(mSize.height(), wSize.height()));

		reserved -= widget->minSize().width();

		switch(item.policy) {
		case WMWIDGET_SIZEPOLICY_EXPAND:
			newWidth = (availableForExpand - reserved);
			if (newWidth > mMaximumSize.width())
				newWidth = mMaximumSize.width();
			break;
		case WMWIDGET_SIZEPOLICY_FIXED:
		default:
			newWidth = wSize.width();
			break;
		}

		availableForExpand -= newWidth;
		xoffset += newWidth;

		newSize = QSize(newWidth, wSize.height());
		if (wPos != newPos)
			widget->setLocalPosition(newPos);

		if (newSize != wSize)
			widget->setSize(newSize);
	}
}


void WmHContainer::repaint(QPainter &painter, const QPoint &pos) {
	painter.fillRect(QRect(pos + mPos, mSize), mColors.backColor);

	foreach(auto item, mItems) {
		auto widget = item.widget;
		/*qDebug() << "repaint: pos=" << item.widget->localPosition();
		if (widget->isDirty()) {
			qDebug() << "repainted";*/
			widget->repaint(painter, mPos);
		//}
	}
}

void WmHContainer::computeMinimums() {
	int minimumWidth = 0, minimumHeight = 0;

	foreach(auto item, mItems) {
		auto widget = item.widget;

		switch(item.policy) {
		case WMWIDGET_SIZEPOLICY_FIXED:
			minimumWidth += widget->size().width();
			break;
		case WMWIDGET_SIZEPOLICY_EXPAND:
			minimumWidth += widget->minSize().width();
			break;
		}

		int minH = widget->minSize().height();
		if (minimumHeight < minH)
			minimumHeight = minH;
	}

	if (!minimumWidth)
		minimumWidth = 1;
	if (!minimumHeight)
		minimumHeight = 1;

	mMinimumSize = QSize(minimumWidth, minimumHeight);
}

