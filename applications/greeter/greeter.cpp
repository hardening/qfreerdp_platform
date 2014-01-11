
#include <QtUiTools/QtUiTools>
#include <QtWidgets/QtWidgets>
//#include <QtWidgets/QGuiApplication>
#include "greeter.h"


Greeter::Greeter(QWidget *parent) : QMainWindow(parent), mLoginEntry(0), mPasswordEntry(0),
	mTimer(new QTimer(this))
{
	QWidget *widget = loadUiFile();

	mLoginEntry = findChild<QLineEdit *>("loginEntry");
	mPasswordEntry = findChild<QLineEdit *>("passwordEntry");
	mTimeLabel = findChild<QLabel *>("timeLabel");
	mDateLabel = findChild<QLabel *>("dateLabel");
	mConnectButton = findChild<QPushButton *>("connectButton");

	connect(mTimer, SIGNAL(timeout()), this, SLOT(update()));
	mTimer->start(1000);
	connect(mConnectButton, SIGNAL(clicked()), this, SLOT(on_connect_button_clicked()));

	widget->showFullScreen();
	widget->show();
}

Greeter::~Greeter() {
	mTimer->stop();
	delete mTimer;
}

QWidget *Greeter::loadUiFile() {
    QUiLoader loader;
    QFile file(":/forms/greeter.ui");
    if(!file.open(QFile::ReadOnly))
    	return 0;
    QWidget *ret = loader.load(&file, this);
    file.close();

    return ret;
}

void Greeter::on_connect_button_clicked() {
	if(QGuiApplication::platformName() == "freerdp") {
		qWarning("running through RDP");
	}
}

void Greeter::update() {
	QDateTime now = QDateTime::currentDateTime();
	mDateLabel->setText( now.toString("dd MMM yyyy") );
	mTimeLabel->setText( now.toString("hh:mm") );
}




