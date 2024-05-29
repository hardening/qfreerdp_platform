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

#include "wmlabel.h"

#include <QPainter>
#include <QDebug>

WmLabel::WmLabel(const QString &title, const WmTheme& theme, WmWidget *parent)
: WmWidget(theme, parent)
, mTitle(title)
, mFontMetrics(theme.font)
{
	mSize = mFontMetrics.boundingRect(title.length() ? title : " ").size();
}

void WmLabel::setTitle(const QString &title) {
	if (mTitle != title) {
		mTitle = title;
		handleChildDirty(this, localGeometry());
	}
}

void WmLabel::handleResize() {
}

void WmLabel::repaint(QPainter &painter, const QPoint &pos) {
	painter.fillRect(QRect(pos + mPos, size()), mTheme.backColor);

	painter.setFont(mTheme.font);
	painter.setPen(mTheme.frontColor);

	QRect renderedRect = mFontMetrics.boundingRect(mTitle);
	int y = (mSize.height() + mFontMetrics.height()/2)/2;
	QPoint dest((mSize.width() - renderedRect.width()) / 2, /*mSize.height() + renderedRect.top() / 2*/ y);
	painter.drawText(pos + mPos + dest, mTitle);

#ifdef DEBUG_LABEL
	qDebug() << "mSize=" << mSize << " rendered=" << renderedRect << " mPos=" << mPos << " destPos=" << pos + mPos + dest
			<< "height=" << mFontMetrics.height();
	painter.setPen(Qt::red);
	painter.drawLine(mPos + pos + QPoint(0, y), mPos + pos + QPoint(mSize.width()-1, y));
#endif
}
