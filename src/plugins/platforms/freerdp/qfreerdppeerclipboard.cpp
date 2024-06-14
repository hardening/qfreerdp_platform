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
#include <string.h>

#include <QDebug>
#include <QStringList>
#include <QClipboard>

#include "qfreerdppeerclipboard.h"
#include "qfreerdpclipboard.h"
#include "qfreerdpplatform.h"
#include "qfreerdppeer.h"

QFreerdpPeerClipboard::ClipboardFormats::ClipboardFormats()
{
	reset();
}

void QFreerdpPeerClipboard::ClipboardFormats::reset() {
	canText = false;
	canUnicode = false;
	canHtml = false;;
	canBmp = false;
	canPng = false;
}



QFreerdpPeerClipboard::QFreerdpPeerClipboard(QFreeRdpPeer* parent, HANDLE vcm)
: mParent(parent)
, mClipboard(cliprdr_server_context_new(vcm))
, isLocked(false)
, mCurrentData(nullptr)
, mPendingData(nullptr)
, mHtmlId(0xc002)
, mCurrentRetrieveFormat(RDP_CLIPBOARD_UNKNOWN)
{
	Q_ASSERT(mClipboard);

	mClipboard->custom = this;
	mClipboard->useLongFormatNames = TRUE;
	mClipboard->canLockClipData = FALSE;
	mClipboard->fileClipNoFilePaths = TRUE;
	mClipboard->hasHugeFileSupport = TRUE;

	mClipboard->ClientCapabilities = cliprdr_client_capabilities;
	mClipboard->TempDirectory = cliprdr_temp_directory;
	mClipboard->ClientFormatList = cliprdr_client_format_list;
	mClipboard->ClientFormatListResponse = cliprdr_client_format_list_response;
	mClipboard->ClientFormatDataRequest = cliprdr_client_format_data_request;
	mClipboard->ClientFormatDataResponse = cliprdr_client_format_data_response;
	mClipboard->ClientLockClipboardData = cliprdr_client_lock_clipboard_data;
	mClipboard->ClientUnlockClipboardData = cliprdr_client_unlock_clipboard_data;
}

QFreerdpPeerClipboard::~QFreerdpPeerClipboard() {
	mClipboard->Stop(mClipboard);
	cliprdr_server_context_free(mClipboard);
	mClipboard = nullptr;

	mParent->mPlatform->rdpClipboard()->unregisterPeer(this);
}

bool QFreerdpPeerClipboard::start() {
	if (mClipboard->Start(mClipboard) != CHANNEL_RC_OK) {
		qDebug() << "error starting clipboard";
		return false;
	}

	return true;
}

void QFreerdpPeerClipboard::setClipboardData(QMimeData *data)
{
	CLIPRDR_FORMAT_LIST formatList;
	CLIPRDR_FORMAT formats[10] = { };
	size_t nformats = 0;

	delete mCurrentData;
	mCurrentData = copyMimeData(*data);

	if (data->hasText() || data->hasHtml()) {
		formats[nformats].formatId = CF_TEXT;
		formats[nformats].formatName = NULL;
		nformats++;

		formats[nformats].formatId = CF_UNICODETEXT;
		formats[nformats].formatName = NULL;
		nformats++;
	}

	// do not announce HTML for now until we manage to forge something good from the
	// QT HTML clipboard
#if 0
	if (data->hasHtml()) {
		formats[nformats].formatId = mHtmlId;
		formats[nformats].formatName = (char*)"HTML Format";
		nformats++;
	}
#endif

	formatList.common.msgType = CB_FORMAT_LIST;
	formatList.common.dataLen = 0;
	formatList.formats = formats;
	formatList.numFormats = nformats;

	mClipboard->ServerFormatList(mClipboard, &formatList);
}

