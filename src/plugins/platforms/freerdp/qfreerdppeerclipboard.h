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
#ifndef __QFREERDPPEERCLIPBOARD_H__
#define __QFREERDPPEERCLIPBOARD_H__

#include <freerdp/server/cliprdr.h>

#include <QObject>
#include <QMimeData>

class QFreeRdpPeer;

/** @brief RDP peer clipboard */
class QFreerdpPeerClipboard : public QObject {
	Q_OBJECT

protected:
	/** @brief supported clipboard formats */
	struct ClipboardFormats {
		ClipboardFormats();
		void reset();

		bool canText;
		bool canUnicode;
		bool canHtml;
		bool canBmp;
		bool canPng;
	};

	/** @brief kind of clipboard format */
	enum ClipboardFormatType {
		RDP_CLIPBOARD_UNKNOWN,
		RDP_CLIPBOARD_TEXT,
		RDP_CLIPBOARD_UNICODE,
		RDP_CLIPBOARD_HTML,
	};

public:
	QFreerdpPeerClipboard(QFreeRdpPeer* parent, HANDLE vcm);
	~QFreerdpPeerClipboard();

	bool start();

public slots:
	void setClipboardData(QMimeData *data);

protected:
	static QMimeData *copyMimeData(const QMimeData &data);
	static QString extractHtmlContent(const BYTE *data, size_t dataLen);
	static QString generateHtmlContent(const QString &content);

protected:
	/** clipboard callbacks
	 * @{ */
	static UINT cliprdr_client_capabilities(CliprdrServerContext *cliprdr_context, const CLIPRDR_CAPABILITIES *capabilities);
	static UINT cliprdr_temp_directory (CliprdrServerContext *cliprdr_context, const CLIPRDR_TEMP_DIRECTORY *temp_directory);
	static UINT cliprdr_client_format_list (CliprdrServerContext *cliprdr_context, const CLIPRDR_FORMAT_LIST *format_list);
	static UINT cliprdr_client_format_list_response (CliprdrServerContext *cliprdr_context, const CLIPRDR_FORMAT_LIST_RESPONSE *format_list_response);
	static UINT cliprdr_client_format_data_request (CliprdrServerContext *cliprdr_context, const CLIPRDR_FORMAT_DATA_REQUEST *format_data_request);
	static UINT cliprdr_client_format_data_response (CliprdrServerContext *cliprdr_context, const CLIPRDR_FORMAT_DATA_RESPONSE *format_data_response);
	static UINT cliprdr_client_lock_clipboard_data(CliprdrServerContext* context, const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData);
	static UINT cliprdr_client_unlock_clipboard_data(CliprdrServerContext* context, const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData);
	/** @} */

	QFreeRdpPeer* mParent;
	CliprdrServerContext* mClipboard;

	ClipboardFormats mFormats;
	bool isLocked;
	QMimeData *mCurrentData;
	QMimeData *mPendingData;
	UINT32 mHtmlId;
	ClipboardFormatType mCurrentRetrieveFormat;
};

#endif /* __QFREERDPPEERCLIPBOARD_H__ */
