#pragma once

#include <QDialog>
#include "kerneldevice.h"

class QTableWidget;

class DriverDetailDialog : public QDialog
{
    Q_OBJECT

public:
    DriverDetailDialog(KernelDevice* device, ULONG64 driverObject, const QString& driverName, QWidget* parent = nullptr);

private slots:
    void refreshDevices();
    void refreshDispatch();

private:
    KernelDevice* m_device;
    ULONG64       m_driverObject;

    QTableWidget* m_devicesTable = nullptr;
    QTableWidget* m_dispatchTable = nullptr;
};
