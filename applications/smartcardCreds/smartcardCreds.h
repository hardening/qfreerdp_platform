/**
 * Copyright © 2013-2026 David Fort <contact@hardening-consulting.com>
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

#include <QObject>
#include <QThread>
#include <QList>
#include <QTimer>

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>

#include <QProgressDialog>
#include <QSplashScreen>


#include <freerdp/utils/smartcardlogon.h>

class SmartcardCreds;

/** @brief */
class SmartcardLister : public QThread
{
	Q_OBJECT

	friend class SmartcardCreds;
public:
	SmartcardLister();
	virtual ~SmartcardLister();

	/** enumerates the certificates, blocking */
	bool enumerate();
	void run() override;
signals:
	void smartcardListed();

protected:
	SmartcardCertInfo **mInfos;
	size_t mNcerts;
};



/** @brief does the pkinit, blocking (must not run in the main thread with the freerdp QPA) */
bool doPkinit(const QString &ccachePath, SmartcardCertInfo *info, const QString &pin);

/**
 * @brief
 */
class SmartcardCreds : public QMainWindow
{
	Q_OBJECT

public:
	SmartcardCreds(const QString &ccachePath, QIODevice &commandChannel, bool isStdin);
	virtual ~SmartcardCreds();

protected:
	void buildUi();
	void sendResponse(const QString &content, bool cmdChannel);
	void treatCommand(const QString &line, bool cmdChannel);

public slots:
	void onSmartcardStarted();
	void onSmartcardStopped();
	void onSmartcardListed();

	void onRefreshClicked();
	void onCertSelectionChanged(int index);
	void onLogonClicked();
	void onCancelClicked();
	void onPkinitResult(bool result);
	void onCommandChannelReadReadyStdin(int fd);
	void onCommandChannelReadReadySocket(int fd);
	void onProgressTimeout();

protected:
	bool mIsRemote;
	bool mIsStdin;
	QComboBox *mCertCombo;
	QPushButton *mRefreshBtn;
	QLabel *mDetailLabel;
	QLineEdit *mPinEdit;
	QPushButton *mLogonBtn;
	QPushButton *mCancelBtn;
	QLabel *mStatusLabel;

	std::unique_ptr<SmartcardLister> mLister;
	QString mCcachePath;

	QIODevice &mCommandChannel;
	QByteArray mCurrentBuffer;

	QTimer mProgressTimer;
	QProgressDialog mProgressDialog;
};
