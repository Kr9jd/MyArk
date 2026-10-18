#include "kerneldevice.h"
#include "protocol.h"
#include "pattern.h"
#include <unordered_map>
#include <cstddef>
#include <QString>

static QString wcharToQString(const WCHAR* w)
{
    return QString::fromWCharArray(w ? w : L"");
}

static QString wcharToPath(const WCHAR* w)
{
    QString s = QString::fromWCharArray(w ? w : L"");
    int idx = s.indexOf("\\SystemRoot", 0, Qt::CaseInsensitive);
    if (idx >= 0)
    {
        WCHAR windir[MAX_PATH] = {};
        if (GetWindowsDirectoryW(windir, MAX_PATH) > 0)
        {
            const QString prefix = "\\SystemRoot";
            s = s.left(idx) + QString::fromWCharArray(windir) + s.mid(idx + prefix.length());
        }
    }
    return s;
}

// Open a handle to the driver device.
bool KernelDevice::connectDevice()
{
    if (m_hDevice != INVALID_HANDLE_VALUE)
        return true;

    m_hDevice = CreateFileW(MYARK_DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, 0, nullptr);
    return m_hDevice != INVALID_HANDLE_VALUE;
}

// Close the driver device handle.
void KernelDevice::disconnectDevice()
{
    if (m_hDevice != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
}

// Send an IOCTL with no input and return the raw response buffer.
std::vector<BYTE> KernelDevice::doIoctl(DWORD code)
{
    std::vector<BYTE> buf;
    if (m_hDevice == INVALID_HANDLE_VALUE)
        return buf;

    ULONG bufSize = 32 * 1024 * 1024;
    buf.resize(bufSize);
    DWORD returned = 0;
    if (!DeviceIoControl(m_hDevice, code, nullptr, 0, buf.data(), bufSize, &returned, nullptr)
        || returned < sizeof(MYARK_RESPONSE_HEADER))
    {
        buf.clear();
        return buf;
    }

    auto* resp = reinterpret_cast<MYARK_RESPONSE_HEADER*>(buf.data());
    if (resp->ProtocolVersion != MYARK_PROTOCOL_VERSION)
    {
        m_lastProtocolMismatch = true;
        buf.clear();
        return buf;
    }
    m_lastProtocolMismatch = false;
    return buf;
}

// Send an IOCTL with an input buffer and return the raw response.
std::vector<BYTE> KernelDevice::doIoctlWithInput(DWORD code, const void* input, DWORD inputSize)
{
    std::vector<BYTE> buf;
    if (m_hDevice == INVALID_HANDLE_VALUE)
        return buf;

    ULONG bufSize = 32 * 1024 * 1024;
    buf.resize(bufSize);
    DWORD returned = 0;
    if (!DeviceIoControl(m_hDevice, code, const_cast<void*>(input), inputSize,
        buf.data(), bufSize, &returned, nullptr)
        || returned < sizeof(MYARK_RESPONSE_HEADER))
    {
        buf.clear();
        return buf;
    }

    auto* resp = reinterpret_cast<MYARK_RESPONSE_HEADER*>(buf.data());
    if (resp->ProtocolVersion != MYARK_PROTOCOL_VERSION)
    {
        m_lastProtocolMismatch = true;
        buf.clear();
        return buf;
    }
    m_lastProtocolMismatch = false;
    return buf;
}

// Build a syscall-number to name map from a module export table.
void KernelDevice::buildSyscallNameMapFor(PCWSTR moduleName, std::unordered_map<ULONG, QString>& outMap)
{
    if (!outMap.empty())
        return;

    HMODULE mod = GetModuleHandleW(moduleName);
    if (!mod)
        return;

    auto* dos = (PIMAGE_DOS_HEADER)mod;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return;
    auto* nth = (PIMAGE_NT_HEADERS)((ULONG_PTR)mod + dos->e_lfanew);
    if (nth->Signature != IMAGE_NT_SIGNATURE)
        return;

    ULONG exportRva = nth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!exportRva)
        return;
    auto* expDir = (PIMAGE_EXPORT_DIRECTORY)((ULONG_PTR)mod + exportRva);
    auto* eat = (PULONG)((ULONG_PTR)mod + expDir->AddressOfFunctions);
    auto* ent = (PULONG)((ULONG_PTR)mod + expDir->AddressOfNames);
    auto* eot = (PUSHORT)((ULONG_PTR)mod + expDir->AddressOfNameOrdinals);

    std::unordered_map<ULONG64, QString> exportNames;
    for (ULONG i = 0; i < expDir->NumberOfNames; i++)
    {
        ULONG64 addr = (ULONG64)mod + eat[eot[i]];
        const char* name = (const char*)((ULONG_PTR)mod + ent[i]);
        exportNames[addr] = QString::fromLatin1(name);
    }

    ULONG imageSize = nth->OptionalHeader.SizeOfImage;
    ULONG hitCount = 0;
    void** hits = PatternScanAll(mod, imageSize, "4C 8B D1 B8", &hitCount);
    if (!hits)
        return;

    for (ULONG i = 0; i < hitCount; i++)
    {
        PUCHAR entry = (PUCHAR)hits[i];
        ULONG syscallNo = *(PULONG)(entry + 4);
        auto it = exportNames.find((ULONG64)entry);
        if (it != exportNames.end())
            outMap[syscallNo] = it->second;
    }
    delete[] hits;
}

