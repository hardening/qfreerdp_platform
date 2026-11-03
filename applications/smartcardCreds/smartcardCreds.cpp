/**
 * Copyright © 2026 David Fort <contact@hardening-consulting.com>
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

#include <QTimer>

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QAbstractSocket>

#include <QGuiApplication>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QProgressBar>

#include <QtGui/qpa/qplatformnativeinterface.h>

#include <qfreerdp.h>
#include <stdlib.h>

#include <winpr/wlog.h>
#include <winpr/sspi.h>

#include "smartcardCreds.h"

#define TAG ""

SmartcardLister::SmartcardLister()
: mInfos(nullptr)
, mNcerts(0)
{
}

SmartcardLister::~SmartcardLister()
{
	wait();
	smartcardCertList_Free(mInfos, mNcerts);
}

bool SmartcardLister::enumerate()
{
	rdpSettings *settings = freerdp_settings_new(0);

	smartcardCertList_Free(mInfos, mNcerts);
	mInfos = nullptr;
	mNcerts = 0;

	WLog_DBG(TAG, "listing smartcards");
	BOOL ok = smartcard_enumerateCerts(settings, &mInfos, &mNcerts, FALSE);
	freerdp_settings_free(settings);
	if (!ok)
	{
		WLog_ERR(TAG, "unable to list certs");
		return false;
	}

	WLog_DBG(TAG, "detected %zu smartcards", mNcerts);
	return true;
}

void SmartcardLister::run()
{
	if (enumerate())
		emit smartcardListed();
}

bool doPkinit(const QString &ccachePath, SmartcardCertInfo *mInfo, const QString &mPin)
{
	SEC_WINPR_KERBEROS_SETTINGS kerbSettings = { 0 };
	kerbSettings.cache = strdup(ccachePath.toLatin1().data());
	kerbSettings.pkinitX509Identity = strdup(mInfo->pkinitArgs);

	SEC_WINNT_AUTH_IDENTITY_WINPR identity = { 0 };
	SEC_WINNT_AUTH_IDENTITY_EXW *identityW = &identity.identity;
	size_t sz;

	identityW->Version = SEC_WINNT_AUTH_IDENTITY_VERSION;
	identityW->Length = sizeof(SEC_WINNT_AUTH_IDENTITY_EX);

	identityW->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE | SEC_WINNT_AUTH_IDENTITY_EXTENDED;
	if (mInfo->userHint) {
		identityW->User = ConvertUtf8ToWCharAlloc(mInfo->userHint, &sz);
		identityW->UserLength = sz;
	}

	if (mInfo->domainHint) {
		identityW->Domain = ConvertUtf8ToWCharAlloc(mInfo->domainHint, &sz);
		identityW->DomainLength = sz;
	}

	identityW->Password = ConvertUtf8ToWCharAlloc(mPin.toLatin1().data(), &sz);
	identityW->PasswordLength = sz;

	identity.kerberosSettings = &kerbSettings;

	SecurityFunctionTable* table = InitSecurityInterfaceEx(0);
	SecPkgInfo* pkginfo = nullptr;
	SECURITY_STATUS status = table->QuerySecurityPackageInfoA((SEC_CHAR *)"Kerberos", &pkginfo);

	QString domain(mInfo->domainHint);
	QString principal = QString("%1@%2").arg(mInfo->userHint, domain.toUpper());
	CredHandle creds = { 0 };
	status = table->AcquireCredentialsHandleA(principal.toLatin1().data(), pkginfo->Name, SECPKG_CRED_OUTBOUND, nullptr, &identity, nullptr, nullptr,
	    &creds, nullptr);

	return status == SEC_E_OK;
}

SmartcardCreds::SmartcardCreds(const QString &ccachePath, QIODevice &commandChannel, bool isStdin)
: mIsRemote(false)
, mIsStdin(isStdin)
, mCertCombo(nullptr)
, mRefreshBtn(nullptr)
, mDetailLabel(nullptr)
, mPinEdit(nullptr)
, mLogonBtn(nullptr)
, mCancelBtn(nullptr)
, mStatusLabel(nullptr)
, mCcachePath(ccachePath)
, mCommandChannel(commandChannel)
, mProgressDialog("Waiting for smartcard service", "", 0, 101)
{
	QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();

	QObject *obj = reinterpret_cast<QObject *>(native->nativeResourceForIntegration(QFREERDP_SMARTCARD));
	IFreeRdpSmartcardService *scService = qobject_cast<IFreeRdpSmartcardService *>(obj);
	if (scService)
	{
		connect(scService->asQObject(), SIGNAL(smartcardChannelCreated()),
				this, SLOT(onSmartcardStarted())
		);
		connect(scService->asQObject(), SIGNAL(smartcardChannelRemoved()),
				this, SLOT(onSmartcardStopped())
		);

		if (!scService->start())
			return;

		setenv("PCSCLITE_CSOCK_NAME", scService->pcscdPath().toLatin1().data(), 1);
		mIsRemote = true;
	}
	else
	{
		QTimer::singleShot(50, this, &SmartcardCreds::onSmartcardStarted);
		mIsRemote = false;
	}

	if (isStdin)
		connect(&commandChannel, &QIODevice::channelReadyRead, this, &SmartcardCreds::onCommandChannelReadReadyStdin);
	else
		connect(&commandChannel, &QIODevice::channelReadyRead, this, &SmartcardCreds::onCommandChannelReadReadySocket);
	buildUi();


	connect(&mProgressTimer, &QTimer::timeout, this, &SmartcardCreds::onProgressTimeout);
	mProgressTimer.setInterval(100);
	mProgressTimer.start();

	auto progressBar = new QProgressBar(&mProgressDialog);
	progressBar->setTextVisible(false);
	mProgressDialog.setBar(progressBar);
	mProgressDialog.setCancelButton(nullptr);
	mProgressDialog.setValue(0);

	mProgressDialog.show();
}

SmartcardCreds::~SmartcardCreds()
{
	delete mStatusLabel;
	delete mLogonBtn;
	delete mPinEdit;
	delete mDetailLabel;
	delete mRefreshBtn;
	delete mCertCombo;
}

void SmartcardCreds::onProgressTimeout()
{
	mProgressDialog.setValue((mProgressDialog.value() + 4) % 100);
}

void SmartcardCreds::onSmartcardStarted()
{
	if (!mLister) {
		mProgressDialog.setLabelText("Enumerating smartcards...");
		mProgressDialog.show();

		mLister = std::make_unique<SmartcardLister>();
		connect(mLister.get(), &SmartcardLister::smartcardListed, this, &SmartcardCreds::onSmartcardListed);
		mLister->start();
	}
	//mStatusLabel->setText("Enumerating certificates...");
}

void SmartcardCreds::onSmartcardStopped()
{
}

void SmartcardCreds::onSmartcardListed()
{
	mProgressDialog.hide();

	while (mCertCombo->count())
		mCertCombo->removeItem(0);

	for (size_t i = 0; i < mLister->mNcerts; i++)
	{
		SmartcardCertInfo *info = mLister->mInfos[i];

		mCertCombo->addItem(info->upn);
	}

	onCertSelectionChanged(mLister->mNcerts ? 0 : -1);
}

void SmartcardCreds::onRefreshClicked()
{
	/* the initial enumeration isn't done or still running */
	if (!mLister || mLister->isRunning())
		return;

	mProgressDialog.setLabelText("Enumerating smartcards...");
	mProgressDialog.show();

	/* the enumeration talks to the card through the RDP connection: run it out of the main thread */
	bool ok = false;
	QFreeRdp::runBlocking([&] { ok = mLister->enumerate(); });
	qWarning("refresh: enumeration result=%d certs=%zu", ok, mLister->mNcerts);

	onSmartcardListed();
}

