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

#include "qfreerdpclipboard.h"
#include "qfreerdppeerclipboard.h"

#include <QDebug>

QFreeRdpClipboard::QFreeRdpClipboard()
: mAvailableData(nullptr)
{
}

QFreeRdpClipboard::~QFreeRdpClipboard() {
}

void QFreeRdpClipboard::initialize() {

}

QMimeData *QFreeRdpClipboard::mimeData(QClipboard::Mode mode) {
	QMimeData *ret = mAvailableData ? mAvailableData : &mEmptyData;
	qDebug() << "mimeData(" << mode << "): text=" << ret->hasText()
			 << " html=" << ret->hasHtml();

	return ret;
}

void QFreeRdpClipboard::updateAvailableData(QMimeData *data) {
	if (mAvailableData)
		delete mAvailableData;

	mAvailableData = data;
	emitChanged(QClipboard::Clipboard);
}

void QFreeRdpClipboard::setMimeData(QMimeData *data, QClipboard::Mode mode) {
	switch (mode) {
    //case QClipboard::Selection:
    case QClipboard::Clipboard:
    	if (data)
    		qDebug() << "setMimeData(): mode=" << mode << " text=" << data->hasText()
    				<< " html=" << data->hasHtml()
    				<< " url=" << data->hasUrls();
    	else
    		qDebug() << "no data";

    	foreach(QFreerdpPeerClipboard* peer, mPeers) {
    		peer->setClipboardData(data);
    	}
    	break;
    default:
    	break;
    }
}

bool QFreeRdpClipboard::supportsMode(QClipboard::Mode mode) const  {
	switch (mode) {
	case QClipboard::Clipboard:
	case QClipboard::Selection:
		return true;
	default:
		return false;
	}
}

bool QFreeRdpClipboard::ownsMode(QClipboard::Mode mode) const  {
	switch (mode) {
		case QClipboard::Clipboard:
		case QClipboard::Selection:
			return true;
		default:
			return false;
		}}

void QFreeRdpClipboard::registerPeer(QFreerdpPeerClipboard* peer) {
	mPeers.push_back(peer);
}

void QFreeRdpClipboard::unregisterPeer(QFreerdpPeerClipboard* peer) {
	mPeers.removeAll(peer);
}