// Query ObRegisterCallbacks registrations.
std::vector<ObCallbackDisplayEntry> KernelDevice::queryObCallbacks()
{
    std::vector<ObCallbackDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_OB_CALLBACKS);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_OB_CALLBACK_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        ObCallbackDisplayEntry e;
        e.preOperation  = resp->Entries[i].PreOperation;
        e.postOperation = resp->Entries[i].PostOperation;
        e.operations    = resp->Entries[i].Operations;
        e.type          = resp->Entries[i].Type;
        e.moduleName   = wcharToQString(resp->Entries[i].ModuleName);
        e.modulePath = wcharToPath(resp->Entries[i].ModulePath);
        result.push_back(e);
    }
    return result;
}

// Query process/thread/image notify routines.
std::vector<ProcessCallbackDisplayEntry> KernelDevice::queryProcessCallbacks()
{
    std::vector<ProcessCallbackDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_PROCERSS_CALLBACKS);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_PROCESS_CALLBACK_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        ProcessCallbackDisplayEntry e;
        e.callback   = resp->Entries[i].Callback;
        e.type       = resp->Entries[i].Type;
        e.moduleName = wcharToQString(resp->Entries[i].ModuleName);
        e.modulePath = wcharToPath(resp->Entries[i].ModulePath);
        result.push_back(e);
    }
    return result;
}

// Query CmRegisterCallback registrations.
std::vector<RegCallbackDisplayEntry> KernelDevice::queryRegCallbacks()
{
    std::vector<RegCallbackDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_REG_CALLBACKS);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_REGISTER_CALLBACK_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        RegCallbackDisplayEntry e;
        e.callback   = resp->Entries[i].Callback;
        e.altitude   = wcharToQString(resp->Entries[i].Altitude);
        e.moduleName = wcharToQString(resp->Entries[i].ModuleName);
        e.modulePath = wcharToPath(resp->Entries[i].ModulePath);
        result.push_back(e);
    }
    return result;
}

// Query KeRegisterBugCheckReasonCallback callbacks.
std::vector<BugCheckCallbackDisplayEntry> KernelDevice::queryBugCheckCallbacks()
{
    std::vector<BugCheckCallbackDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_BUGCHECK_CALLBACKS);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_BUGCHECK_CALLBACK_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        BugCheckCallbackDisplayEntry e;
        e.callback   = resp->Entries[i].Callback;
        e.reason     = resp->Entries[i].Reason;
        e.moduleName = wcharToQString(resp->Entries[i].ModuleName);
        e.modulePath = wcharToPath(resp->Entries[i].ModulePath);
        result.push_back(e);
    }
    return result;
}