QMimeData *QFreerdpPeerClipboard::copyMimeData(const QMimeData &mimedata)
{
	QMimeData* ret = new QMimeData();

	foreach(QString format, mimedata.formats())
	{
		// Retrieving data
		QByteArray data = mimedata.data(format);
		// Checking for custom MIME types
		if(format.startsWith("application/x-qt"))
		{
			// Retrieving true format name
			int indexBegin = format.indexOf('"') + 1;
			int indexEnd = format.indexOf('"', indexBegin);
			format = format.mid(indexBegin, indexEnd - indexBegin);
		}
		ret->setData(format, data);
	}

	return ret;

}

UINT QFreerdpPeerClipboard::cliprdr_client_capabilities(CliprdrServerContext *context, const CLIPRDR_CAPABILITIES *capabilities)
{
	Q_UNUSED(capabilities);
	QStringList features;

	if (context->useLongFormatNames)
		features.append("long format names");
	if (context->streamFileClipEnabled)
		features.append("stream file clip");
	if (context->fileClipNoFilePaths)
		features.append("file clip no file paths");
	if (context->canLockClipData)
		features.append("can lock clip data");
	if (context->hasHugeFileSupport)
		features.append("huge file support");

	qDebug() << "features=" << features.join(',');
	return CHANNEL_RC_OK;
}

UINT QFreerdpPeerClipboard::cliprdr_temp_directory (CliprdrServerContext *context, const CLIPRDR_TEMP_DIRECTORY *temp_directory)
{
	Q_UNUSED(context);
	Q_UNUSED(temp_directory);

	return CHANNEL_RC_OK;
}

UINT QFreerdpPeerClipboard::cliprdr_client_format_list (CliprdrServerContext *context, const CLIPRDR_FORMAT_LIST *format_list) {
	QFreerdpPeerClipboard *peerClipboard = (QFreerdpPeerClipboard *)context->custom;
	QStringList formatStr;

	ClipboardFormats &formats = peerClipboard->mFormats;
	formats.reset();

	for (UINT32 i = 0; i < format_list->numFormats; i++) {
		switch (format_list->formats[i].formatId) {
		case CF_TEXT:
			formats.canText = true;
			formatStr.append("text");
			continue;
		case CF_UNICODETEXT:
			formats.canUnicode = true;
			formatStr.append("unicode");
			continue;
		case CF_DIB:
			formats.canBmp = true;
			formatStr.append("dib");
			continue;
		default:
			break;
		}

		const char *formatName = format_list->formats[i].formatName;
		if (formatName) {
			if (strcmp(formatName, "HTML Format") == 0) {
				formats.canHtml = true;
				formatStr.append("html");
			}
			qDebug() << "short=" << format_list->formats[i].formatId << "long=" << formatName;
		}
	}

	qDebug() << "remote proposed formats=" << formatStr.join(",");

	peerClipboard->mCurrentRetrieveFormat = RDP_CLIPBOARD_UNKNOWN;
	if (formats.canText)
		peerClipboard->mCurrentRetrieveFormat = RDP_CLIPBOARD_TEXT;
	if (formats.canUnicode)
		peerClipboard->mCurrentRetrieveFormat = RDP_CLIPBOARD_UNICODE;
	/*if (formats.canHtml)
		peerClipboard->mCurrentRetrieveFormat = RDP_CLIPBOARD_HTML;*/

	CLIPRDR_FORMAT_DATA_REQUEST req = { };
	req.common.msgType = CB_FORMAT_DATA_REQUEST;
	req.common.msgFlags = CB_RESPONSE_OK;
	req.common.dataLen = 4;

	switch(peerClipboard->mCurrentRetrieveFormat) {
	case RDP_CLIPBOARD_TEXT:
		req.requestedFormatId = CF_TEXT;
		break;
	case RDP_CLIPBOARD_UNICODE:
		req.requestedFormatId = CF_UNICODETEXT;
		break;
	case RDP_CLIPBOARD_HTML:
		req.requestedFormatId = peerClipboard->mHtmlId;
		break;
	case RDP_CLIPBOARD_UNKNOWN:
	default:
		req.common.msgFlags = CB_RESPONSE_OK;
		break;
	}

	return context->ServerFormatDataRequest(context, &req);
}

