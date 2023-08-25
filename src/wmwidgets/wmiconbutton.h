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

/** @brief a pressable icon */
class WmIconButton : public WmWidget {
	Q_OBJECT
public:
	WmIconButton(QImage *normal, QImage *over, WmWidget *parent = nullptr);

	void handleEnter(const QPoint &pos) override;
	void handleLeave() override;
	void handleMouse(const QPoint &pos, Qt::MouseButtons buttons) override;
	void repaint(QPainter &painter, const QPoint &pos) override;

signals:
	void clicked(void);

protected:
	QImage *mNormal;
	QImage *mOver;
	QImage *mCurrentImage;
	bool mButtonDown;
};

QT_END_NAMESPACE