// Query last-chance shutdown callbacks.
std::vector<ShutdownCallbackDisplayEntry> KernelDevice::queryShutdownCallbacks()
{
    std::vector<ShutdownCallbackDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_SHUTDOWN_CALLBACKS);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_SHUTDOWN_CALLBACK_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        ShutdownCallbackDisplayEntry e;
        e.callback   = resp->Entries[i].Callback;
        e.moduleName = wcharToQString(resp->Entries[i].ModuleName);
        e.modulePath = wcharToPath(resp->Entries[i].ModulePath);
        result.push_back(e);
    }
    return result;
}

// Query KeRegisterBugCheckCallback callbacks.
std::vector<BugCheckRegisterDisplayEntry> KernelDevice::queryBugCheckRegisters()
{
    std::vector<BugCheckRegisterDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_BUGCHECK_REGISTERS);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_BUGCHECK_REGISTER_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        BugCheckRegisterDisplayEntry e;
        e.callback   = resp->Entries[i].Callback;
        e.component  = wcharToQString(resp->Entries[i].Component);
        e.moduleName = wcharToQString(resp->Entries[i].ModuleName);
        e.modulePath = wcharToPath(resp->Entries[i].ModulePath);
        result.push_back(e);
    }
    return result;
}

// Query IoRegisterShutdownNotification callbacks.
std::vector<NotifyShutdownDisplayEntry> KernelDevice::queryNotifyShutdown()
{
    std::vector<NotifyShutdownDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_NOTIFY_SHUTDOWN);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_NOTIFY_SHUTDOWN_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        NotifyShutdownDisplayEntry e;
        e.callback   = resp->Entries[i].Callback;
        e.moduleName = wcharToQString(resp->Entries[i].ModuleName);
        e.modulePath = wcharToPath(resp->Entries[i].ModulePath);
        result.push_back(e);
    }
    return result;
}

// Query DPC and IO timers.
std::vector<TimerDisplayEntry> KernelDevice::queryTimers()
{
    std::vector<TimerDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_TIMERS);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_TIMER_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        TimerDisplayEntry e;
        e.type           = resp->Entries[i].Type;
        e.timerAddress   = resp->Entries[i].TimerAddress;
        e.routineAddress = resp->Entries[i].RoutineAddress;
        e.moduleName     = wcharToQString(resp->Entries[i].ModuleName);
        e.modulePath = wcharToPath(resp->Entries[i].ModulePath);
        result.push_back(e);
    }
    return result;
}

// Query the SSDT.
std::vector<SsdtDisplayEntry> KernelDevice::querySsdt()
{
    std::vector<SsdtDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_SSDT);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_SSDT_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        SsdtDisplayEntry e;
        e.index        = resp->Entries[i].Index;
        e.address      = resp->Entries[i].Address;
        e.functionName = wcharToQString(resp->Entries[i].FunctionName);
        result.push_back(e);
    }

    buildSyscallNameMapFor(L"ntdll.dll", m_ntdllMap);
    for (auto& e : result)
    {
        if (!e.functionName.isEmpty())
            continue;
        auto it = m_ntdllMap.find(e.index);
        if (it != m_ntdllMap.end())
            e.functionName = it->second;
    }
    return result;
}

// Query the shadow SSDT.
std::vector<SsdtDisplayEntry> KernelDevice::queryShadowSsdt()
{
    std::vector<SsdtDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_SHADOW_SSDT);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_SSDT_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        SsdtDisplayEntry e;
        e.index        = resp->Entries[i].Index;
        e.address      = resp->Entries[i].Address;
        e.functionName = wcharToQString(resp->Entries[i].FunctionName);
        result.push_back(e);
    }

    buildSyscallNameMapFor(L"win32u.dll", m_win32uMap);
    for (auto& e : result)
    {
        if (!e.functionName.isEmpty())
            continue;
        auto it = m_win32uMap.find(0x1000 + e.index);
        if (it != m_win32uMap.end())
            e.functionName = it->second;
    }
    return result;
}

