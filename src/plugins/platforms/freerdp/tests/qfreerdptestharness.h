#pragma once

#include <QObject>

class QFreeRdpTest: public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {}

    void windowManagerTestValidateGeometry_data();
    void windowManagerTestValidateGeometry();

    void cleanupTestCase() {}
};
