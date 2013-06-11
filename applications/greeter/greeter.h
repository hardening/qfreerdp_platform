#ifndef __GREATER_H__
#define __GREATER_H__

#include <QtWidgets/QWidget>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

/**
 *
 */
class Greeter : public QWidget
{
	Q_OBJECT

public:
    Greeter(QWidget *parent = 0);
    virtual ~Greeter();

private slots:
	void on_connect_button_clicked();
	void update();

protected:
	QWidget *loadUiFile();

protected:
	QLineEdit *mLoginEntry;
	QLineEdit *mPasswordEntry;
	QLabel	  *mDateLabel;
	QLabel	  *mTimeLabel;
	QTimer	  *mTimer;
	QPushButton *mConnectButton;
};


#endif /* __GREATER_H__ */