// Query the IDT.
std::vector<IdtDisplayEntry> KernelDevice::queryIdt()
{
    std::vector<IdtDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_IDT);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_IDT_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        IdtDisplayEntry e;
        e.cpu      = resp->Entries[i].Cpu;
        e.index    = resp->Entries[i].Index;
        e.selector = resp->Entries[i].Selector;
        e.dpl      = resp->Entries[i].Dpl;
        e.address  = resp->Entries[i].Address;
        e.isrTotal = resp->Entries[i].IsrCount;
        for (ULONG j = 0; j < resp->Entries[i].IsrCount && j < 4; j++)
            e.isrList.push_back(resp->Entries[i].IsrAddress[j]);
        result.push_back(e);
    }
    return result;
}

// Query system threads.
std::vector<SystemThreadDisplayEntry> KernelDevice::querySystemThreads()
{
    std::vector<SystemThreadDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_SYSTEM_THREADS);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_SYSTEM_THREAD_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        SystemThreadDisplayEntry e;
        e.tid               = resp->Entries[i].Tid;
        e.startAddress      = resp->Entries[i].StartAddress;
        e.win32StartAddress = resp->Entries[i].Win32StartAddress;
        e.priority          = resp->Entries[i].Priority;
        e.basePriority      = resp->Entries[i].BasePriority;
        e.state             = resp->Entries[i].State;
        e.moduleName        = wcharToQString(resp->Entries[i].ModuleName);
        e.modulePath = wcharToPath(resp->Entries[i].ModulePath);
        result.push_back(e);
    }
    return result;
}

// Query worker threads.
std::vector<WorkThreadDisplayEntry> KernelDevice::queryWorkThreads()
{
    std::vector<WorkThreadDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_WORK_THREADS);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_WORK_THREAD_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        WorkThreadDisplayEntry e;
        e.poolType     = resp->Entries[i].PoolType;
        e.tid          = resp->Entries[i].Tid;
        e.startAddress = resp->Entries[i].StartAddress;
        e.priority     = resp->Entries[i].Priority;
        e.basePriority = resp->Entries[i].BasePriority;
        e.state        = resp->Entries[i].State;
        e.moduleName   = wcharToQString(resp->Entries[i].ModuleName);
        result.push_back(e);
    }
    return result;
}

// Query WFP filters.
std::vector<WfpFilterDisplayEntry> KernelDevice::queryWfpFilters()
{
    std::vector<WfpFilterDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_WFP_FILTERS);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_WFP_FILTER_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        WfpFilterDisplayEntry e;
        e.filterId    = resp->Entries[i].FilterId;
        e.name        = wcharToQString(resp->Entries[i].Name);
        e.description = wcharToQString(resp->Entries[i].Description);
        e.actionType  = resp->Entries[i].ActionType;
        e.layerKey    = wcharToQString(resp->Entries[i].LayerKey);
        e.calloutKey  = wcharToQString(resp->Entries[i].CalloutKey);
        result.push_back(e);
    }
    return result;
}

// Query WFP callouts.
std::vector<WfpCalloutDisplayEntry> KernelDevice::queryWfpCallouts()
{
    std::vector<WfpCalloutDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_WFP_CALLOUTS);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_WFP_CALLOUT_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        WfpCalloutDisplayEntry e;
        e.calloutId    = resp->Entries[i].CalloutId;
        e.classifyFn   = resp->Entries[i].ClassifyFn;
        e.notifyFn     = resp->Entries[i].NotifyFn;
        e.flowDeleteFn = resp->Entries[i].FlowDeleteFn;
        e.moduleName   = wcharToQString(resp->Entries[i].ModuleName);
        e.modulePath = wcharToPath(resp->Entries[i].ModulePath);
        result.push_back(e);
    }
    return result;
}

// Query loaded drivers.
std::vector<DriverDisplayEntry> KernelDevice::queryDrivers()
{
    std::vector<DriverDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_DRIVER);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_DRIVER_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        DriverDisplayEntry e;
        e.driverObject = resp->Entries[i].DriverObject;
        e.baseAddress  = resp->Entries[i].BaseAddress;
        e.size         = resp->Entries[i].Size;
        e.name         = wcharToQString(resp->Entries[i].Name);
        e.objectName   = wcharToQString(resp->Entries[i].ObjectName);
        e.path         = wcharToQString(resp->Entries[i].Path);
        result.push_back(e);
    }
    return result;
}

