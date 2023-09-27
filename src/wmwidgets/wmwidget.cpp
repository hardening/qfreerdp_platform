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

#include "wmwidget.h"

WmColorScheme defaultColorScheme = {
	Qt::white, Qt::black
};


WmWidget::WmWidget(WmWidget *parent)
: mParent(parent)
, mPos(0, 0)
, mMinimumSize(1, 1)
, mMaximumSize(3000, 3000)
, mSize(1,  1)
, mColors(defaultColorScheme)
{
}

WmWidget* WmWidget::parent() const {
	return mParent;
}

void WmWidget::setParent(WmWidget* parent) {
	mParent = parent;
}

QRect WmWidget::geometry() {
	return QRect(position(), size());
}

QPoint WmWidget::localPosition() const {
	return mPos;
}

void WmWidget::setLocalPosition(const QPoint &pos) {
	mPos = pos;
}

QRect WmWidget::localGeometry() const {
	return QRect(mPos, mSize);
}


QPoint WmWidget::position() const {
	const WmWidget* p = parent();

	if (p)
		return p->position() + mPos;

	return mPos;
}

QSize WmWidget::size() const {
	return mSize;
}

QSize WmWidget::minSize() const {
	return mMinimumSize;
}

QSize WmWidget::maxSize() const {
	return mMaximumSize;
}


void WmWidget::setSize(const QSize &size) {
	mSize = size;
	handleResize();
}

void WmWidget::setColors(const WmColorScheme &colors) {
	mColors = colors;
}


void WmWidget::handleEnter(const QPoint &pos) {
	Q_UNUSED(pos);
}

void WmWidget::handleLeave() {
}

void WmWidget::handleMouse(const QPoint &pos, Qt::MouseButtons buttons) {
	Q_UNUSED(pos);
	Q_UNUSED(buttons);
}

void WmWidget::handleResize() {
}

void WmWidget::repaint(QPainter &painter, const QPoint &pos) {
	Q_UNUSED(painter);
	Q_UNUSED(pos);
}

void WmWidget::handleChildDirty(WmWidget* child, const QRegion &dirty) {
	Q_UNUSED(child);

	if (mParent)
		mParent->handleChildDirty(this, dirty.translated(mPos));
}