UINT QFreerdpPeerClipboard::cliprdr_client_format_list_response (CliprdrServerContext *context,
		const CLIPRDR_FORMAT_LIST_RESPONSE *format_list_response)
{
	Q_UNUSED(context);
	Q_UNUSED(format_list_response);

	return CHANNEL_RC_OK;
}

UINT QFreerdpPeerClipboard::cliprdr_client_format_data_request (CliprdrServerContext *context,
		const CLIPRDR_FORMAT_DATA_REQUEST *request)
{
	QFreerdpPeerClipboard *peer = (QFreerdpPeerClipboard *)context->custom;
	CLIPRDR_FORMAT_DATA_RESPONSE response;
	QByteArray payload;

	response.common.msgType = CB_FORMAT_DATA_RESPONSE;
	response.common.msgFlags = CB_RESPONSE_OK;
	response.common.dataLen = 0;

	if (request->requestedFormatId == CF_TEXT) {
		if (peer->mCurrentData->hasText()) {
			payload = peer->mCurrentData->text().toLatin1();
		} else if (peer->mCurrentData->hasHtml()) {
			payload = peer->mCurrentData->html().toLatin1();
		}
	} else if (request->requestedFormatId == CF_UNICODETEXT) {
		if (peer->mCurrentData->hasText()) {
			auto u16 = peer->mCurrentData->text().toStdU16String();
			payload = QByteArray((const char*)u16.data(), u16.size() * 2);
		} else if (peer->mCurrentData->hasHtml()) {
			auto u16 = peer->mCurrentData->html().toStdU16String();
			payload = QByteArray((const char*)u16.data(), u16.size() * 2);
		}

	} else if (request->requestedFormatId == peer->mHtmlId) {
		QString r = generateHtmlContent(peer->mCurrentData->html());
		payload = r.toLatin1();
	} else {
		qDebug() << "unknown formatId" << request->requestedFormatId;
		response.common.msgFlags = CB_RESPONSE_FAIL;
	}

	if (payload.size()) {
		// Add an UTF-16 null-terminator as both CF_TEXT and CF_UNICODETEXT require it
		payload = payload.append(2, '\0');
		response.requestedFormatData = (const BYTE *)payload.data();
		response.common.dataLen = payload.size();
	}
	return context->ServerFormatDataResponse(context, &response);
}

static bool extractNumber(const QByteArray &ba, const char *prefix, int &res) {
	QString numberStr;

	int startIndex = ba.indexOf(prefix);
	if (startIndex < 0)
		return false;

	startIndex += strlen(prefix);

	int endIndex = ba.indexOf("\r\n", startIndex);
	if (endIndex < 0)
		return false;

	numberStr = ba.mid(startIndex, endIndex - startIndex);
	bool ret;
	res = numberStr.toInt(&ret, 10);
	return ret;
}

QString QFreerdpPeerClipboard::extractHtmlContent(const BYTE *data, size_t dataLen) {
	QByteArray ba((const char*)data, dataLen);
	QString ret;
	bool ok;

	int startIndex, endIndex;
	ok = extractNumber(ba, "StartHTML:", startIndex);
	if (!ok)
		return ret;

	ok = extractNumber(ba, "EndHTML:", endIndex);
	if (!ok)
		return ret;

	return ba.mid(startIndex, endIndex - startIndex);
}