// Query unloaded drivers from the selected source.
std::vector<UnloadedDriverDisplayEntry> KernelDevice::queryUnloadedDrivers(ULONG source)
{
    std::vector<UnloadedDriverDisplayEntry> result;
    MYARK_UNLOADED_DRIVER_REQUEST req = {};
    req.Source = source;
    auto buf = doIoctlWithInput(IOCTL_MYARK_QUERY_UNLOADED_DRIVERS, &req, sizeof(req));
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_UNLOADED_DRIVER_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        UnloadedDriverDisplayEntry e;
        e.driverStart = resp->Entries[i].DriverStart;
        e.driverEnd   = resp->Entries[i].DriverEnd;
        e.driverName  = wcharToQString(resp->Entries[i].DriverName);
        result.push_back(e);
    }
    return result;
}

// Query the device list of a driver object.
std::vector<DriverDeviceDisplayEntry> KernelDevice::queryDriverDevices(ULONG64 driverObject)
{
    std::vector<DriverDeviceDisplayEntry> result;
    MYARK_DRIVER_DETAIL_REQUEST req = {};
    req.DriverObject = driverObject;
    auto buf = doIoctlWithInput(IOCTL_MYARK_QUERY_DRIVER_DEVICES, &req, sizeof(req));
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_DRIVER_DEVICE_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        DriverDeviceDisplayEntry e;
        e.deviceObject  = resp->Entries[i].DeviceObject;
        e.nextDevice    = resp->Entries[i].NextDevice;
        e.attachedDevice = resp->Entries[i].AttachedDevice;
        e.flags         = resp->Entries[i].Flags;
        e.characteristics = resp->Entries[i].Characteristics;
        e.deviceType    = resp->Entries[i].DeviceType;
        e.referenceCount = resp->Entries[i].ReferenceCount;
        e.stackSize     = resp->Entries[i].StackSize;
        e.deviceName    = wcharToQString(resp->Entries[i].DeviceName);
        result.push_back(e);
    }
    return result;
}

// Query the dispatch table of a driver object.
std::vector<DriverDispatchDisplayEntry> KernelDevice::queryDriverDispatch(ULONG64 driverObject)
{
    std::vector<DriverDispatchDisplayEntry> result;
    MYARK_DRIVER_DETAIL_REQUEST req = {};
    req.DriverObject = driverObject;
    auto buf = doIoctlWithInput(IOCTL_MYARK_QUERY_DRIVER_DISPATCH, &req, sizeof(req));
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_DRIVER_DISPATCH_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        DriverDispatchDisplayEntry e;
        e.majorFunction   = resp->Entries[i].MajorFunction;
        e.dispatchAddress = resp->Entries[i].DispatchAddress;
        e.moduleName      = wcharToQString(resp->Entries[i].ModuleName);
        e.modulePath      = wcharToPath(resp->Entries[i].ModulePath);
        result.push_back(e);
    }
    return result;
}