void SmartcardCreds::onCertSelectionChanged(int index)
{
	if (!mLister)
		return;

    if (index < 0 || index >= (int)mLister->mNcerts) {
        mDetailLabel->clear();
        return;
    }

    SmartcardCertInfo *info = mLister->mInfos[index];

    QString details = QString("<b>UPN:</b> %2<br/><b>Reader:</b> %4<br/><b>Subject:</b> %1<br/><b>Issuer:</b> %3<br/><b>Container name:</b> %5")
    		.arg(info->subject, info->upn, info->issuer, QString::fromUtf16((char16_t *)info->reader),
    				QString::fromUtf16((char16_t *)info->containerName));
    mDetailLabel->setText(details);
}

void SmartcardCreds::onLogonClicked()
{
	int i = mCertCombo->currentIndex();
	if (i < 0)
		return;

	mProgressDialog.setLabelText("Authenticating...");
	mProgressDialog.raise();
	mProgressDialog.show();

	SmartcardCertInfo *info = mLister->mInfos[i];

	const QString pin = mPinEdit->text();

	/* the pkinit talks to the smartcard through the RDP connection, which the main
	 * thread services: run it in a worker while the main thread keeps pumping events */
	bool ok = false;
	QFreeRdp::runBlocking([&] { ok = doPkinit(mCcachePath, info, pin); });

	onPkinitResult(ok);
}

