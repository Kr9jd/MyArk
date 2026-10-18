#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_mainwindow.h"
#include "kerneldevice.h"

class QTreeWidgetItem;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindowClass; };
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void onRefreshProcess();
    void onRefreshService();
    void onRefreshObCallbacks();
    void onRefreshProcCallbacks();
    void onRefreshRegCallbacks(); 
    void onRefreshBugCheckCallbacks();
    void onRefreshShutdownCallbacks();
    void onRefreshBugCheckRegisters();
    void onRefreshNotifyShutdown();
    void onRefreshTimers();
    void onRefreshSsdt();
    void onRefreshShadowSsdt();
    void onRefreshIdt();
    void onRefreshSystemThreads();
    void onRefreshWorkThreads();
    void onRefreshWfpFilters();
    void onRefreshWfpCallouts();
    void onRefreshDrivers();      
    void onRefreshUnloadedDrivers();
    void onDriverDoubleClicked(int row, int column);
    void onObjectProcedureDoubleClicked(int row, int column);
    void onRefreshFilters();
    void onRefreshObject();
    void onObjectTabChanged(int index);
    void onMainTabChanged(int index);
    void onUnloadedSourceChanged(bool checked);

private:
    void setupTables();
    void setupRefreshMenus();
    void openRowInExplorer(QWidget* view, const QPoint& pos);
    void disassembleRow(QWidget* view, const QPoint& pos);
    bool isExistingFile(const QString& text);
    bool parseCodeAddress(const QString& text, ULONG64& out);
    QString findPathInTreeItem(QTreeWidgetItem* item);
    void logInfo(const QString& text);
    void checkProtocolMismatch();
    void copyTableSelection(QTableWidget* table);

    Ui::MainWindowClass *ui;
    KernelDevice m_device;
};