// Query minifilters.
std::vector<MiniFilterDisplayEntry> KernelDevice::queryFilters()
{
    std::vector<MiniFilterDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_FILTERS);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_FILTER_RESPONSE*>(buf.data());
    const UCHAR* p = buf.data() + sizeof(MYARK_RESPONSE_HEADER);
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        const MYARK_FILTER_ENTRY* entry = (const MYARK_FILTER_ENTRY*)p;
        const MYARK_FILTER_OPERATION_ENTRY* ops = (const MYARK_FILTER_OPERATION_ENTRY*)(entry + 1);

        MiniFilterDisplayEntry e;
        e.name                              = wcharToQString(entry->Name);
        e.moduleName                        = wcharToQString(entry->ModuleName);
        e.modulePath = wcharToPath(entry->ModulePath);
        e.filterAddress                     = entry->FilterAddress;
        e.filterUnloadCallback              = entry->Callbacks.FilterUnloadCallback;
        e.instanceSetupCallback             = entry->Callbacks.InstanceSetupCallback;
        e.instanceQueryTeardownCallback     = entry->Callbacks.InstanceQueryTeardownCallback;
        e.instanceTeardownStartCallback     = entry->Callbacks.InstanceTeardownStartCallback;
        e.instanceTeardownCompleteCallback  = entry->Callbacks.InstanceTeardownCompleteCallback;
        e.generateFileNameCallback          = entry->Callbacks.GenerateFileNameCallback;
        e.normalizeNameComponentCallback    = entry->Callbacks.NormalizeNameComponentCallback;
        e.normalizeContextCleanupCallback   = entry->Callbacks.NormalizeContextCleanupCallback;
        e.transactionNotificationCallback   = entry->Callbacks.TransactionNotificationCallback;
        e.normalizeNameComponentExCallback  = entry->Callbacks.NormalizeNameComponentExCallback;
        e.sectionNotificationCallback       = entry->Callbacks.SectionNotificationCallback;

        for (ULONG j = 0; j < entry->OperationCount; j++)
        {
            MiniFilterOperationDisplayEntry op;
            op.majorFunction = ops[j].MajorFunction;
            op.preOperation  = ops[j].PreOperation;
            op.postOperation = ops[j].PostOperation;
            e.operations.push_back(op);
        }

        p = (const UCHAR*)(ops + entry->OperationCount);
        result.push_back(e);
    }
    return result;
}

// Query kernel object types.
std::vector<KernelObjectDisplayEntry> KernelDevice::queryObject()
{
    std::vector<KernelObjectDisplayEntry> result;
    auto buf = doIoctl(IOCTL_MYARK_QUERY_KERNEL_OBJECT);
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_KERNEL_OBJECT_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        KernelObjectDisplayEntry e;
        e.index = resp->Entries[i].Index;
        e.handleCount = resp->Entries[i].HandleCount;
        e.objectCount = resp->Entries[i].ObjectCount;
        e.objectName =  wcharToQString(resp->Entries[i].ObjectName);
        e.objectType = resp->Entries[i].ObjectType;

        result.push_back(e);
    }
    return result;
}

// Query the procedures of an object type.
std::vector<ObjectProcedureDisplayEntry> KernelDevice::queryObjectTypeProcedures(ULONG64 objectType)
{
    std::vector<ObjectProcedureDisplayEntry> result;
    MYARK_OBJECT_TYPE_REQUEST req = {};
    req.ObjectType = objectType;
    auto buf = doIoctlWithInput(IOCTL_MYARK_QUERY_OBJECT_TYPE_PROCEDURES, &req, sizeof(req));
    if (buf.empty())
        return result;

    auto* resp = reinterpret_cast<MYARK_OBJECT_TYPE_PROCEDURE_RESPONSE*>(buf.data());
    for (ULONG i = 0; i < resp->Header.Count; i++)
    {
        ObjectProcedureDisplayEntry e;
        e.procedureId = resp->Entries[i].ProcedureId;
        e.address     = resp->Entries[i].Address;
        e.moduleName  = wcharToQString(resp->Entries[i].ModuleName);
        e.modulePath  = wcharToPath(resp->Entries[i].ModulePath);
        result.push_back(e);
    }
    return result;
}

// Read a block of kernel memory.
std::vector<BYTE> KernelDevice::readMemory(ULONG64 address, ULONG64 size)
{
    std::vector<BYTE> result;
    if (!address || !size)
        return result;

    MYARK_MEMORY_REQUEST req = {};
    req.Address = address;
    req.ReadSize = size;
    auto buf = doIoctlWithInput(IOCTL_MYARK_READ_MEMORY, &req, sizeof(req));
    if (buf.size() < sizeof(MYARK_RESPONSE_HEADER))
        return result;

    auto* resp = reinterpret_cast<MYARK_MEMORY_RESPONSE*>(buf.data());
    ULONG64 count = resp->Header.Count;
    if (count == 0)
        return result;
    if (count > buf.size() - offsetof(MYARK_MEMORY_RESPONSE, Buffer))
        count = buf.size() - offsetof(MYARK_MEMORY_RESPONSE, Buffer);

    result.assign(resp->Buffer, resp->Buffer + count);
    return result;
}