void SmartcardCreds::onCancelClicked()
{
	sendResponse("CANCELED", !mIsStdin);
	QApplication::quit();

}


void SmartcardCreds::onPkinitResult(bool result)
{
	mProgressDialog.hide();

	qWarning("onPkinitResult(result=%d)", result);

	if (!result) {
		mStatusLabel->setText("Error during authentication");
	}
	else {
		mStatusLabel->setText("Authenticated");
		sendResponse("SUCCESS", !mIsStdin);
		QApplication::quit();
	}
}


void SmartcardCreds::onCommandChannelReadReadyStdin(int fd)
{
	mCurrentBuffer += mCommandChannel.read(200);

	int idx = mCurrentBuffer.indexOf('\n');
	if (idx > 0)
	{
		treatCommand(mCurrentBuffer.left(idx), false);
		mCurrentBuffer.remove(0, idx + 1);
	}
}


void SmartcardCreds::onCommandChannelReadReadySocket(int fd)
{
	mCurrentBuffer += mCommandChannel.read(200);
	if (mCurrentBuffer.size() < 4)
		return;

	const UINT32 *len = reinterpret_cast<const UINT32 *>(mCurrentBuffer.data());
	if (*len <= mCurrentBuffer.size() - 4)
	{
		treatCommand(mCurrentBuffer.mid(4, *len), true);
		mCurrentBuffer.remove(0, *len + 4);
	}
}

void SmartcardCreds::sendResponse(const QString &content, bool cmdChannel)
{
	if (cmdChannel) {
		BYTE lenBuf[4];
		wStream staticS;
		wStream *s = Stream_StaticInit(&staticS, lenBuf, sizeof(lenBuf));

		Stream_Write_UINT32(s, content.length());
		mCommandChannel.write((const char *)Stream_Buffer(s), 4);
	}
	mCommandChannel.write(content.toLatin1().data());
	if (!cmdChannel)
		mCommandChannel.write("\n");

}

void SmartcardCreds::treatCommand(const QString &line, bool cmdChannel)
{
    if (line.startsWith("PING")) {
    	qDebug("ping command");
        sendResponse("PONG", cmdChannel);
    } else if(line.startsWith("PIN ")) {
    	qDebug("pin command pin=%s", line.last(line.size() - 4).toLatin1().data());
    	mPinEdit->setText(line.last(line.size() - 4));

    } else if(line.startsWith("START")) {
    	qDebug("start command");
    	if (QGuiApplication::platformName() == "freerdp")
    		showFullScreen();
    	else
    		show();
    } else {
    	qFatal(QString("unknown command line=%1").arg(line).toLatin1().data());
    }
}

