#include "driverdetaildialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>

static QString hex(ULONG64 v)
{
    return QString("0x%1").arg(v, 16, 16, QLatin1Char('0'));
}

static const char* majorFunctionName(ULONG mf)
{
    switch (mf)
    {
    case 0x00: return "IRP_MJ_CREATE";
    case 0x01: return "IRP_MJ_CREATE_NAMED_PIPE";
    case 0x02: return "IRP_MJ_CLOSE";
    case 0x03: return "IRP_MJ_READ";
    case 0x04: return "IRP_MJ_WRITE";
    case 0x05: return "IRP_MJ_QUERY_INFORMATION";
    case 0x06: return "IRP_MJ_SET_INFORMATION";
    case 0x07: return "IRP_MJ_QUERY_EA";
    case 0x08: return "IRP_MJ_SET_EA";
    case 0x09: return "IRP_MJ_FLUSH_BUFFERS";
    case 0x0a: return "IRP_MJ_QUERY_VOLUME_INFORMATION";
    case 0x0b: return "IRP_MJ_SET_VOLUME_INFORMATION";
    case 0x0c: return "IRP_MJ_DIRECTORY_CONTROL";
    case 0x0d: return "IRP_MJ_FILE_SYSTEM_CONTROL";
    case 0x0e: return "IRP_MJ_DEVICE_CONTROL";
    case 0x0f: return "IRP_MJ_INTERNAL_DEVICE_CONTROL";
    case 0x10: return "IRP_MJ_SHUTDOWN";
    case 0x11: return "IRP_MJ_LOCK_CONTROL";
    case 0x12: return "IRP_MJ_CLEANUP";
    case 0x13: return "IRP_MJ_CREATE_MAILSLOT";
    case 0x14: return "IRP_MJ_QUERY_SECURITY";
    case 0x15: return "IRP_MJ_SET_SECURITY";
    case 0x16: return "IRP_MJ_POWER";
    case 0x17: return "IRP_MJ_SYSTEM_CONTROL";
    case 0x18: return "IRP_MJ_DEVICE_CHANGE";
    case 0x19: return "IRP_MJ_QUERY_QUOTA";
    case 0x1a: return "IRP_MJ_SET_QUOTA";
    case 0x1b: return "IRP_MJ_PNP";
    default: return "IRP_MJ_UNKNOWN";
    }
}

