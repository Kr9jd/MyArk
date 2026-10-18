#pragma once

// ============================================================
// MyArk R0 <-> R3 communication protocol
// Shared by the kernel driver and the user-mode client
// ============================================================

// Ensure base types are defined
#ifdef _NTDDK_
// Kernel mode: types are provided by wdm.h/ntddk.h

#else
// User mode: use types from windows.h
#ifndef _WINDOWS_
#include <windows.h>
#endif
#ifndef ULONG64
typedef unsigned __int64 ULONG64;
#endif
#endif

#define MYARK_DEVICE_NAME       L"\\\\.\\liunonglink"

#ifndef CTL_CODE
#define CTL_CODE(DeviceType, Function, Method, Access) \
    (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#endif

#ifndef FILE_ANY_ACCESS
#define FILE_ANY_ACCESS 0
#endif

#ifndef METHOD_BUFFERED
#define METHOD_BUFFERED 0
#endif

#define FILE_DEVICE_MYARK       0x00000022  // FILE_DEVICE_UNKNOWN

#define MYARK_CTL(code) \
    CTL_CODE(FILE_DEVICE_MYARK, (code), METHOD_BUFFERED, FILE_ANY_ACCESS)

// IOCTL codes
#define IOCTL_MYARK_QUERY_PROCESS       MYARK_CTL(0x830)  // Enumerate process ObCallback
#define IOCTL_MYARK_QUERY_OB_CALLBACKS  MYARK_CTL(0x831)  // Enumerate ObRegisterCallbacks
#define IOCTL_MYARK_QUERY_DRIVER        MYARK_CTL(0x832)  // Enumerate loaded driver modules
#define IOCTL_MYARK_QUERY_PROCERSS_CALLBACKS  MYARK_CTL(0x833)
#define IOCTL_MYARK_QUERY_REG_CALLBACKS  MYARK_CTL(0x834)

// ============================================================
// Common response header - first field of every output struct
// ============================================================
#define MYARK_PROTOCOL_VERSION  3

typedef struct _MYARK_RESPONSE_HEADER
{
    ULONG   Status;           // 0 = success, otherwise an error code
    ULONG   Count;            // Number of returned entries
    ULONG   TotalSize;        // Total bytes returned this call
    ULONG   ProtocolVersion;  // Protocol version; R3 validates it, reload driver on mismatch
} MYARK_RESPONSE_HEADER;

// ============================================================
// IOCTL_MYARK_QUERY_OB_CALLBACKS
// Input: none (InputBufferLength = 0)
// Output: MYARK_OB_CALLBACK_RESPONSE
// ============================================================
#define MYARK_MAX_MODULE_NAME   64
#define MYARK_MAX_MODULE_PATH   260

typedef struct _MYARK_OB_CALLBACK_ENTRY
{
    ULONG64     PreOperation;               // Callback function address
    ULONG64     PostOperation;              // Callback function address
    ULONG64     RegistrationHandle;         // ObUnRegisterCallbacks handle
    ULONG       Operations;                 // OB_OPERATION_HANDLE_CREATE etc.
    ULONG       Type;                       // 0 = Process, 1 = Thread
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME]; // Owning module name (resolved from address)
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_OB_CALLBACK_ENTRY;

typedef struct _MYARK_OB_CALLBACK_RESPONSE
{
    MYARK_RESPONSE_HEADER   Header;
    MYARK_OB_CALLBACK_ENTRY Entries[1]; // Variable-length array, actual size = Header.Count
} MYARK_OB_CALLBACK_RESPONSE;

// ============================================================
// IOCTL_MYARK_QUERY_DRIVER
// Input: none
// Output: MYARK_DRIVER_RESPONSE
// ============================================================
typedef struct _MYARK_DRIVER_ENTRY
{
    ULONG64     DriverObject;           // Undocumented: PDRIVER_OBJECT (reserved)
    ULONG64     BaseAddress;            // DllBase
    ULONG       Size;                   // SizeOfImage
    ULONG       Reserved;
    WCHAR       Name[MYARK_MAX_MODULE_NAME];   // Driver name (BaseDllName)
    WCHAR       ObjectName[MYARK_MAX_MODULE_NAME]; // Undocumented: object name (reserved)
    WCHAR       Path[260];              // Full path (FullDllName)
} MYARK_DRIVER_ENTRY;

typedef struct _MYARK_DRIVER_RESPONSE
{
    MYARK_RESPONSE_HEADER   Header;
    MYARK_DRIVER_ENTRY      Entries[1];
} MYARK_DRIVER_RESPONSE;

// ============================================================
// Compute the buffer size needed for a variable-length response
// ============================================================
#define MYARK_OB_CALLBACK_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_OB_CALLBACK_ENTRY) * (count))

#define MYARK_DRIVER_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_DRIVER_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_PROCERSS_CALLBACKS
// Input: none
// Output: MYARK_PROCESS_CALLBACK_RESPONSE
// ============================================================
typedef struct _MYARK_PROCESS_CALLBACK_ENTRY
{
    ULONG64     Callback;                       // Callback function address
    ULONG64     Type;
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME];
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_PROCESS_CALLBACK_ENTRY;

typedef struct _MYARK_PROCESS_CALLBACK_RESPONSE
{
    MYARK_RESPONSE_HEADER           Header;
    MYARK_PROCESS_CALLBACK_ENTRY    Entries[1];
} MYARK_PROCESS_CALLBACK_RESPONSE;

#define MYARK_PROCESS_CALLBACK_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_PROCESS_CALLBACK_ENTRY) * (count))

typedef struct _MYARK_REGISTER_CALLBACK_ENTRY
{
    ULONG64     Callback;                       // Callback function address
    WCHAR       Altitude[32];                   // Altitude string
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME];
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_REGISTER_CALLBACK_ENTRY;

typedef struct _MYARK_REGISTER_CALLBACK_RESPONSE
{
    MYARK_RESPONSE_HEADER           Header;
    MYARK_REGISTER_CALLBACK_ENTRY    Entries[1];
} MYARK_REGISTER_CALLBACK_RESPONSE;

#define MYARK_REGISTER_CALLBACK_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_REGISTER_CALLBACK_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_TIMERS
// Combined DPC timers and IO timers
// ============================================================
#define IOCTL_MYARK_QUERY_TIMERS     MYARK_CTL(0x835)

typedef struct _MYARK_TIMER_ENTRY
{
    ULONG64     Type;               // 0 = DPC Timer, 1 = IO Timer
    ULONG64     TimerAddress;       // KTIMER* or IO_TIMER*
    ULONG64     RoutineAddress;     // DeferredRoutine / TimerRoutine
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME];
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_TIMER_ENTRY;

typedef struct _MYARK_TIMER_RESPONSE
{
    MYARK_RESPONSE_HEADER   Header;
    MYARK_TIMER_ENTRY       Entries[1];
} MYARK_TIMER_RESPONSE;

#define MYARK_TIMER_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_TIMER_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_SSDT
// Input: none
// Output: MYARK_SSDT_RESPONSE (includes function names resolved from the export table)
// ============================================================
#define IOCTL_MYARK_QUERY_SSDT       MYARK_CTL(0x836)

typedef struct _MYARK_SSDT_ENTRY
{
    ULONG32     Index;
    ULONG32     Reserved;
    ULONG64     Address;
    WCHAR       FunctionName[MYARK_MAX_MODULE_NAME];
} MYARK_SSDT_ENTRY;

typedef struct _MYARK_SSDT_RESPONSE
{
    MYARK_RESPONSE_HEADER   Header;
    ULONG64                 ModuleBase;         // Kernel base of the module containing the service (for R3 symbol resolution)
    ULONG64                 SelfTestAddress;    // Runtime address of a known export (KeSetTimerEx) in the module, used to validate symbol mapping
    MYARK_SSDT_ENTRY        Entries[1];
} MYARK_SSDT_RESPONSE;

#define MYARK_SSDT_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + 16 + sizeof(MYARK_SSDT_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_SHADOW_SSDT
// Output: MYARK_SSDT_RESPONSE (win32k service table, NtUserXxx/NtGdiXxx)
// ============================================================
#define IOCTL_MYARK_QUERY_SHADOW_SSDT  MYARK_CTL(0x837)

// ============================================================
// IOCTL_MYARK_QUERY_IDT
// Output: MYARK_IDT_RESPONSE (IDT entries grouped by CPU)
// ============================================================
#define IOCTL_MYARK_QUERY_IDT          MYARK_CTL(0x838)

typedef struct _MYARK_IDT_ENTRY
{
    ULONG32     Cpu;
    ULONG32     Index;
    USHORT      Selector;
    USHORT      Dpl;
    ULONG64     Address;
    ULONG64     IsrAddress[4];      // Handler addresses (1 when exclusive; up to the first 4 when shared)
    ULONG32     IsrCount;           // Actual handler count (objects on a shared chain, may be >4)
    ULONG32     Reserved;
} MYARK_IDT_ENTRY;

typedef struct _MYARK_IDT_RESPONSE
{
    MYARK_RESPONSE_HEADER   Header;
    MYARK_IDT_ENTRY         Entries[1];
} MYARK_IDT_RESPONSE;

#define MYARK_IDT_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_IDT_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_SYSTEM_THREADS
// Output: MYARK_SYSTEM_THREAD_RESPONSE (System process thread list)
// ============================================================
#define IOCTL_MYARK_QUERY_SYSTEM_THREADS  MYARK_CTL(0x839)

typedef struct _MYARK_SYSTEM_THREAD_ENTRY
{
    ULONG64     Tid;
    ULONG64     StartAddress;       // Kernel entry (e.g. PspSystemThreadStartup)
    ULONG64     Win32StartAddress;  // User-mode entry (empty = system thread)
    CHAR      Priority;
    CHAR      BasePriority;
    ULONG32     State;
    ULONG32     Reserved;
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME];
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_SYSTEM_THREAD_ENTRY;

typedef struct _MYARK_SYSTEM_THREAD_RESPONSE
{
    MYARK_RESPONSE_HEADER           Header;
    MYARK_SYSTEM_THREAD_ENTRY       Entries[1];
} MYARK_SYSTEM_THREAD_RESPONSE;

#define MYARK_SYSTEM_THREAD_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_SYSTEM_THREAD_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_FILTERS
// Output: MYARK_FILTER_RESPONSE (variable-length: one ENTRY per filter plus OperationCount OP entries)
// ============================================================
#define IOCTL_MYARK_QUERY_FILTERS  MYARK_CTL(0x83A)

typedef struct _MYARK_FILTER_CALLBACKS
{
    ULONG64     FilterUnloadCallback;
    ULONG64     InstanceSetupCallback;
    ULONG64     InstanceQueryTeardownCallback;
    ULONG64     InstanceTeardownStartCallback;
    ULONG64     InstanceTeardownCompleteCallback;
    ULONG64     GenerateFileNameCallback;
    ULONG64     NormalizeNameComponentCallback;
    ULONG64     NormalizeContextCleanupCallback;
    ULONG64     TransactionNotificationCallback;
    ULONG64     NormalizeNameComponentExCallback;
    ULONG64     SectionNotificationCallback;
} MYARK_FILTER_CALLBACKS;

typedef struct _MYARK_FILTER_OPERATION_ENTRY
{
    ULONG32     MajorFunction;
    ULONG32     Reserved;
    ULONG64     PreOperation;
    ULONG64     PostOperation;
} MYARK_FILTER_OPERATION_ENTRY;

typedef struct _MYARK_FILTER_ENTRY
{
    WCHAR                   Name[MYARK_MAX_MODULE_NAME];
    WCHAR                   ModuleName[MYARK_MAX_MODULE_NAME];
    WCHAR                   ModulePath[MYARK_MAX_MODULE_PATH];
    ULONG64                 FilterAddress;
    MYARK_FILTER_CALLBACKS  Callbacks;
    ULONG32                 OperationCount;
    ULONG32                 Reserved;
} MYARK_FILTER_ENTRY;

typedef struct _MYARK_FILTER_RESPONSE
{
    MYARK_RESPONSE_HEADER   Header;
    // Variable-length: after the Header come:
    //   MYARK_FILTER_ENTRY + OperationCount * MYARK_FILTER_OPERATION_ENTRY
    //   (repeated for Header.Count filters)
} MYARK_FILTER_RESPONSE;

// ============================================================
// IOCTL_MYARK_QUERY_KERNEL_OBJECT
// ============================================================
#define IOCTL_MYARK_QUERY_KERNEL_OBJECT  MYARK_CTL(0x83B)

typedef struct _MYARK_KERNEL_OBJECT_ENTRY
{
    UCHAR       Index;
    WCHAR       ObjectName[MYARK_MAX_MODULE_NAME];
    ULONG64     HandleCount;
    ULONG64     ObjectCount;
    ULONG64     ObjectType;         // POBJECT_TYPE
} MYARK_KERNEL_OBJECT_ENTRY;

typedef struct _MYARK_KERNEL_OBJECT_RESPONSE
{
    MYARK_RESPONSE_HEADER           Header;
    MYARK_KERNEL_OBJECT_ENTRY       Entries[1];
} MYARK_KERNEL_OBJECT_RESPONSE;

#define MYARK_KERNEL_OBJECT_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_KERNEL_OBJECT_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_WORK_THREADS
// Output: MYARK_WORK_THREAD_RESPONSE (ExPool / IoPool worker threads)
// ============================================================
#define IOCTL_MYARK_QUERY_WORK_THREADS  MYARK_CTL(0x83C)

typedef struct _MYARK_WORK_THREAD_ENTRY
{
    ULONG32     PoolType;           // 0 = ExPool, 1 = IoPool
    ULONG32     Reserved;
    ULONG64     Tid;
    ULONG64     StartAddress;
    CHAR        Priority;
    CHAR        BasePriority;
    UCHAR       State;
    UCHAR       Pad;
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME];
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_WORK_THREAD_ENTRY;

typedef struct _MYARK_WORK_THREAD_RESPONSE
{
    MYARK_RESPONSE_HEADER           Header;
    MYARK_WORK_THREAD_ENTRY         Entries[1];
} MYARK_WORK_THREAD_RESPONSE;

#define MYARK_WORK_THREAD_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_WORK_THREAD_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_WFP_FILTERS
// Output: MYARK_WFP_FILTER_RESPONSE
// ============================================================
#define IOCTL_MYARK_QUERY_WFP_FILTERS   MYARK_CTL(0x83D)

typedef struct _MYARK_WFP_FILTER_ENTRY
{
    ULONG64     FilterId;
    WCHAR       Name[64];
    WCHAR       Description[128];
    ULONG32     ActionType;
    ULONG32     Reserved;
    WCHAR       LayerKey[64];
    WCHAR       CalloutKey[64];
} MYARK_WFP_FILTER_ENTRY;

typedef struct _MYARK_WFP_FILTER_RESPONSE
{
    MYARK_RESPONSE_HEADER           Header;
    MYARK_WFP_FILTER_ENTRY          Entries[1];
} MYARK_WFP_FILTER_RESPONSE;

#define MYARK_WFP_FILTER_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_WFP_FILTER_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_WFP_CALLOUTS
// Output: MYARK_WFP_CALLOUT_RESPONSE
// ============================================================
#define IOCTL_MYARK_QUERY_WFP_CALLOUTS  MYARK_CTL(0x83E)

typedef struct _MYARK_WFP_CALLOUT_ENTRY
{
    ULONG32     CalloutId;
    ULONG32     Reserved;
    ULONG64     ClassifyFn;
    ULONG64     NotifyFn;
    ULONG64     FlowDeleteFn;
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME];
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_WFP_CALLOUT_ENTRY;

typedef struct _MYARK_WFP_CALLOUT_RESPONSE
{
    MYARK_RESPONSE_HEADER           Header;
    MYARK_WFP_CALLOUT_ENTRY         Entries[1];
} MYARK_WFP_CALLOUT_RESPONSE;

#define MYARK_WFP_CALLOUT_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_WFP_CALLOUT_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_UNLOADED_DRIVERS
// Input: MYARK_UNLOADED_DRIVER_REQUEST (selects the enumeration source)
// Output: MYARK_UNLOADED_DRIVER_RESPONSE
// ============================================================
#define IOCTL_MYARK_QUERY_UNLOADED_DRIVERS  MYARK_CTL(0x840)

#define MYARK_UNLOADED_SOURCE_MM       0    // MmUnloadedDrivers
#define MYARK_UNLOADED_SOURCE_PIDDB    1    // PiDDBCacheTable
#define MYARK_UNLOADED_SOURCE_CI       2    // ci.dll g_KernelHashBucketList

typedef struct _MYARK_UNLOADED_DRIVER_REQUEST
{
    ULONG       Source;         // MYARK_UNLOADED_SOURCE_*
    ULONG       Reserved;
} MYARK_UNLOADED_DRIVER_REQUEST;

typedef struct _MYARK_UNLOADED_DRIVER_ENTRY
{
    ULONG64     DriverStart;
    ULONG64     DriverEnd;
    WCHAR       DriverName[MYARK_MAX_MODULE_NAME];
} MYARK_UNLOADED_DRIVER_ENTRY;

typedef struct _MYARK_UNLOADED_DRIVER_RESPONSE
{
    MYARK_RESPONSE_HEADER           Header;
    MYARK_UNLOADED_DRIVER_ENTRY     Entries[1];
} MYARK_UNLOADED_DRIVER_RESPONSE;

#define MYARK_UNLOADED_DRIVER_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_UNLOADED_DRIVER_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_BUGCHECK_CALLBACKS
// Output: MYARK_BUGCHECK_CALLBACK_RESPONSE (KeRegisterBugCheckReasonCallback callbacks)
// ============================================================
#define IOCTL_MYARK_QUERY_BUGCHECK_CALLBACKS  MYARK_CTL(0x841)

typedef struct _MYARK_BUGCHECK_CALLBACK_ENTRY
{
    ULONG64     Callback;                       // Callback function address
    ULONG       Reason;                         // KBUGCHECK_CALLBACK_REASON
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME];
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_BUGCHECK_CALLBACK_ENTRY;

typedef struct _MYARK_BUGCHECK_CALLBACK_RESPONSE
{
    MYARK_RESPONSE_HEADER               Header;
    MYARK_BUGCHECK_CALLBACK_ENTRY       Entries[1];
} MYARK_BUGCHECK_CALLBACK_RESPONSE;

#define MYARK_BUGCHECK_CALLBACK_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_BUGCHECK_CALLBACK_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_SHUTDOWN_CALLBACKS
// Output: MYARK_SHUTDOWN_CALLBACK_RESPONSE (IoRegisterLastChanceShutdownNotification callbacks)
// ============================================================
#define IOCTL_MYARK_QUERY_SHUTDOWN_CALLBACKS  MYARK_CTL(0x842)

typedef struct _MYARK_SHUTDOWN_CALLBACK_ENTRY
{
    ULONG64     Callback;                       // Callback function address
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME];
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_SHUTDOWN_CALLBACK_ENTRY;

typedef struct _MYARK_SHUTDOWN_CALLBACK_RESPONSE
{
    MYARK_RESPONSE_HEADER               Header;
    MYARK_SHUTDOWN_CALLBACK_ENTRY       Entries[1];
} MYARK_SHUTDOWN_CALLBACK_RESPONSE;

#define MYARK_SHUTDOWN_CALLBACK_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_SHUTDOWN_CALLBACK_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_BUGCHECK_REGISTERS
// Output: MYARK_BUGCHECK_REGISTER_RESPONSE (KeRegisterBugCheckCallback callbacks)
// ============================================================
#define IOCTL_MYARK_QUERY_BUGCHECK_REGISTERS  MYARK_CTL(0x843)

typedef struct _MYARK_BUGCHECK_REGISTER_ENTRY
{
    ULONG64     Callback;                       // Callback function address
    WCHAR       Component[MYARK_MAX_MODULE_NAME];
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME];
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_BUGCHECK_REGISTER_ENTRY;

typedef struct _MYARK_BUGCHECK_REGISTER_RESPONSE
{
    MYARK_RESPONSE_HEADER               Header;
    MYARK_BUGCHECK_REGISTER_ENTRY       Entries[1];
} MYARK_BUGCHECK_REGISTER_RESPONSE;

#define MYARK_BUGCHECK_REGISTER_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_BUGCHECK_REGISTER_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_NOTIFY_SHUTDOWN
// Output: MYARK_NOTIFY_SHUTDOWN_RESPONSE (IoRegisterShutdownNotification callbacks)
// ============================================================
#define IOCTL_MYARK_QUERY_NOTIFY_SHUTDOWN  MYARK_CTL(0x844)

typedef struct _MYARK_NOTIFY_SHUTDOWN_ENTRY
{
    ULONG64     Callback;                       // MajorFunction[IRP_MJ_SHUTDOWN]
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME];
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_NOTIFY_SHUTDOWN_ENTRY;

typedef struct _MYARK_NOTIFY_SHUTDOWN_RESPONSE
{
    MYARK_RESPONSE_HEADER           Header;
    MYARK_NOTIFY_SHUTDOWN_ENTRY     Entries[1];
} MYARK_NOTIFY_SHUTDOWN_RESPONSE;

#define MYARK_NOTIFY_SHUTDOWN_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_NOTIFY_SHUTDOWN_ENTRY) * (count))

// ============================================================
// Driver detail query: input request (R3 -> R0)
// DriverObject is the PDRIVER_OBJECT address of the selected driver row
// ============================================================
typedef struct _MYARK_DRIVER_DETAIL_REQUEST
{
    ULONG64     DriverObject;
} MYARK_DRIVER_DETAIL_REQUEST;

// ============================================================
// IOCTL_MYARK_QUERY_DRIVER_DEVICES
// Input: MYARK_DRIVER_DETAIL_REQUEST
// Output: MYARK_DRIVER_DEVICE_RESPONSE (DriverObject->DeviceObject list)
// ============================================================
#define IOCTL_MYARK_QUERY_DRIVER_DEVICES  MYARK_CTL(0x845)

typedef struct _MYARK_DRIVER_DEVICE_ENTRY
{
    ULONG64     DeviceObject;       // PDEVICE_OBJECT
    ULONG64     NextDevice;         // Next device in the same driver's list
    ULONG64     AttachedDevice;     // Attached device (filter)
    ULONG32     Flags;              // DEVICE_OBJECT->Flags
    ULONG32     Characteristics;
    ULONG32     DeviceType;
    LONG        ReferenceCount;
    ULONG32     StackSize;
    ULONG32     Reserved;
    WCHAR       DeviceName[MYARK_MAX_MODULE_PATH];  // Device name in the object manager
} MYARK_DRIVER_DEVICE_ENTRY;

typedef struct _MYARK_DRIVER_DEVICE_RESPONSE
{
    MYARK_RESPONSE_HEADER           Header;
    MYARK_DRIVER_DEVICE_ENTRY       Entries[1];
} MYARK_DRIVER_DEVICE_RESPONSE;

#define MYARK_DRIVER_DEVICE_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_DRIVER_DEVICE_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_DRIVER_DISPATCH
// Input: MYARK_DRIVER_DETAIL_REQUEST
// Output: MYARK_DRIVER_DISPATCH_RESPONSE (DriverObject->MajorFunction[0..27])
// ============================================================
#define IOCTL_MYARK_QUERY_DRIVER_DISPATCH  MYARK_CTL(0x846)

#define MYARK_IRP_MJ_COUNT  28

typedef struct _MYARK_DRIVER_DISPATCH_ENTRY
{
    ULONG32     MajorFunction;      // IRP_MJ_* index
    ULONG32     Reserved;
    ULONG64     DispatchAddress;    // DriverObject->MajorFunction[index]
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME];
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_DRIVER_DISPATCH_ENTRY;

typedef struct _MYARK_DRIVER_DISPATCH_RESPONSE
{
    MYARK_RESPONSE_HEADER           Header;
    MYARK_DRIVER_DISPATCH_ENTRY     Entries[1];
} MYARK_DRIVER_DISPATCH_RESPONSE;

#define MYARK_DRIVER_DISPATCH_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_DRIVER_DISPATCH_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_OBJECT_TYPE_PROCEDURES
// Input: MYARK_OBJECT_TYPE_REQUEST (ObjectType pointer)
// Output: MYARK_OBJECT_TYPE_PROCEDURE_RESPONSE (procedures of _OBJECT_TYPE_INITIALIZER)
// ============================================================
#define IOCTL_MYARK_QUERY_OBJECT_TYPE_PROCEDURES  MYARK_CTL(0x847)

typedef struct _MYARK_OBJECT_TYPE_REQUEST
{
    ULONG64     ObjectType;
} MYARK_OBJECT_TYPE_REQUEST;

#define MYARK_OBJECT_PROCEDURE_COUNT  8

typedef struct _MYARK_OBJECT_PROCEDURE_ENTRY
{
    ULONG32     ProcedureId;        // See OBJECT_PROCEDURE_ID_*
    ULONG32     Reserved;
    ULONG64     Address;
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME];
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_OBJECT_PROCEDURE_ENTRY;

typedef struct _MYARK_OBJECT_TYPE_PROCEDURE_RESPONSE
{
    MYARK_RESPONSE_HEADER               Header;
    MYARK_OBJECT_PROCEDURE_ENTRY        Entries[1];
} MYARK_OBJECT_TYPE_PROCEDURE_RESPONSE;

#define MYARK_OBJECT_TYPE_PROCEDURE_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_OBJECT_PROCEDURE_ENTRY) * (count))

#define OBJECT_PROCEDURE_ID_DUMP           0
#define OBJECT_PROCEDURE_ID_OPEN           1
#define OBJECT_PROCEDURE_ID_CLOSE          2
#define OBJECT_PROCEDURE_ID_DELETE         3
#define OBJECT_PROCEDURE_ID_PARSE          4
#define OBJECT_PROCEDURE_ID_SECURITY       5
#define OBJECT_PROCEDURE_ID_QUERY_NAME     6
#define OBJECT_PROCEDURE_ID_OKAY_TO_CLOSE  7


typedef struct _MYARK_MEMORY_REQUEST
{
    ULONG64     Address;
    ULONG64     ReadSize;
} MYARK_MEMORY_REQUEST;

typedef struct _MYARK_MEMORY_RESPONSE
{
    MYARK_RESPONSE_HEADER           Header;
    UCHAR       Buffer[1];
} MYARK_MEMORY_RESPONSE;

#define MYARK_MEMORY_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + (count))

// ============================================================
// IOCTL_MYARK_READ_MEMORY
// Input: MYARK_MEMORY_REQUEST
// Output: MYARK_MEMORY_RESPONSE (raw bytes)
// ============================================================
#define IOCTL_MYARK_READ_MEMORY  MYARK_CTL(0x848)