#pragma once
#include <windows.h>
#include <vector>
#include <unordered_map>
#include <QString>
#include "protocol.h"

struct ObCallbackDisplayEntry {
    ULONG64 preOperation;
    ULONG64 postOperation;
    ULONG   operations;
    ULONG   type;               // 0 = Process, 1 = Thread
    QString moduleName;
    QString modulePath;
};

struct ProcessCallbackDisplayEntry {
    ULONG64 callback;
    ULONG64 type;
    QString moduleName;
    QString modulePath;
};

struct RegCallbackDisplayEntry {
    ULONG64 callback;
    QString altitude;
    QString moduleName;
    QString modulePath;
};

struct TimerDisplayEntry {
    ULONG64 type;
    ULONG64 timerAddress;
    ULONG64 routineAddress;
    QString moduleName;
    QString modulePath;
};

struct SsdtDisplayEntry {
    ULONG32 index;
    ULONG64 address;
    QString functionName;
};

struct IdtDisplayEntry {
    ULONG32 cpu;
    ULONG32 index;
    USHORT selector;
    USHORT dpl;
    ULONG64 address;
    ULONG32 isrTotal;
    std::vector<ULONG64> isrList;
};

struct SystemThreadDisplayEntry {
    ULONG64 tid;
    ULONG64 startAddress;
    ULONG64 win32StartAddress;
    CHAR priority;
    CHAR basePriority;
    ULONG32 state;
    QString moduleName;
    QString modulePath;
};

struct WorkThreadDisplayEntry {
    ULONG32 poolType;        // 0 = ExPool, 1 = IoPool
    ULONG64 tid;
    ULONG64 startAddress;
    CHAR priority;
    CHAR basePriority;
    ULONG32 state;
    QString moduleName;
    QString modulePath;
};

struct MiniFilterOperationDisplayEntry {
    ULONG32 majorFunction;
    ULONG64 preOperation;
    ULONG64 postOperation;
};

struct MiniFilterDisplayEntry {
    QString name;
    QString moduleName;
    QString modulePath;
    ULONG64 filterAddress;
    ULONG64 filterUnloadCallback;
    ULONG64 instanceSetupCallback;
    ULONG64 instanceQueryTeardownCallback;
    ULONG64 instanceTeardownStartCallback;
    ULONG64 instanceTeardownCompleteCallback;
    ULONG64 generateFileNameCallback;
    ULONG64 normalizeNameComponentCallback;
    ULONG64 normalizeContextCleanupCallback;
    ULONG64 transactionNotificationCallback;
    ULONG64 normalizeNameComponentExCallback;
    ULONG64 sectionNotificationCallback;
    std::vector<MiniFilterOperationDisplayEntry> operations;
};

struct KernelObjectDisplayEntry {
    UCHAR       index;
    QString     objectName;
    ULONG64     handleCount;
    ULONG64     objectCount;
    ULONG64     objectType;
};

struct ObjectProcedureDisplayEntry {
    ULONG32     procedureId;
    ULONG64     address;
    QString     moduleName;
    QString     modulePath;
};

struct WfpFilterDisplayEntry {
    ULONG64     filterId;
    QString     name;
    QString     description;
    ULONG32     actionType;
    QString     layerKey;
    QString     calloutKey;
};

struct WfpCalloutDisplayEntry {
    ULONG32     calloutId;
    ULONG64     classifyFn;
    ULONG64     notifyFn;
    ULONG64     flowDeleteFn;
    QString     moduleName;
    QString     modulePath;
};

struct DriverDisplayEntry {
    ULONG64     driverObject;
    ULONG64     baseAddress;
    ULONG       size;
    QString     name;
    QString     objectName;
    QString     path;
};

struct UnloadedDriverDisplayEntry {
    ULONG64     driverStart;
    ULONG64     driverEnd;
    QString     driverName;
};

struct BugCheckCallbackDisplayEntry {
    ULONG64     callback;
    ULONG       reason;
    QString     moduleName;
    QString     modulePath;
};

struct ShutdownCallbackDisplayEntry {
    ULONG64     callback;
    QString     moduleName;
    QString     modulePath;
};

struct BugCheckRegisterDisplayEntry {
    ULONG64     callback;
    QString     component;
    QString     moduleName;
    QString     modulePath;
};

struct NotifyShutdownDisplayEntry {
    ULONG64     callback;
    QString     moduleName;
    QString     modulePath;
};

struct DriverDeviceDisplayEntry {
    ULONG64     deviceObject;
    ULONG64     nextDevice;
    ULONG64     attachedDevice;
    ULONG32     flags;
    ULONG32     characteristics;
    ULONG32     deviceType;
    LONG        referenceCount;
    ULONG32     stackSize;
    QString     deviceName;
};

struct DriverDispatchDisplayEntry {
    ULONG32     majorFunction;
    ULONG64     dispatchAddress;
    QString     moduleName;
    QString     modulePath;
};

class KernelDevice
{
public:
    bool connectDevice();
    void disconnectDevice();
    bool isConnected() const { return m_hDevice != INVALID_HANDLE_VALUE; }

    std::vector<ObCallbackDisplayEntry> queryObCallbacks();
    std::vector<ProcessCallbackDisplayEntry> queryProcessCallbacks();
    std::vector<RegCallbackDisplayEntry> queryRegCallbacks();
    std::vector<BugCheckCallbackDisplayEntry> queryBugCheckCallbacks();
    std::vector<ShutdownCallbackDisplayEntry> queryShutdownCallbacks();
    std::vector<BugCheckRegisterDisplayEntry> queryBugCheckRegisters();
    std::vector<NotifyShutdownDisplayEntry> queryNotifyShutdown();
    std::vector<TimerDisplayEntry> queryTimers();
    std::vector<SsdtDisplayEntry> querySsdt();
    std::vector<SsdtDisplayEntry> queryShadowSsdt();
    std::vector<IdtDisplayEntry> queryIdt();
    std::vector<SystemThreadDisplayEntry> querySystemThreads();
    std::vector<WorkThreadDisplayEntry> queryWorkThreads();
    std::vector<MiniFilterDisplayEntry> queryFilters();
    std::vector<KernelObjectDisplayEntry> queryObject();
    std::vector<ObjectProcedureDisplayEntry> queryObjectTypeProcedures(ULONG64 objectType);
    std::vector<BYTE> readMemory(ULONG64 address, ULONG64 size);
    std::vector<WfpFilterDisplayEntry> queryWfpFilters();
    std::vector<WfpCalloutDisplayEntry> queryWfpCallouts();
    std::vector<DriverDisplayEntry> queryDrivers();
    std::vector<UnloadedDriverDisplayEntry> queryUnloadedDrivers(ULONG source);
    std::vector<DriverDeviceDisplayEntry> queryDriverDevices(ULONG64 driverObject);
    std::vector<DriverDispatchDisplayEntry> queryDriverDispatch(ULONG64 driverObject);
    bool lastProtocolMismatch() const { return m_lastProtocolMismatch; }

private:
    std::vector<BYTE> doIoctl(DWORD code);
    std::vector<BYTE> doIoctlWithInput(DWORD code, const void* input, DWORD inputSize);
    void buildSyscallNameMapFor(PCWSTR moduleName, std::unordered_map<ULONG, QString>& outMap);

    HANDLE m_hDevice = INVALID_HANDLE_VALUE;
    bool m_lastProtocolMismatch = false;
    std::unordered_map<ULONG, QString> m_ntdllMap;
    std::unordered_map<ULONG, QString> m_win32uMap;
};