QString QFreerdpPeerClipboard::generateHtmlContent(const QString &content)
{
	QString ret;
	QString headerFormat("Version:0.9\r\n"
	          "StartHTML:%1\r\n"
	          "EndHTML:%2\r\n"
	          "StartFragment:%3\r\n"
	          "EndFragment:%4\r\n");

	QString dummyHeader = headerFormat
			.arg((uint)0, 10, 10, (QChar)'0')
			.arg((uint)0, 10, 10, (QChar)'0')
			.arg((uint)0, 10, 10, (QChar)'0')
			.arg((uint)0, 10, 10, (QChar)'0');
	QString contentEx = "<HTML><BODY><!--StartFragment-->" + content + "<!--EndFragment--></BODY></HTML>";
	int startHtml = dummyHeader.size();
	int startFragment = startHtml + strlen("<HTML><BODY><!--StartFragment-->");
	int endFragment = startFragment + content.size();
	int endHtml = startHtml + contentEx.size();
	return headerFormat
			.arg((uint)startHtml, 10, 10, (QChar)'0')
			.arg((uint)endHtml, 10, 10, (QChar)'0')
			.arg((uint)startFragment, 10, 10, (QChar)'0')
			.arg((uint)endFragment, 10, 10, (QChar)'0')
			+ contentEx;
}

UINT QFreerdpPeerClipboard::cliprdr_client_format_data_response (CliprdrServerContext *context,
		const CLIPRDR_FORMAT_DATA_RESPONSE *format_data_response)
{
	QFreerdpPeerClipboard *peerClipboard = (QFreerdpPeerClipboard *)context->custom;

	QFreeRdpClipboard *clipboard = peerClipboard->mParent->mPlatform->rdpClipboard();

	QMimeData *data = new QMimeData();
	QString str;
	size_t len;

	qDebug() << "cliprdr_client_format_data_response: currentFormat=" << peerClipboard->mCurrentRetrieveFormat
			<< "len=" << format_data_response->common.dataLen;
	switch (peerClipboard->mCurrentRetrieveFormat) {
	case RDP_CLIPBOARD_TEXT:
		if (format_data_response->requestedFormatData) {
			len = strnlen((const char*)format_data_response->requestedFormatData, format_data_response->common.dataLen);
			str = QString::fromLatin1((const char*)format_data_response->requestedFormatData, len);
			data->setText(str);
		} else {
			delete data;
			data = nullptr;
		}
		break;
	case RDP_CLIPBOARD_UNICODE:
		if (format_data_response->requestedFormatData) {
			len = _wcsnlen((const WCHAR*)format_data_response->requestedFormatData, format_data_response->common.dataLen);
			str = QString::fromUtf16((const char16_t*)format_data_response->requestedFormatData, len);
			data->setText(str);
		} else {
			delete data;
			data = nullptr;
		}
		break;
	case RDP_CLIPBOARD_HTML:
		str = extractHtmlContent(format_data_response->requestedFormatData, format_data_response->common.dataLen);
		//qDebug() << "extractedHtml=" << str;
		data->setHtml(str);
		break;
	case RDP_CLIPBOARD_UNKNOWN:
	default:
		delete data;
		data = nullptr;
		break;
	}

	if (data)
		clipboard->updateAvailableData(data);
	clipboard->emitChanged(QClipboard::Clipboard);
	clipboard->emitChanged(QClipboard::Selection);
	return CHANNEL_RC_OK;
}

UINT QFreerdpPeerClipboard::cliprdr_client_lock_clipboard_data(CliprdrServerContext* context, const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
{
	Q_UNUSED(context);
	Q_UNUSED(lockClipboardData);

	//QFreerdpPeerClipboard *rdpPeer = (QFreerdpPeerClipboard *)context->custom;
	return CHANNEL_RC_OK;
}

UINT QFreerdpPeerClipboard::cliprdr_client_unlock_clipboard_data(CliprdrServerContext* context, const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
{
	Q_UNUSED(context);
	Q_UNUSED(unlockClipboardData);

	//QFreerdpPeerClipboard *rdpPeer = (QFreerdpPeerClipboard *)context->custom;
	return CHANNEL_RC_OK;

}

