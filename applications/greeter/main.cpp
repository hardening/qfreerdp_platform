#include <QtWidgets/QApplication>
#include <QtCore/QResource>
#include "greeter.h"

#include <QDebug>

int main(int argc, char *argv[])
{
	Q_INIT_RESOURCE(greeter);

	QApplication app(argc, argv);
/*	if(!QResource::registerResource("greeter.qrc"))
		qWarning("failed to register resource\n");*/

    Greeter *widget = new Greeter();
    //widget->show();

    return app.exec();
}
