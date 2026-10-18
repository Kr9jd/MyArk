#pragma once

#include <QDialog>
#include "kerneldevice.h"

class QTextEdit;
class QLabel;

class DisassemblyDialog : public QDialog
{
    Q_OBJECT

public:
    DisassemblyDialog(KernelDevice* device, ULONG64 address, const QString& title, QWidget* parent = nullptr);

private slots:
    void refresh();

private:
    KernelDevice* m_device;
    ULONG64       m_address;

    QTextEdit* m_text = nullptr;
    QLabel*       m_infoLabel = nullptr;
};