void SmartcardCreds::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *root = new QVBoxLayout(central);
    root->setSpacing(12);
    root->setContentsMargins(16, 16, 16, 16);

    // ── Certificate group ──────────────────────────────────────────────────
    auto *certGroup  = new QGroupBox("Certificate selection", central);
    auto *certLayout = new QVBoxLayout(certGroup);

    auto *comboRow = new QHBoxLayout;
    mCertCombo   = new QComboBox(certGroup);
    mCertCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mRefreshBtn  = new QPushButton("Refresh", certGroup);
    mRefreshBtn->setFixedWidth(80);
    comboRow->addWidget(mCertCombo);
    comboRow->addWidget(mRefreshBtn);

    mDetailLabel = new QLabel(certGroup);
    mDetailLabel->setWordWrap(true);
    mDetailLabel->setStyleSheet("color: gray; font-size: 11px;");
    mDetailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    certLayout->addLayout(comboRow);
    certLayout->addWidget(mDetailLabel);
    root->addWidget(certGroup);

    // ── Authentication group ───────────────────────────────────────────────
    auto *authGroup  = new QGroupBox("Authentication", central);
    auto *authForm   = new QFormLayout(authGroup);

    mPinEdit = new QLineEdit(authGroup);
    mPinEdit->setEchoMode(QLineEdit::Password);
    mPinEdit->setPlaceholderText("Enter PIN");
    mPinEdit->setMaxLength(32);
    authForm->addRow("PIN:", mPinEdit);
    root->addWidget(authGroup);

    // ── Controls button ───────────────────────────────────────────────────────
    auto *controlForm = new QHBoxLayout(central);
    mLogonBtn = new QPushButton("Log On", central);
    mLogonBtn->setFixedHeight(36);
    controlForm->addWidget(mLogonBtn);

    mCancelBtn = new QPushButton("Cancel", central);
    mCancelBtn->setFixedHeight(36);
    controlForm->addWidget(mCancelBtn);

    authForm->addRow(controlForm);

    // ── Status label ───────────────────────────────────────────────────────
    mStatusLabel = new QLabel(central);
    mStatusLabel->setWordWrap(true);
    mStatusLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(mStatusLabel);

    root->addStretch();

    connect(mCertCombo, &QComboBox::currentIndexChanged, this, &SmartcardCreds::onCertSelectionChanged);
    connect(mRefreshBtn, &QPushButton::clicked, this, &SmartcardCreds::onRefreshClicked);
    connect(mLogonBtn, &QPushButton::clicked, this, &SmartcardCreds::onLogonClicked);
    connect(mPinEdit, &QLineEdit::returnPressed, this, &SmartcardCreds::onLogonClicked);
    connect(mCancelBtn, &QPushButton::clicked, this, &SmartcardCreds::onCancelClicked);
}

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);

	QCommandLineParser parser;
	const QCommandLineOption ccachePathOpt("ccache", "location of the ccache", "ccache");
	const QCommandLineOption socketFdOpt("controlSocket", "file descriptor of the command channel", "controlSocket");
	parser.addOption(ccachePathOpt);
	parser.addOption(socketFdOpt);
	parser.addHelpOption();

	parser.process(app);
	if (!parser.isSet(ccachePathOpt))
		qFatal("need a ccachePath in cmdline arguments");

	QString ccachePath = parser.value(ccachePathOpt);
	int commandFd = fileno(stdin);
	if(parser.isSet(socketFdOpt)) {
		bool ok;
		commandFd = parser.value(socketFdOpt).toInt(&ok, 10);
		if (!ok)
			qFatal("invalid socketfd");
	}

	QAbstractSocket commandChannel(QAbstractSocket::UnknownSocketType, nullptr);
	commandChannel.setSocketDescriptor(commandFd);

	SmartcardCreds gui(ccachePath, commandChannel, !parser.isSet(socketFdOpt));

	return app.exec();
}
