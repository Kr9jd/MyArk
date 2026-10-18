#include "mainwindow.h"
#include "processservice.h"
#include "driverdetaildialog.h"
#include "objectproceduredialog.h"
#include "disassemblydialog.h"
#include <QTableWidget>
#include <QTreeWidget>
#include <QTabWidget>
#include <QRadioButton>
#include <QHeaderView>
#include <QClipboard>
#include <QKeyEvent>
#include <QEvent>
#include <QMenu>
#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTreeWidgetItem>

static QString hex(ULONG64 v)
{
    return QString("0x%1").arg(v, 16, 16, QLatin1Char('0'));
}

static QString hex4(DWORD v)
{
    return QString("0x%1").arg(v, 4, 16, QLatin1Char('0'));
}

static const char* threadStateName(ULONG state)
{
    switch (state)
    {
    case 0: return "Initialized";
    case 1: return "Ready";
    case 2: return "Running";
    case 3: return "Standby";
    case 4: return "Terminated";
    case 5: return "Waiting";
    case 6: return "Transition";
    case 7: return "DeferredReady";
    default: return "Unknown";
    }
}

static const char* majorFunctionName(ULONG mf)
{
    switch (mf)
    {
    case 0x00: return "CREATE";
    case 0x01: return "CREATE_NAMED_PIPE";
    case 0x02: return "CLOSE";
    case 0x03: return "READ";
    case 0x04: return "WRITE";
    case 0x05: return "QUERY_INFORMATION";
    case 0x06: return "SET_INFORMATION";
    case 0x07: return "QUERY_EA";
    case 0x08: return "SET_EA";
    case 0x09: return "FLUSH_BUFFERS";
    case 0x0a: return "QUERY_VOLUME_INFORMATION";
    case 0x0b: return "SET_VOLUME_INFORMATION";
    case 0x0c: return "DIRECTORY_CONTROL";
    case 0x0d: return "FILE_SYSTEM_CONTROL";
    case 0x0e: return "DEVICE_CONTROL";
    case 0x0f: return "INTERNAL_DEVICE_CONTROL";
    case 0x10: return "SHUTDOWN";
    case 0x11: return "LOCK_CONTROL";
    case 0x12: return "CLEANUP";
    case 0x13: return "CREATE_MAILSLOT";
    case 0x14: return "QUERY_SECURITY";
    case 0x15: return "SET_SECURITY";
    case 0x16: return "POWER";
    case 0x17: return "SYSTEM_CONTROL";
    case 0x18: return "DEVICE_CHANGE";
    case 0x19: return "QUERY_QUOTA";
    case 0x1a: return "SET_QUOTA";
    case 0x1b: return "PNP";
    default: return "UNKNOWN";
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowClass())
{
    ui->setupUi(this);
    setupTables();
    setupRefreshMenus();

    connect(ui->driversTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onDriverDoubleClicked);
    connect(ui->objectTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onObjectProcedureDoubleClicked);

    connect(ui->mainTabs, &QTabWidget::currentChanged, this, &MainWindow::onMainTabChanged);
    connect(ui->objectTab, &QTabWidget::currentChanged, this, &MainWindow::onObjectTabChanged);

    connect(ui->radioMmUnloaded, &QRadioButton::toggled, this, &MainWindow::onUnloadedSourceChanged);
    connect(ui->radioPiDDB, &QRadioButton::toggled, this, &MainWindow::onUnloadedSourceChanged);
    connect(ui->radioCiHash, &QRadioButton::toggled, this, &MainWindow::onUnloadedSourceChanged);


    if (m_device.connectDevice())
        logInfo("[INFO] Driver connected.");
    else
        logInfo("[WARN] Driver not connected. Load the driver and restart MyArk.");
}

MainWindow::~MainWindow()
{
    m_device.disconnectDevice();
    delete ui;
}

// Append a line to the log view.
void MainWindow::logInfo(const QString& text)
{
    ui->logEdit->appendPlainText(text);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() == QEvent::KeyPress)
    {
        auto* key = static_cast<QKeyEvent*>(ev);
        if (key->matches(QKeySequence::Copy))
        {
            if (auto* table = qobject_cast<QTableWidget*>(obj))
            {
                copyTableSelection(table);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::copyTableSelection(QTableWidget* table)
{
    QList<QTableWidgetItem*> sel = table->selectedItems();
    if (sel.isEmpty())
        return;

    int minRow = INT_MAX, maxRow = -1, minCol = INT_MAX, maxCol = -1;
    QHash<int, QHash<int, QString>> grid;
    for (auto* it : sel)
    {
        grid[it->row()][it->column()] = it->text();
        minRow = qMin(minRow, it->row());
        maxRow = qMax(maxRow, it->row());
        minCol = qMin(minCol, it->column());
        maxCol = qMax(maxCol, it->column());
    }

    QStringList lines;
    for (int r = minRow; r <= maxRow; r++)
    {
        QStringList cells;
        for (int c = minCol; c <= maxCol; c++)
            cells << grid.value(r).value(c, QString());
        lines << cells.join('\t');
    }
    QApplication::clipboard()->setText(lines.join('\n'));
}

// Log a warning when the driver protocol version does not match.
void MainWindow::checkProtocolMismatch()
{
    if (m_device.lastProtocolMismatch())
        logInfo("[ERROR] Driver protocol version mismatch - reload the new KMDFDriver1.sys!");
}

// Configure columns and styling for every table and tree.
void MainWindow::setupTables()
{
    auto styleTable = [this](QTableWidget* t) {
        t->setShowGrid(false);
        t->verticalHeader()->setVisible(false);
        t->verticalHeader()->setDefaultSectionSize(26);
        t->horizontalHeader()->setHighlightSections(false);
        t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        t->horizontalHeader()->setStretchLastSection(true);
        t->horizontalHeader()->setMinimumSectionSize(40);
        t->installEventFilter(this);
    };
    auto styleTree = [](QTreeWidget* t) {
        t->setIndentation(16);
        t->header()->setHighlightSections(false);
        t->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        t->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        t->header()->setStretchLastSection(true);
        t->header()->setMinimumSectionSize(40);
    };

    ui->processTable->setColumnCount(5);
    ui->processTable->setHorizontalHeaderLabels({ "PID", "Parent PID", "Name", "Threads", "Path" });
    styleTable(ui->processTable);

    ui->serviceTable->setColumnCount(5);
    ui->serviceTable->setHorizontalHeaderLabels({ "Name", "Display Name", "Type", "Status", "Start Type" });
    styleTable(ui->serviceTable);

    ui->obCallbacksTable->setColumnCount(6);
    ui->obCallbacksTable->setHorizontalHeaderLabels({ "PreOperation", "PostOperation", "Operations", "Type", "Module", "Path" });
    styleTable(ui->obCallbacksTable);

    ui->processCallbacksTable->setColumnCount(4);
    ui->processCallbacksTable->setHorizontalHeaderLabels({ "Type", "Callback", "Module", "Path" });
    styleTable(ui->processCallbacksTable);

    ui->regCallbacksTable->setColumnCount(4);
    ui->regCallbacksTable->setHorizontalHeaderLabels({ "Callback", "Altitude", "Module", "Path" });
    styleTable(ui->regCallbacksTable);

    ui->bugCheckCallbacksTable->setColumnCount(4);
    ui->bugCheckCallbacksTable->setHorizontalHeaderLabels({ "Callback", "Reason", "Module", "Path" });
    styleTable(ui->bugCheckCallbacksTable);

    ui->shutdownCallbacksTable->setColumnCount(3);
    ui->shutdownCallbacksTable->setHorizontalHeaderLabels({ "Callback", "Module", "Path" });
    styleTable(ui->shutdownCallbacksTable);

    ui->bugCheckRegistersTable->setColumnCount(4);
    ui->bugCheckRegistersTable->setHorizontalHeaderLabels({ "Callback", "Component", "Module", "Path" });
    styleTable(ui->bugCheckRegistersTable);

    ui->notifyShutdownTable->setColumnCount(3);
    ui->notifyShutdownTable->setHorizontalHeaderLabels({ "Callback", "Module", "Path" });
    styleTable(ui->notifyShutdownTable);

    ui->timersTable->setColumnCount(5);
    ui->timersTable->setHorizontalHeaderLabels({ "Type", "Timer", "Routine", "Module", "Path" });
    styleTable(ui->timersTable);

    ui->ssdtTable->setColumnCount(3);
    ui->ssdtTable->setHorizontalHeaderLabels({ "Index", "Function", "Address" });
    styleTable(ui->ssdtTable);

    ui->shadowSsdtTable->setColumnCount(3);
    ui->shadowSsdtTable->setHorizontalHeaderLabels({ "Index", "Function", "Address" });
    styleTable(ui->shadowSsdtTable);

    ui->idtTree->setColumnCount(5);
    ui->idtTree->setHeaderLabels({ "Index", "Address", "DPL", "Selector", "ISR" });
    styleTree(ui->idtTree);

    ui->systemThreadsTable->setColumnCount(7);
    ui->systemThreadsTable->setHorizontalHeaderLabels({ "TID", "Start Address", "Priority", "Base", "State", "Module", "Path" });
    styleTable(ui->systemThreadsTable);

    ui->workThreadsTable->setColumnCount(7);
    ui->workThreadsTable->setHorizontalHeaderLabels({ "Pool", "TID", "Start Address", "Priority", "State", "Module", "Path" });
    styleTable(ui->workThreadsTable);

    ui->wfpFiltersTable->setColumnCount(6);
    ui->wfpFiltersTable->setHorizontalHeaderLabels({ "Filter ID", "Name", "Description", "Action", "Layer Key", "Callout Key" });
    styleTable(ui->wfpFiltersTable);

    ui->wfpCalloutsTable->setColumnCount(6);
    ui->wfpCalloutsTable->setHorizontalHeaderLabels({ "Callout ID", "Classify", "Notify", "FlowDelete", "Module", "Path" });
    styleTable(ui->wfpCalloutsTable);

    ui->driversTable->setColumnCount(6);
    ui->driversTable->setHorizontalHeaderLabels({ "Driver", "Base Address", "Size", "Object Address", "Object Name", "Path" });
    styleTable(ui->driversTable);
    ui->unloadedDriversTable->setColumnCount(3);
    ui->unloadedDriversTable->setHorizontalHeaderLabels({ "Driver Name", "Start", "End" });
    styleTable(ui->unloadedDriversTable);

    ui->filtersTree->setColumnCount(4);
    ui->filtersTree->setHeaderLabels({ "Field", "PreOperation", "PostOperation", "Value" });
    styleTree(ui->filtersTree);

    ui->objectTable->setColumnCount(4);
    ui->objectTable->setHorizontalHeaderLabels({ "Index", "Object Name", "Handle Count", "Object Count" });
    styleTable(ui->objectTable);
}

// Install the right-click refresh/disassemble/open menu on each view.
void MainWindow::setupRefreshMenus()
{
    auto bind = [this](QWidget* widget, void (MainWindow::* slot)())
    {
        if (!widget)
            return;
        widget->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(widget, &QWidget::customContextMenuRequested, this,
            [this, widget, slot](const QPoint& pos)
            {
                QMenu menu(widget);
                QAction* refreshAction = menu.addAction("Refresh");
                QAction* openAction = menu.addAction("Open in Explorer");
                QAction* disasmAction = menu.addAction("Disassemble");
                QAction* chosen = menu.exec(widget->mapToGlobal(pos));
                if (chosen == refreshAction)
                    (this->*slot)();
                else if (chosen == openAction)
                    openRowInExplorer(widget, pos);
                else if (chosen == disasmAction)
                    disassembleRow(widget, pos);
            });
    };

    bind(ui->processTable,          &MainWindow::onRefreshProcess);
    bind(ui->serviceTable,          &MainWindow::onRefreshService);
    bind(ui->obCallbacksTable,      &MainWindow::onRefreshObCallbacks);
    bind(ui->processCallbacksTable, &MainWindow::onRefreshProcCallbacks);
    bind(ui->regCallbacksTable,     &MainWindow::onRefreshRegCallbacks);
    bind(ui->bugCheckCallbacksTable, &MainWindow::onRefreshBugCheckCallbacks);
    bind(ui->shutdownCallbacksTable, &MainWindow::onRefreshShutdownCallbacks);
    bind(ui->bugCheckRegistersTable, &MainWindow::onRefreshBugCheckRegisters);
    bind(ui->notifyShutdownTable,   &MainWindow::onRefreshNotifyShutdown);
    bind(ui->timersTable,           &MainWindow::onRefreshTimers);
    bind(ui->ssdtTable,             &MainWindow::onRefreshSsdt);
    bind(ui->shadowSsdtTable,       &MainWindow::onRefreshShadowSsdt);
    bind(ui->idtTree,               &MainWindow::onRefreshIdt);
    bind(ui->systemThreadsTable,    &MainWindow::onRefreshSystemThreads);
    bind(ui->workThreadsTable,      &MainWindow::onRefreshWorkThreads);
    bind(ui->wfpFiltersTable,       &MainWindow::onRefreshWfpFilters);
    bind(ui->wfpCalloutsTable,      &MainWindow::onRefreshWfpCallouts);
    bind(ui->driversTable,          &MainWindow::onRefreshDrivers);
    bind(ui->unloadedDriversTable,  &MainWindow::onRefreshUnloadedDrivers);
    bind(ui->filtersTree,           &MainWindow::onRefreshFilters);
    bind(ui->objectTable,           &MainWindow::onRefreshObject);
}

// Open the file referenced by the selected row in Explorer.
void MainWindow::openRowInExplorer(QWidget* view, const QPoint& pos)
{
    QString path;

    if (auto* table = qobject_cast<QTableWidget*>(view))
    {
        int row = table->rowAt(pos.y());
        if (row < 0)
            row = table->currentRow();
        if (row < 0)
            return;
        for (int col = table->columnCount() - 1; col >= 0; --col)
        {
            auto* item = table->item(row, col);
            if (!item)
                continue;
            QString text = item->text().trimmed();
            if (isExistingFile(text))
            {
                path = text;
                break;
            }
        }
    }
    else if (auto* tree = qobject_cast<QTreeWidget*>(view))
    {
        auto* item = tree->itemAt(pos);
        if (!item)
            item = tree->currentItem();
        if (!item)
            return;
        // A path may be in the "Value" column or a child node; walk this node and its children
        path = findPathInTreeItem(item);
    }

    if (path.isEmpty())
    {
        logInfo("[WARN] No file path found for the selected row.");
        return;
    }

    QString native = QDir::toNativeSeparators(path);
    QProcess::startDetached("explorer.exe", { "/select,", native });
}

// Disassemble the code address found in the selected row.
void MainWindow::disassembleRow(QWidget* view, const QPoint& pos)
{
    ULONG64 address = 0;
    QString label;

    if (auto* table = qobject_cast<QTableWidget*>(view))
    {
        int row = table->rowAt(pos.y());
        if (row < 0)
            row = table->currentRow();
        if (row < 0)
            return;
        for (int col = 0; col < table->columnCount(); ++col)
        {
            auto* item = table->item(row, col);
            if (!item)
                continue;
            ULONG64 val = 0;
            if (parseCodeAddress(item->text(), val))
            {
                address = val;
                label = item->text();
                break;
            }
        }
    }
    else if (auto* tree = qobject_cast<QTreeWidget*>(view))
    {
        auto* item = tree->itemAt(pos);
        if (!item)
            item = tree->currentItem();
        if (item)
        {
            for (int col = 0; col < item->columnCount(); ++col)
            {
                ULONG64 val = 0;
                if (parseCodeAddress(item->text(col), val))
                {
                    address = val;
                    label = item->text(col);
                    break;
                }
            }
        }
    }

    if (!address)
    {
        logInfo("[WARN] No code address found for the selected row.");
        return;
    }

    auto* dlg = new DisassemblyDialog(&m_device, address, label, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

// Parse a canonical kernel code address from text.
bool MainWindow::parseCodeAddress(const QString& text, ULONG64& out)
{
    QString t = text.trimmed();
    if (!t.startsWith("0x", Qt::CaseInsensitive))
        return false;
    bool ok = false;
    ULONG64 val = t.mid(2).toULongLong(&ok, 16);
    if (!ok)
        return false;
    // Canonical kernel address (top 16 bits are 0xFFFF); excludes small/user addresses
    if ((val >> 48) != 0xFFFF)
        return false;
    out = val;
    return true;
}

// Return true if text is an absolute path to an existing file.
bool MainWindow::isExistingFile(const QString& text)
{
    if (text.isEmpty())
        return false;
    // Absolute path such as C:\... or \\...
    if (text.length() < 3)
        return false;
    if (!( (text[0].isLetter() && text[1] == ':' && (text[2] == '\\' || text[2] == '/'))
        || text.startsWith("\\\\") ))
        return false;
    return QFileInfo::exists(text);
}

// Find an existing file path in a tree item or its children.
QString MainWindow::findPathInTreeItem(QTreeWidgetItem* item)
{
    for (int col = item->columnCount() - 1; col >= 0; --col)
    {
        QString text = item->text(col).trimmed();
        if (isExistingFile(text))
            return text;
    }
    for (int i = 0; i < item->childCount(); ++i)
    {
        QString found = findPathInTreeItem(item->child(i));
        if (!found.isEmpty())
            return found;
    }
    return QString();
}

void MainWindow::onRefreshProcess()
{
    ui->processTable->setSortingEnabled(false);
    QString filter = ui->processFilterEdit->text();
    auto processes = EnumProcesses();

    ui->processTable->setRowCount(0);
    for (auto& p : processes)
    {
        if (!filter.isEmpty() && !p.name.contains(filter, Qt::CaseInsensitive))
            continue;

        int row = ui->processTable->rowCount();
        ui->processTable->insertRow(row);
        ui->processTable->setItem(row, 0, new QTableWidgetItem(QString::number(p.pid)));
        ui->processTable->setItem(row, 1, new QTableWidgetItem(QString::number(p.parentPid)));
        ui->processTable->setItem(row, 2, new QTableWidgetItem(p.name));
        ui->processTable->setItem(row, 3, new QTableWidgetItem(QString::number(p.threadCount)));
        ui->processTable->setItem(row, 4, new QTableWidgetItem(p.path));
    }
    ui->processCountLabel->setText(QString("Count: %1").arg(ui->processTable->rowCount()));
    ui->processTable->setSortingEnabled(true);
}

void MainWindow::onRefreshService()
{
    ui->serviceTable->setSortingEnabled(false);
    auto services = EnumServices();

    ui->serviceTable->setRowCount(0);
    for (auto& s : services)
    {
        int row = ui->serviceTable->rowCount();
        ui->serviceTable->insertRow(row);
        ui->serviceTable->setItem(row, 0, new QTableWidgetItem(s.name));
        ui->serviceTable->setItem(row, 1, new QTableWidgetItem(s.displayName));
        ui->serviceTable->setItem(row, 2, new QTableWidgetItem(
            (s.serviceType & SERVICE_KERNEL_DRIVER) ? "Kernel Driver" :
            (s.serviceType & SERVICE_FILE_SYSTEM_DRIVER) ? "FS Driver" :
            (s.serviceType & SERVICE_WIN32_OWN_PROCESS) ? "Own Process" :
            (s.serviceType & SERVICE_WIN32_SHARE_PROCESS) ? "Shared Process" : "Unknown"));
        ui->serviceTable->setItem(row, 3, new QTableWidgetItem(
            s.currentState == SERVICE_RUNNING ? "Running" :
            s.currentState == SERVICE_STOPPED ? "Stopped" : "Other"));
        ui->serviceTable->setItem(row, 4, new QTableWidgetItem(
            s.startType == SERVICE_AUTO_START ? "Auto" :
            s.startType == SERVICE_BOOT_START ? "Boot" :
            s.startType == SERVICE_DEMAND_START ? "Manual" :
            s.startType == SERVICE_DISABLED ? "Disabled" : "System"));
    }
    ui->serviceCountLabel->setText(QString("Count: %1").arg(ui->serviceTable->rowCount()));
    ui->serviceTable->setSortingEnabled(true);
}

void MainWindow::onRefreshObCallbacks()
{
    ui->obCallbacksTable->setSortingEnabled(false);
    auto callbacks = m_device.queryObCallbacks();

    ui->obCallbacksTable->setRowCount(0);
    for (auto& cb : callbacks)
    {
        int row = ui->obCallbacksTable->rowCount();
        ui->obCallbacksTable->insertRow(row);
        ui->obCallbacksTable->setItem(row, 0, new QTableWidgetItem(cb.preOperation ? hex(cb.preOperation) : "-"));
        ui->obCallbacksTable->setItem(row, 1, new QTableWidgetItem(cb.postOperation ? hex(cb.postOperation) : "-"));
        ui->obCallbacksTable->setItem(row, 2, new QTableWidgetItem(
            (cb.operations == 1) ? "CREATE" :
            (cb.operations == 2) ? "DUPLICATE" :
            (cb.operations == 3) ? "CREATE|DUPLICATE" : "Unknown"));
        ui->obCallbacksTable->setItem(row, 3, new QTableWidgetItem(cb.type == 0 ? "Process" : "Thread"));
        ui->obCallbacksTable->setItem(row, 4, new QTableWidgetItem(cb.moduleName));
        ui->obCallbacksTable->setItem(row, 5, new QTableWidgetItem(cb.modulePath));
    }
    ui->obCountLabel->setText(QString("Count: %1").arg(ui->obCallbacksTable->rowCount()));
    checkProtocolMismatch();
    ui->obCallbacksTable->setSortingEnabled(true);
}

void MainWindow::onRefreshProcCallbacks()
{
    ui->processCallbacksTable->setSortingEnabled(false);
    auto callbacks = m_device.queryProcessCallbacks();

    ui->processCallbacksTable->setRowCount(0);
    for (auto& cb : callbacks)
    {
        int row = ui->processCallbacksTable->rowCount();
        ui->processCallbacksTable->insertRow(row);
        ui->processCallbacksTable->setItem(row, 0, new QTableWidgetItem(
            cb.type == 0 ? "Process" : cb.type == 1 ? "Thread" : "Image"));
        ui->processCallbacksTable->setItem(row, 1, new QTableWidgetItem(hex(cb.callback)));
        ui->processCallbacksTable->setItem(row, 2, new QTableWidgetItem(cb.moduleName));
        ui->processCallbacksTable->setItem(row, 3, new QTableWidgetItem(cb.modulePath));
    }
    ui->procCbCountLabel->setText(QString("Count: %1").arg(ui->processCallbacksTable->rowCount()));
    checkProtocolMismatch();
    ui->processCallbacksTable->setSortingEnabled(true);
}

void MainWindow::onRefreshRegCallbacks()
{
    ui->regCallbacksTable->setSortingEnabled(false);
    auto callbacks = m_device.queryRegCallbacks();

    ui->regCallbacksTable->setRowCount(0);
    for (auto& cb : callbacks)
    {
        int row = ui->regCallbacksTable->rowCount();
        ui->regCallbacksTable->insertRow(row);
        ui->regCallbacksTable->setItem(row, 0, new QTableWidgetItem(hex(cb.callback)));
        ui->regCallbacksTable->setItem(row, 1, new QTableWidgetItem(cb.altitude));
        ui->regCallbacksTable->setItem(row, 2, new QTableWidgetItem(cb.moduleName));
        ui->regCallbacksTable->setItem(row, 3, new QTableWidgetItem(cb.modulePath));
    }
    ui->regCbCountLabel->setText(QString("Count: %1").arg(ui->regCallbacksTable->rowCount()));
    checkProtocolMismatch();
    ui->regCallbacksTable->setSortingEnabled(true);
}

void MainWindow::onRefreshBugCheckCallbacks()
{
    ui->bugCheckCallbacksTable->setSortingEnabled(false);
    auto callbacks = m_device.queryBugCheckCallbacks();

    ui->bugCheckCallbacksTable->setRowCount(0);
    for (auto& cb : callbacks)
    {
        int row = ui->bugCheckCallbacksTable->rowCount();
        ui->bugCheckCallbacksTable->insertRow(row);
        ui->bugCheckCallbacksTable->setItem(row, 0, new QTableWidgetItem(hex(cb.callback)));
        ui->bugCheckCallbacksTable->setItem(row, 1, new QTableWidgetItem(QString::number(cb.reason)));
        ui->bugCheckCallbacksTable->setItem(row, 2, new QTableWidgetItem(cb.moduleName));
        ui->bugCheckCallbacksTable->setItem(row, 3, new QTableWidgetItem(cb.modulePath));
    }
    ui->bugCheckCbCountLabel->setText(QString("Count: %1").arg(ui->bugCheckCallbacksTable->rowCount()));
    checkProtocolMismatch();
    ui->bugCheckCallbacksTable->setSortingEnabled(true);
}

void MainWindow::onRefreshShutdownCallbacks()
{
    ui->shutdownCallbacksTable->setSortingEnabled(false);
    auto callbacks = m_device.queryShutdownCallbacks();

    ui->shutdownCallbacksTable->setRowCount(0);
    for (auto& cb : callbacks)
    {
        int row = ui->shutdownCallbacksTable->rowCount();
        ui->shutdownCallbacksTable->insertRow(row);
        ui->shutdownCallbacksTable->setItem(row, 0, new QTableWidgetItem(hex(cb.callback)));
        ui->shutdownCallbacksTable->setItem(row, 1, new QTableWidgetItem(cb.moduleName));
        ui->shutdownCallbacksTable->setItem(row, 2, new QTableWidgetItem(cb.modulePath));
    }
    ui->shutdownCbCountLabel->setText(QString("Count: %1").arg(ui->shutdownCallbacksTable->rowCount()));
    checkProtocolMismatch();
    ui->shutdownCallbacksTable->setSortingEnabled(true);
}

void MainWindow::onRefreshBugCheckRegisters()
{
    ui->bugCheckRegistersTable->setSortingEnabled(false);
    auto callbacks = m_device.queryBugCheckRegisters();

    ui->bugCheckRegistersTable->setRowCount(0);
    for (auto& cb : callbacks)
    {
        int row = ui->bugCheckRegistersTable->rowCount();
        ui->bugCheckRegistersTable->insertRow(row);
        ui->bugCheckRegistersTable->setItem(row, 0, new QTableWidgetItem(hex(cb.callback)));
        ui->bugCheckRegistersTable->setItem(row, 1, new QTableWidgetItem(cb.component));
        ui->bugCheckRegistersTable->setItem(row, 2, new QTableWidgetItem(cb.moduleName));
        ui->bugCheckRegistersTable->setItem(row, 3, new QTableWidgetItem(cb.modulePath));
    }
    ui->bugCheckRegCountLabel->setText(QString("Count: %1").arg(ui->bugCheckRegistersTable->rowCount()));
    checkProtocolMismatch();
    ui->bugCheckRegistersTable->setSortingEnabled(true);
}

void MainWindow::onRefreshNotifyShutdown()
{
    ui->notifyShutdownTable->setSortingEnabled(false);
    auto callbacks = m_device.queryNotifyShutdown();

    ui->notifyShutdownTable->setRowCount(0);
    for (auto& cb : callbacks)
    {
        int row = ui->notifyShutdownTable->rowCount();
        ui->notifyShutdownTable->insertRow(row);
        ui->notifyShutdownTable->setItem(row, 0, new QTableWidgetItem(hex(cb.callback)));
        ui->notifyShutdownTable->setItem(row, 1, new QTableWidgetItem(cb.moduleName));
        ui->notifyShutdownTable->setItem(row, 2, new QTableWidgetItem(cb.modulePath));
    }
    ui->notifyShutdownCountLabel->setText(QString("Count: %1").arg(ui->notifyShutdownTable->rowCount()));
    checkProtocolMismatch();
    ui->notifyShutdownTable->setSortingEnabled(true);
}

void MainWindow::onRefreshTimers()
{
    ui->timersTable->setSortingEnabled(false);
    auto timers = m_device.queryTimers();

    ui->timersTable->setRowCount(0);
    for (auto& t : timers)
    {
        int row = ui->timersTable->rowCount();
        ui->timersTable->insertRow(row);
        ui->timersTable->setItem(row, 0, new QTableWidgetItem(t.type == 0 ? "DPC" : "IO"));
        ui->timersTable->setItem(row, 1, new QTableWidgetItem(hex(t.timerAddress)));
        ui->timersTable->setItem(row, 2, new QTableWidgetItem(hex(t.routineAddress)));
        ui->timersTable->setItem(row, 3, new QTableWidgetItem(t.moduleName));
        ui->timersTable->setItem(row, 4, new QTableWidgetItem(t.modulePath));
    }
    ui->timersCountLabel->setText(QString("Count: %1").arg(ui->timersTable->rowCount()));
    checkProtocolMismatch();
    ui->timersTable->setSortingEnabled(true);
}

void MainWindow::onRefreshSsdt()
{
    ui->ssdtTable->setSortingEnabled(false);
    auto ssdt = m_device.querySsdt();

    ui->ssdtTable->setRowCount(0);
    for (auto& s : ssdt)
    {
        int row = ui->ssdtTable->rowCount();
        ui->ssdtTable->insertRow(row);
        ui->ssdtTable->setItem(row, 0, new QTableWidgetItem(QString::number(s.index)));
        ui->ssdtTable->setItem(row, 1, new QTableWidgetItem(s.functionName.isEmpty() ? "-" : s.functionName));
        ui->ssdtTable->setItem(row, 2, new QTableWidgetItem(hex(s.address)));
    }
    ui->ssdtCountLabel->setText(QString("Count: %1").arg(ui->ssdtTable->rowCount()));
    checkProtocolMismatch();
    ui->ssdtTable->setSortingEnabled(true);
}

void MainWindow::onRefreshShadowSsdt()
{
    ui->shadowSsdtTable->setSortingEnabled(false);
    auto ssdt = m_device.queryShadowSsdt();

    ui->shadowSsdtTable->setRowCount(0);
    for (auto& s : ssdt)
    {
        int row = ui->shadowSsdtTable->rowCount();
        ui->shadowSsdtTable->insertRow(row);
        ui->shadowSsdtTable->setItem(row, 0, new QTableWidgetItem(QString::number(s.index)));
        ui->shadowSsdtTable->setItem(row, 1, new QTableWidgetItem(s.functionName.isEmpty() ? "-" : s.functionName));
        ui->shadowSsdtTable->setItem(row, 2, new QTableWidgetItem(hex(s.address)));
    }
    ui->shadowSsdtCountLabel->setText(QString("Count: %1").arg(ui->shadowSsdtTable->rowCount()));
    checkProtocolMismatch();
    ui->shadowSsdtTable->setSortingEnabled(true);
}

void MainWindow::onRefreshIdt()
{
    auto idt = m_device.queryIdt();

    ui->idtTree->clear();
    ULONG curCpu = 0xFFFFFFFF;
    QTreeWidgetItem* cpuNode = nullptr;
    for (auto& e : idt)
    {
        if (e.cpu != curCpu)
        {
            curCpu = e.cpu;
            cpuNode = new QTreeWidgetItem(ui->idtTree);
            cpuNode->setText(0, QString("CPU %1").arg(curCpu));
            cpuNode->setExpanded(true);
        }

        auto* item = new QTreeWidgetItem(cpuNode);
        item->setText(0, QString("0x%1").arg(e.index, 2, 16, QLatin1Char('0')));
        item->setText(1, hex(e.address));
        item->setText(2, QString::number(e.dpl));
        item->setText(3, hex4(e.selector));

        if (e.isrList.empty())
        {
            item->setText(4, "-");
        }
        else if (e.isrList.size() == 1)
        {
            item->setText(4, hex(e.isrList[0]));
        }
        else
        {
            QStringList list;
            for (auto a : e.isrList)
                list << hex(a);
            if (e.isrTotal > e.isrList.size())
                list << QString("+%1 more").arg(e.isrTotal - (ULONG32)e.isrList.size());
            item->setText(4, list.join(" | "));
        }
    }
    ui->idtCountLabel->setText(QString("Entries: %1").arg(idt.size()));
    checkProtocolMismatch();
}

void MainWindow::onRefreshSystemThreads()
{
    ui->systemThreadsTable->setSortingEnabled(false);
    auto threads = m_device.querySystemThreads();

    ui->systemThreadsTable->setRowCount(0);
    for (auto& t : threads)
    {
        int row = ui->systemThreadsTable->rowCount();
        ui->systemThreadsTable->insertRow(row);
        ui->systemThreadsTable->setItem(row, 0, new QTableWidgetItem(hex(t.tid)));
        ULONG64 dispAddr = t.win32StartAddress ? t.win32StartAddress : t.startAddress;
        ui->systemThreadsTable->setItem(row, 1, new QTableWidgetItem(hex(dispAddr)));
        ui->systemThreadsTable->setItem(row, 2, new QTableWidgetItem(QString::number((int)t.priority)));
        ui->systemThreadsTable->setItem(row, 3, new QTableWidgetItem(QString::number((int)t.basePriority)));
        ui->systemThreadsTable->setItem(row, 4, new QTableWidgetItem(QString::fromLatin1(threadStateName(t.state))));
        ui->systemThreadsTable->setItem(row, 5, new QTableWidgetItem(t.moduleName));
        ui->systemThreadsTable->setItem(row, 6, new QTableWidgetItem(t.modulePath));
    }
    ui->sysThreadsCountLabel->setText(QString("Count: %1").arg(ui->systemThreadsTable->rowCount()));
    checkProtocolMismatch();
    ui->systemThreadsTable->setSortingEnabled(true);
}

void MainWindow::onRefreshWorkThreads()
{
    ui->workThreadsTable->setSortingEnabled(false);
    auto threads = m_device.queryWorkThreads();

    ui->workThreadsTable->setRowCount(0);
    for (auto& t : threads)
    {
        int row = ui->workThreadsTable->rowCount();
        ui->workThreadsTable->insertRow(row);
        ui->workThreadsTable->setItem(row, 0, new QTableWidgetItem(t.poolType == 0 ? "ExPool" : "IoPool"));
        ui->workThreadsTable->setItem(row, 1, new QTableWidgetItem(hex(t.tid)));
        ui->workThreadsTable->setItem(row, 2, new QTableWidgetItem(hex(t.startAddress)));
        ui->workThreadsTable->setItem(row, 3, new QTableWidgetItem(QString::number((int)t.priority)));
        ui->workThreadsTable->setItem(row, 4, new QTableWidgetItem(QString::fromLatin1(threadStateName(t.state))));
        ui->workThreadsTable->setItem(row, 5, new QTableWidgetItem(t.moduleName));
        ui->workThreadsTable->setItem(row, 6, new QTableWidgetItem(t.modulePath));
    }
    ui->workThreadsCountLabel->setText(QString("Count: %1").arg(ui->workThreadsTable->rowCount()));
    checkProtocolMismatch();
    ui->workThreadsTable->setSortingEnabled(true);
}

void MainWindow::onRefreshWfpFilters()
{
    ui->wfpFiltersTable->setSortingEnabled(false);
    auto filters = m_device.queryWfpFilters();

    ui->wfpFiltersTable->setRowCount(0);
    for (auto& f : filters)
    {
        int row = ui->wfpFiltersTable->rowCount();
        ui->wfpFiltersTable->insertRow(row);
        ui->wfpFiltersTable->setItem(row, 0, new QTableWidgetItem(QString::number(f.filterId)));
        ui->wfpFiltersTable->setItem(row, 1, new QTableWidgetItem(f.name));
        ui->wfpFiltersTable->setItem(row, 2, new QTableWidgetItem(f.description));
        ui->wfpFiltersTable->setItem(row, 3, new QTableWidgetItem(
            f.actionType == 0x1001 ? "BLOCK" :
            f.actionType == 0x1002 ? "PERMIT" :
            f.actionType == 0x1003 ? "CALLOUT_TERMINATING" :
            f.actionType == 0x1004 ? "CALLOUT_INSPECTION" :
            f.actionType == 0x1005 ? "CALLOUT_UNKNOWN" : "Unknown"));
        ui->wfpFiltersTable->setItem(row, 4, new QTableWidgetItem(f.layerKey));
        ui->wfpFiltersTable->setItem(row, 5, new QTableWidgetItem(f.calloutKey));
    }
    ui->wfpFiltersCountLabel->setText(QString("Count: %1").arg(ui->wfpFiltersTable->rowCount()));
    checkProtocolMismatch();
    ui->wfpFiltersTable->setSortingEnabled(true);
}

void MainWindow::onRefreshWfpCallouts()
{
    ui->wfpCalloutsTable->setSortingEnabled(false);
    auto callouts = m_device.queryWfpCallouts();

    ui->wfpCalloutsTable->setRowCount(0);
    for (auto& c : callouts)
    {
        int row = ui->wfpCalloutsTable->rowCount();
        ui->wfpCalloutsTable->insertRow(row);

        auto* idItem = new QTableWidgetItem();
        idItem->setData(Qt::DisplayRole, QVariant(c.calloutId));
        ui->wfpCalloutsTable->setItem(row, 0, idItem);

        ui->wfpCalloutsTable->setItem(row, 1, new QTableWidgetItem(c.classifyFn ? hex(c.classifyFn) : "-"));
        ui->wfpCalloutsTable->setItem(row, 2, new QTableWidgetItem(c.notifyFn ? hex(c.notifyFn) : "-"));
        ui->wfpCalloutsTable->setItem(row, 3, new QTableWidgetItem(c.flowDeleteFn ? hex(c.flowDeleteFn) : "-"));
        ui->wfpCalloutsTable->setItem(row, 4, new QTableWidgetItem(c.moduleName));
        ui->wfpCalloutsTable->setItem(row, 5, new QTableWidgetItem(c.modulePath));
    }
    ui->wfpCalloutsCountLabel->setText(QString("Count: %1").arg(ui->wfpCalloutsTable->rowCount()));
    checkProtocolMismatch();
    ui->wfpCalloutsTable->setSortingEnabled(true);
    ui->wfpCalloutsTable->sortByColumn(0, Qt::AscendingOrder);
}

void MainWindow::onRefreshDrivers()
{
    ui->driversTable->setSortingEnabled(false);
    auto drivers = m_device.queryDrivers();

    ui->driversTable->setRowCount(0);
    for (auto& d : drivers)
    {
        int row = ui->driversTable->rowCount();
        ui->driversTable->insertRow(row);
        auto* nameItem = new QTableWidgetItem(d.name);
        nameItem->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(d.driverObject));
        ui->driversTable->setItem(row, 0, nameItem);
        ui->driversTable->setItem(row, 1, new QTableWidgetItem(hex(d.baseAddress)));
        ui->driversTable->setItem(row, 2, new QTableWidgetItem(
            d.size ? QString("0x%1").arg(d.size, 0, 16) : QString()));
        ui->driversTable->setItem(row, 3, new QTableWidgetItem(d.driverObject ? hex(d.driverObject) : "-"));
        ui->driversTable->setItem(row, 4, new QTableWidgetItem(d.objectName));
        ui->driversTable->setItem(row, 5, new QTableWidgetItem(d.path));
    }
    ui->driversCountLabel->setText(QString("Count: %1").arg(ui->driversTable->rowCount()));
    checkProtocolMismatch();
    ui->driversTable->setSortingEnabled(true);
    ui->driversTable->sortByColumn(0, Qt::AscendingOrder);
}

void MainWindow::onDriverDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    auto* item = ui->driversTable->item(row, 0);
    if (!item)
        return;

    ULONG64 driverObject = item->data(Qt::UserRole).toULongLong();
    if (!driverObject)
    {
        logInfo("[WARN] Driver object address unavailable for this entry.");
        return;
    }

    auto* dlg = new DriverDetailDialog(&m_device, driverObject, item->text(), this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MainWindow::onRefreshUnloadedDrivers()
{
    ULONG source = MYARK_UNLOADED_SOURCE_MM;
    if (ui->radioPiDDB->isChecked())
        source = MYARK_UNLOADED_SOURCE_PIDDB;
    else if (ui->radioCiHash->isChecked())
        source = MYARK_UNLOADED_SOURCE_CI;

    ui->unloadedDriversTable->setSortingEnabled(false);
    auto entries = m_device.queryUnloadedDrivers(source);

    ui->unloadedDriversTable->setRowCount(0);
    for (auto& e : entries)
    {
        int row = ui->unloadedDriversTable->rowCount();
        ui->unloadedDriversTable->insertRow(row);
        ui->unloadedDriversTable->setItem(row, 0, new QTableWidgetItem(e.driverName));
        ui->unloadedDriversTable->setItem(row, 1, new QTableWidgetItem(e.driverStart ? hex(e.driverStart) : "-"));
        ui->unloadedDriversTable->setItem(row, 2, new QTableWidgetItem(e.driverEnd ? hex(e.driverEnd) : "-"));
    }
    ui->unloadedDriversCountLabel->setText(QString("Count: %1").arg(ui->unloadedDriversTable->rowCount()));
    checkProtocolMismatch();
    ui->unloadedDriversTable->setSortingEnabled(true);
    ui->unloadedDriversTable->sortByColumn(0, Qt::AscendingOrder);
}

void MainWindow::onRefreshFilters()
{
    auto filters = m_device.queryFilters();

    ui->filtersTree->clear();
    for (auto& f : filters)
    {
        auto* root = new QTreeWidgetItem(ui->filtersTree);
        root->setText(0, QString("%1 (%2)").arg(f.name, f.moduleName));
        root->setExpanded(true);

        auto addField = [&](QTreeWidgetItem* parent, const QString& label, ULONG64 addr) {
            auto* item = new QTreeWidgetItem(parent);
            item->setText(0, label);
            item->setText(3, addr ? hex(addr) : "(null)");
            return item;
        };

        auto* pathItem = new QTreeWidgetItem(root);
        pathItem->setText(0, "Module Path");
        pathItem->setText(3, f.modulePath);

        addField(root, "Filter Address", f.filterAddress);
        addField(root, "FilterUnloadCallback", f.filterUnloadCallback);
        addField(root, "InstanceSetupCallback", f.instanceSetupCallback);
        addField(root, "InstanceQueryTeardownCallback", f.instanceQueryTeardownCallback);
        addField(root, "InstanceTeardownStartCallback", f.instanceTeardownStartCallback);
        addField(root, "InstanceTeardownCompleteCallback", f.instanceTeardownCompleteCallback);
        addField(root, "GenerateFileNameCallback", f.generateFileNameCallback);
        addField(root, "NormalizeNameComponentCallback", f.normalizeNameComponentCallback);
        addField(root, "NormalizeContextCleanupCallback", f.normalizeContextCleanupCallback);
        addField(root, "TransactionNotificationCallback", f.transactionNotificationCallback);
        addField(root, "NormalizeNameComponentExCallback", f.normalizeNameComponentExCallback);
        addField(root, "SectionNotificationCallback", f.sectionNotificationCallback);

        auto* opsNode = new QTreeWidgetItem(root);
        opsNode->setText(0, QString("Operations (%1)").arg(f.operations.size()));
        opsNode->setExpanded(true);

        for (auto& op : f.operations)
        {
            auto* item = new QTreeWidgetItem(opsNode);
            item->setText(0, QString::fromLatin1(majorFunctionName(op.majorFunction)));
            item->setText(1, op.preOperation ? hex(op.preOperation) : "(null)");
            item->setText(2, op.postOperation ? hex(op.postOperation) : "(null)");
        }
    }
    ui->filtersCountLabel->setText(QString("Count: %1").arg(filters.size()));
    checkProtocolMismatch();
}

void MainWindow::onRefreshObject()
{
    ui->objectTable->setSortingEnabled(false);
    auto object = m_device.queryObject();

    ui->objectTable->setRowCount(0);
    for (auto& t : object)
    {
        int row = ui->objectTable->rowCount();
        ui->objectTable->insertRow(row);
        auto* indexItem = new QTableWidgetItem(QString::number(t.index));
        indexItem->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(t.objectType));
        ui->objectTable->setItem(row, 0, indexItem);
        ui->objectTable->setItem(row, 1, new QTableWidgetItem(t.objectName));
        ui->objectTable->setItem(row, 2, new QTableWidgetItem(QString::number(t.handleCount)));
        ui->objectTable->setItem(row, 3, new QTableWidgetItem(QString::number(t.objectCount)));
    }
    ui->procCbCountLabel_5->setText(QString("Count: %1").arg(ui->objectTable->rowCount()));
    checkProtocolMismatch();
    ui->objectTable->setSortingEnabled(true);
}

void MainWindow::onObjectProcedureDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    auto* item = ui->objectTable->item(row, 0);
    if (!item)
        return;

    ULONG64 objectType = item->data(Qt::UserRole).toULongLong();
    if (!objectType)
    {
        logInfo("[WARN] Object type address unavailable for this entry.");
        return;
    }

    auto* nameItem = ui->objectTable->item(row, 1);
    QString objName = nameItem ? nameItem->text() : QString();

    auto* dlg = new ObjectProcedureDialog(&m_device, objectType, objName, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

// Refresh the newly selected top-level tab.
void MainWindow::onMainTabChanged(int index)
{
    QWidget* page = ui->mainTabs->widget(index);
    if (page == ui->tabProcess)
        onRefreshProcess();
    else if (page == ui->tabService)
        onRefreshService();
    else if (page == ui->tabKernel)
        onObjectTabChanged(ui->objectTab->currentIndex());
}

// Refresh the newly selected Kernel sub-tab.
void MainWindow::onObjectTabChanged(int index)
{
    QWidget* page = ui->objectTab->widget(index);
    if (!page)
        return;

    if (page == ui->tabObCallbacks)
        onRefreshObCallbacks();
    else if (page == ui->tabProcessCallbacks)
        onRefreshProcCallbacks();
    else if (page == ui->tabRegCallbacks)
        onRefreshRegCallbacks();
    else if (page == ui->tabBugCheckCallbacks)
        onRefreshBugCheckCallbacks();
    else if (page == ui->tabShutdownCallbacks)
        onRefreshShutdownCallbacks();
    else if (page == ui->tabBugCheckRegisters)
        onRefreshBugCheckRegisters();
    else if (page == ui->tabNotifyShutdown)
        onRefreshNotifyShutdown();
    else if (page == ui->tabTimers)
        onRefreshTimers();
    else if (page == ui->tabSsdt)
        onRefreshSsdt();
    else if (page == ui->tabShadowSsdt)
        onRefreshShadowSsdt();
    else if (page == ui->tabIdt)
        onRefreshIdt();
    else if (page == ui->tabSystemThreads)
        onRefreshSystemThreads();
    else if (page == ui->tabMiniFilters)
        onRefreshFilters();
    else if (page == ui->tabWorkThreads)
        onRefreshWorkThreads();
    else if (page == ui->tabWfpFilters)
        onRefreshWfpFilters();
    else if (page == ui->tabWfpCallouts)
        onRefreshWfpCallouts();
    else if (page == ui->tabDrivers)
        onRefreshDrivers();
    else if (page == ui->tabUnloadedDrivers)
        onRefreshUnloadedDrivers();
    else if (page == ui->tab)
        onRefreshObject();
}

// Refresh the unloaded-driver list when the source changes.
void MainWindow::onUnloadedSourceChanged(bool checked)
{
    // Refresh only when a radio button becomes checked (toggled(true))
    if (checked)
        onRefreshUnloadedDrivers();
}
