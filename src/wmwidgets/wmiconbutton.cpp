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

#include "wmiconbutton.h"

#include <QPainter>
#include <QDebug>

WmIconButton::WmIconButton(QImage *normal, QImage *over, WmWidget *parent)
: WmWidget(parent)
, mNormal(normal)
, mOver(over)
, mCurrentImage(mNormal)
, mButtonDown(false)
{
	mMinimumSize = mMaximumSize = mSize = normal->size();
}

void WmIconButton::handleEnter(const QPoint &pos) {
	Q_UNUSED(pos);
	qDebug("WmIconButton::handleEnter");
	mCurrentImage = mOver;

	handleChildDirty(this, QRect(QPoint(0, 0), mSize));
}

void WmIconButton::handleLeave() {
	qDebug("WmIconButton::handleLeave");
	mCurrentImage = mNormal;
	mButtonDown = false;
	handleChildDirty(this, QRect(QPoint(0, 0), mSize));
}

void WmIconButton::handleMouse(const QPoint &pos, Qt::MouseButtons buttons) {
	Q_UNUSED(pos);

	if (buttons & Qt::LeftButton) {
		/* button clicked */
		mButtonDown = true;
	}

	if (mButtonDown && !(buttons & Qt::LeftButton)) {
		/* button released */
		mButtonDown = false;
		emit clicked();
	}
}

void WmIconButton::repaint(QPainter &painter, const QPoint &pos) {
	painter.drawImage(pos + mPos, *mCurrentImage);
}
