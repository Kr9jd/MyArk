#include "objectproceduredialog.h"
#include "protocol.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>

static QString hex(ULONG64 v)
{
    return QString("0x%1").arg(v, 16, 16, QLatin1Char('0'));
}

static const char* procedureName(ULONG32 id)
{
    switch (id)
    {
    case OBJECT_PROCEDURE_ID_DUMP:          return "DumpProcedure";
    case OBJECT_PROCEDURE_ID_OPEN:          return "OpenProcedure";
    case OBJECT_PROCEDURE_ID_CLOSE:         return "CloseProcedure";
    case OBJECT_PROCEDURE_ID_DELETE:        return "DeleteProcedure";
    case OBJECT_PROCEDURE_ID_PARSE:         return "ParseProcedure";
    case OBJECT_PROCEDURE_ID_SECURITY:      return "SecurityProcedure";
    case OBJECT_PROCEDURE_ID_QUERY_NAME:    return "QueryNameProcedure";
    case OBJECT_PROCEDURE_ID_OKAY_TO_CLOSE: return "OkayToCloseProcedure";
    default: return "Unknown";
    }
}

static void styleTable(QTableWidget* table)
{
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

ObjectProcedureDialog::ObjectProcedureDialog(KernelDevice* device, ULONG64 objectType,
    const QString& objectName, QWidget* parent)
    : QDialog(parent)
    , m_device(device)
    , m_objectType(objectType)
{
    setWindowTitle(QString("Object Type Procedures - %1").arg(objectName));
    resize(900, 400);

    auto* root = new QVBoxLayout(this);

    auto* info = new QLabel(QString("ObjectType: %1    %2").arg(hex(objectType), objectName), this);
    root->addWidget(info);

    auto* top = new QHBoxLayout();
    auto* refreshBtn = new QPushButton("Refresh", this);
    auto* countLabel = new QLabel("Count: 0", this);
    countLabel->setObjectName("countLabel");
    top->addWidget(refreshBtn);
    top->addWidget(countLabel);
    top->addStretch();
    root->addLayout(top);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({ "Procedure", "Address", "Module", "Path" });
    styleTable(m_table);
    root->addWidget(m_table);

    connect(refreshBtn, &QPushButton::clicked, this, &ObjectProcedureDialog::refresh);
    refresh();
}

void ObjectProcedureDialog::refresh()
{
    m_table->setSortingEnabled(false);
    auto entries = m_device->queryObjectTypeProcedures(m_objectType);

    m_table->setRowCount(0);
    for (auto& e : entries)
    {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(procedureName(e.procedureId)));
        m_table->setItem(row, 1, new QTableWidgetItem(e.address ? hex(e.address) : "-"));
        m_table->setItem(row, 2, new QTableWidgetItem(e.moduleName));
        m_table->setItem(row, 3, new QTableWidgetItem(e.modulePath));
    }

    if (auto* lbl = findChild<QLabel*>("countLabel"))
        lbl->setText(QString("Count: %1").arg(m_table->rowCount()));

    m_table->setSortingEnabled(true);
}
