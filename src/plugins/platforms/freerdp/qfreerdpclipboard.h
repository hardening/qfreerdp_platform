/*
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

#ifndef QFREERDP_CLIPBOARD_H_
#define QFREERDP_CLIPBOARD_H_

#include <qpa/qplatformclipboard.h>
#include <freerdp/server/cliprdr.h>

#include <QMimeData>
#include <QList>

class QFreerdpPeerClipboard;

/** @brief */
class QFreeRdpClipboard : public QObject, public QPlatformClipboard {
	Q_OBJECT

public:
	QFreeRdpClipboard();
	~QFreeRdpClipboard() override;

	void initialize();

	QMimeData *mimeData(QClipboard::Mode mode = QClipboard::Clipboard) override;
	void setMimeData(QMimeData *data, QClipboard::Mode mode = QClipboard::Clipboard) override;
	bool supportsMode(QClipboard::Mode mode) const override;
	bool ownsMode(QClipboard::Mode mode) const override;

	void registerPeer(QFreerdpPeerClipboard* peer);
	void unregisterPeer(QFreerdpPeerClipboard* peer);

	void updateAvailableData(QMimeData *data);
protected:
	QMimeData mEmptyData;
	QMimeData *mAvailableData;
	QList<QFreerdpPeerClipboard*> mPeers;
};


#endif /* QFREERDP_CLIPBOARD_H_ */
