#pragma once

#include <QDialog>
#include "kerneldevice.h"

class QTableWidget;

class ObjectProcedureDialog : public QDialog
{
    Q_OBJECT

public:
    ObjectProcedureDialog(KernelDevice* device, ULONG64 objectType, const QString& objectName, QWidget* parent = nullptr);

private slots:
    void refresh();

private:
    KernelDevice* m_device;
    ULONG64       m_objectType;

    QTableWidget* m_table = nullptr;
};
