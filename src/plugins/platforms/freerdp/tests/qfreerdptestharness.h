#pragma once

#include <QObject>

class QFreeRdpTest: public QObject {
    Q_OBJECT

private slots:
    void windowManagerTestValidateGeometry_data();
    void windowManagerTestValidateGeometry();
    void windowManagerTestWindowResize_data();
    void windowManagerTestWindowResize();
};