static void styleTable(QTableWidget* table)
{
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

DriverDetailDialog::DriverDetailDialog(KernelDevice* device, ULONG64 driverObject,
    const QString& driverName, QWidget* parent)
    : QDialog(parent)
    , m_device(device)
    , m_driverObject(driverObject)
{
    setWindowTitle(QString("Driver Detail - %1").arg(driverName));
    resize(900, 560);

    auto* root = new QVBoxLayout(this);

    auto* info = new QLabel(QString("DriverObject: %1    %2").arg(hex(driverObject), driverName), this);
    root->addWidget(info);

    auto* tabs = new QTabWidget(this);
    root->addWidget(tabs);

    // ---- Devices tab ----
    auto* devPage = new QWidget(tabs);
    auto* devLayout = new QVBoxLayout(devPage);
    auto* devTop = new QHBoxLayout();
    auto* devRefresh = new QPushButton("Refresh", devPage);
    auto* devCount = new QLabel("Count: 0", devPage);
    devTop->addWidget(devRefresh);
    devTop->addWidget(devCount);
    devTop->addStretch();
    devLayout->addLayout(devTop);

    m_devicesTable = new QTableWidget(devPage);
    m_devicesTable->setColumnCount(8);
    m_devicesTable->setHorizontalHeaderLabels({
        "Device Object", "Device Name", "Flags", "Characteristics",
        "Device Type", "Reference Count", "Next Device", "Attached Device" });
    styleTable(m_devicesTable);
    devLayout->addWidget(m_devicesTable);
    tabs->addTab(devPage, "Devices");

    // ---- Dispatch tab ----
    auto* dspPage = new QWidget(tabs);
    auto* dspLayout = new QVBoxLayout(dspPage);
    auto* dspTop = new QHBoxLayout();
    auto* dspRefresh = new QPushButton("Refresh", dspPage);
    auto* dspCount = new QLabel("Count: 0", dspPage);
    dspTop->addWidget(dspRefresh);
    dspTop->addWidget(dspCount);
    dspTop->addStretch();
    dspLayout->addLayout(dspTop);

    m_dispatchTable = new QTableWidget(dspPage);
    m_dispatchTable->setColumnCount(4);
    m_dispatchTable->setHorizontalHeaderLabels({ "Major Function", "Address", "Module", "Path" });
    styleTable(m_dispatchTable);
    dspLayout->addWidget(m_dispatchTable);
    tabs->addTab(dspPage, "IO Dispatch");

    connect(devRefresh, &QPushButton::clicked, this, &DriverDetailDialog::refreshDevices);
    connect(dspRefresh, &QPushButton::clicked, this, &DriverDetailDialog::refreshDispatch);

    devCount->setObjectName("devCount");
    dspCount->setObjectName("dspCount");

    refreshDevices();
    refreshDispatch();
}

void DriverDetailDialog::refreshDevices()
{
    m_devicesTable->setSortingEnabled(false);
    auto devices = m_device->queryDriverDevices(m_driverObject);

    m_devicesTable->setRowCount(0);
    for (auto& d : devices)
    {
        int row = m_devicesTable->rowCount();
        m_devicesTable->insertRow(row);
        m_devicesTable->setItem(row, 0, new QTableWidgetItem(hex(d.deviceObject)));
        m_devicesTable->setItem(row, 1, new QTableWidgetItem(d.deviceName));
        m_devicesTable->setItem(row, 2, new QTableWidgetItem(QString("0x%1").arg(d.flags, 8, 16, QLatin1Char('0'))));
        m_devicesTable->setItem(row, 3, new QTableWidgetItem(QString("0x%1").arg(d.characteristics, 8, 16, QLatin1Char('0'))));
        m_devicesTable->setItem(row, 4, new QTableWidgetItem(QString::number(d.deviceType)));
        m_devicesTable->setItem(row, 5, new QTableWidgetItem(QString::number(d.referenceCount)));
        m_devicesTable->setItem(row, 6, new QTableWidgetItem(d.nextDevice ? hex(d.nextDevice) : "-"));
        m_devicesTable->setItem(row, 7, new QTableWidgetItem(d.attachedDevice ? hex(d.attachedDevice) : "-"));
    }

    if (auto* lbl = findChild<QLabel*>("devCount"))
        lbl->setText(QString("Count: %1").arg(m_devicesTable->rowCount()));

    m_devicesTable->setSortingEnabled(true);
}

void DriverDetailDialog::refreshDispatch()
{
    m_dispatchTable->setSortingEnabled(false);
    auto entries = m_device->queryDriverDispatch(m_driverObject);

    m_dispatchTable->setRowCount(0);
    for (auto& e : entries)
    {
        int row = m_dispatchTable->rowCount();
        m_dispatchTable->insertRow(row);
        m_dispatchTable->setItem(row, 0, new QTableWidgetItem(majorFunctionName(e.majorFunction)));
        m_dispatchTable->setItem(row, 1, new QTableWidgetItem(e.dispatchAddress ? hex(e.dispatchAddress) : "-"));
        m_dispatchTable->setItem(row, 2, new QTableWidgetItem(e.moduleName));
        m_dispatchTable->setItem(row, 3, new QTableWidgetItem(e.modulePath));
    }

    if (auto* lbl = findChild<QLabel*>("dspCount"))
        lbl->setText(QString("Count: %1").arg(m_dispatchTable->rowCount()));

    m_dispatchTable->setSortingEnabled(true);
}
