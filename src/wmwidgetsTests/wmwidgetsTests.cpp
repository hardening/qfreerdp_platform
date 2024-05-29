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

#include <wmwidgets/wmwidget.h>
#include <wmwidgets/wmlabel.h>
#include <wmwidgets/wmspacer.h>
#include <wmwidgets/wmcontainer.h>
#include <wmwidgets/wmiconbutton.h>

#include <QFont>
#include <QPainter>
#include <QApplication>

int main(int argc, char *argv[]) {
	Q_UNUSED(argc);
	Q_UNUSED(argv);

	QApplication app(argc, argv);
	QImage img(QSize(1024, 768), QImage::Format_ARGB32);
	QPainter painter(&img);

	WmTheme backRed = { Qt::white, Qt::red, QFont("time", 15, QFont::Bold)};
	WmTheme backGreen = { Qt::white, Qt::green, QFont("time", 15, QFont::Bold)};

	WmLabel label("the title with pop's", backRed);
	label.setSize(QSize(300, 30));
	label.setLocalPosition(QPoint(10, 20));

	label.repaint(painter, QPoint(0, 0));
	img.save("/tmp/wmwidgets-label.png", "PNG", 100);

	/* HContainer */
	WmHContainer hcontainer(backGreen);
	hcontainer.setSize(QSize(600, 30));

	WmSpacer spacer(QSize(30, 30), backRed);
	hcontainer.push(&spacer);

	hcontainer.push(&label, WMWIDGET_SIZEPOLICY_EXPAND);

	QImage imgClose(":/wmwidgetsTests/sample-icon.png");
	WmIconButton button(&imgClose, &imgClose, backRed);
	hcontainer.push(&button);

	WmSpacer spacer2(QSize(5, 30), backRed);
	hcontainer.push(&spacer2);

	/*hcontainer.repaint(painter, QPoint(0, 0));
	img.save("/tmp/wmwidgets-container.png", "PNG", 100);*/

	hcontainer.setSize(QSize(900, 30));
	hcontainer.repaint(painter, QPoint(0, 0));
	img.save("/tmp/wmwidgets-container2.png", "PNG", 100);

	Qt::MouseButtons buttons;
	hcontainer.handleMouse(QPoint(890, 15), buttons);
	hcontainer.repaint(painter, QPoint(0, 0));
	img.save("/tmp/wmwidgets-container3.png", "PNG", 100);

	return 0;
}
