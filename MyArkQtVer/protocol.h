#pragma once

// ============================================================
// MyArk R0 <-> R3 通信协议
// 驱动和用户态程序都包含此文件
// ============================================================

// 确保基础类型已定义
#ifdef _NTDDK_
// 内核模式：类型已由 wdm.h/ntddk.h 定义

#else
// 用户模式：使用 windows.h 的类型
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

// IOCTL 代码
#define IOCTL_MYARK_QUERY_PROCESS       MYARK_CTL(0x830)  // 枚举进程 ObCallback
#define IOCTL_MYARK_QUERY_OB_CALLBACKS  MYARK_CTL(0x831)  // 枚举 ObRegisterCallbacks
#define IOCTL_MYARK_QUERY_DRIVER        MYARK_CTL(0x832)  // 枚举加载的驱动模块
#define IOCTL_MYARK_QUERY_PROCERSS_CALLBACKS  MYARK_CTL(0x833)
#define IOCTL_MYARK_QUERY_REG_CALLBACKS  MYARK_CTL(0x834)

// ============================================================
// 通用响应头 — 所有输出结构体的第一个字段
// ============================================================
#define MYARK_PROTOCOL_VERSION  3

typedef struct _MYARK_RESPONSE_HEADER
{
    ULONG   Status;           // 0 = 成功，其他为错误码
    ULONG   Count;            // 返回的条目数量
    ULONG   TotalSize;        // 本次返回的数据总字节数
    ULONG   ProtocolVersion;  // 协议版本，R3 校验，不匹配请重新加载驱动
} MYARK_RESPONSE_HEADER;

// ============================================================
// IOCTL_MYARK_QUERY_OB_CALLBACKS
// 输入：无 (InputBufferLength = 0)
// 输出：MYARK_OB_CALLBACK_RESPONSE
// ============================================================
#define MYARK_MAX_MODULE_NAME   64
#define MYARK_MAX_MODULE_PATH   260

typedef struct _MYARK_OB_CALLBACK_ENTRY
{
    ULONG64     PreOperation;               // 回调函数地址
    ULONG64     PostOperation;              // 回调函数地址
    ULONG64     RegistrationHandle;         // ObUnRegisterCallbacks 句柄
    ULONG       Operations;                 // OB_OPERATION_HANDLE_CREATE 等
    ULONG       Type;                       // 0 = Process, 1 = Thread
    WCHAR       ModuleName[MYARK_MAX_MODULE_NAME]; // 所属模块名（从地址解析）
    WCHAR       ModulePath[MYARK_MAX_MODULE_PATH];
} MYARK_OB_CALLBACK_ENTRY;

typedef struct _MYARK_OB_CALLBACK_RESPONSE
{
    MYARK_RESPONSE_HEADER   Header;
    MYARK_OB_CALLBACK_ENTRY Entries[1]; // 变长数组，实际大小 = Header.Count
} MYARK_OB_CALLBACK_RESPONSE;

// ============================================================
// IOCTL_MYARK_QUERY_DRIVER
// 输入：无
// 输出：MYARK_DRIVER_RESPONSE
// ============================================================
typedef struct _MYARK_DRIVER_ENTRY
{
    ULONG64     DriverObject;           // 未公开：PDRIVER_OBJECT（预留）
    ULONG64     BaseAddress;            // DllBase
    ULONG       Size;                   // SizeOfImage
    ULONG       Reserved;
    WCHAR       Name[MYARK_MAX_MODULE_NAME];   // 驱动名（BaseDllName）
    WCHAR       ObjectName[MYARK_MAX_MODULE_NAME]; // 未公开：对象名（预留）
    WCHAR       Path[260];              // FullDllName 完整路径
} MYARK_DRIVER_ENTRY;

typedef struct _MYARK_DRIVER_RESPONSE
{
    MYARK_RESPONSE_HEADER   Header;
    MYARK_DRIVER_ENTRY      Entries[1];
} MYARK_DRIVER_RESPONSE;

// ============================================================
// 计算可变长响应所需缓冲区大小
// ============================================================
#define MYARK_OB_CALLBACK_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_OB_CALLBACK_ENTRY) * (count))

#define MYARK_DRIVER_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + sizeof(MYARK_DRIVER_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_PROCERSS_CALLBACKS
// 输入：无
// 输出：MYARK_PROCESS_CALLBACK_RESPONSE
// ============================================================
typedef struct _MYARK_PROCESS_CALLBACK_ENTRY
{
    ULONG64     Callback;                       // 回调函数地址
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
    ULONG64     Callback;                       // 回调函数地址
    WCHAR       Altitude[32];                   // 海拔字符串
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
// DPC Timer 与 IO Timer 合并返回
// ============================================================
#define IOCTL_MYARK_QUERY_TIMERS     MYARK_CTL(0x835)

typedef struct _MYARK_TIMER_ENTRY
{
    ULONG64     Type;               // 0 = DPC Timer, 1 = IO Timer
    ULONG64     TimerAddress;       // KTIMER* 或 IO_TIMER*
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
// 输入：无
// 输出：MYARK_SSDT_RESPONSE（含导出表解析出的函数名）
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
    ULONG64                 ModuleBase;         // 服务函数所在模块的内核基址（用于 R3 符号解析）
    ULONG64                 SelfTestAddress;    // 模块内已知导出函数运行时地址（KeSetTimerEx），用于校验符号映射
    MYARK_SSDT_ENTRY        Entries[1];
} MYARK_SSDT_RESPONSE;

#define MYARK_SSDT_RESPONSE_SIZE(count) \
    (sizeof(MYARK_RESPONSE_HEADER) + 16 + sizeof(MYARK_SSDT_ENTRY) * (count))

// ============================================================
// IOCTL_MYARK_QUERY_SHADOW_SSDT
// 输出：MYARK_SSDT_RESPONSE（win32k 服务表，NtUserXxx/NtGdiXxx）
// ============================================================
#define IOCTL_MYARK_QUERY_SHADOW_SSDT  MYARK_CTL(0x837)

// ============================================================
// IOCTL_MYARK_QUERY_IDT
// 输出：MYARK_IDT_RESPONSE（按 CPU 分组的 IDT 表项）
// ============================================================
#define IOCTL_MYARK_QUERY_IDT          MYARK_CTL(0x838)

typedef struct _MYARK_IDT_ENTRY
{
    ULONG32     Cpu;
    ULONG32     Index;
    USHORT      Selector;
    USHORT      Dpl;
    ULONG64     Address;
    ULONG64     IsrAddress[4];      // 处理函数地址（独占 1 个；共享最多存前 4 个）
    ULONG32     IsrCount;           // 真实处理函数数量（共享环上对象数，可能 >4）
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
// 输出：MYARK_SYSTEM_THREAD_RESPONSE（System 进程线程链表）
// ============================================================
#define IOCTL_MYARK_QUERY_SYSTEM_THREADS  MYARK_CTL(0x839)

typedef struct _MYARK_SYSTEM_THREAD_ENTRY
{
    ULONG64     Tid;
    ULONG64     StartAddress;       // 内核入口（PspSystemThreadStartup 等）
    ULONG64     Win32StartAddress;  // 用户态入口（空 = 系统线程）
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
// 输出：MYARK_FILTER_RESPONSE（变长：每个 filter 一个 ENTRY + OperationCount 个 OP 条目）
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
    // 变长：Header 后依次是：
    //   MYARK_FILTER_ENTRY + OperationCount * MYARK_FILTER_OPERATION_ENTRY
    //   （按 Header.Count 个 filter 重复）
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
// 输出：MYARK_WORK_THREAD_RESPONSE（ExPool / IoPool 工作线程）
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
// 输出：MYARK_WFP_FILTER_RESPONSE
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
// 输出：MYARK_WFP_CALLOUT_RESPONSE
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
// 输入：MYARK_UNLOADED_DRIVER_REQUEST（选择枚举来源）
// 输出：MYARK_UNLOADED_DRIVER_RESPONSE
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
// 输出：MYARK_BUGCHECK_CALLBACK_RESPONSE（KeRegisterBugCheckReasonCallback 回调）
// ============================================================
#define IOCTL_MYARK_QUERY_BUGCHECK_CALLBACKS  MYARK_CTL(0x841)

typedef struct _MYARK_BUGCHECK_CALLBACK_ENTRY
{
    ULONG64     Callback;                       // 回调函数地址
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
// 输出：MYARK_SHUTDOWN_CALLBACK_RESPONSE（IoRegisterLastChanceShutdownNotification 回调）
// ============================================================
#define IOCTL_MYARK_QUERY_SHUTDOWN_CALLBACKS  MYARK_CTL(0x842)

typedef struct _MYARK_SHUTDOWN_CALLBACK_ENTRY
{
    ULONG64     Callback;                       // 回调函数地址
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
// 输出：MYARK_BUGCHECK_REGISTER_RESPONSE（KeRegisterBugCheckCallback 回调）
// ============================================================
#define IOCTL_MYARK_QUERY_BUGCHECK_REGISTERS  MYARK_CTL(0x843)

typedef struct _MYARK_BUGCHECK_REGISTER_ENTRY
{
    ULONG64     Callback;                       // 回调函数地址
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
// 输出：MYARK_NOTIFY_SHUTDOWN_RESPONSE（IoRegisterShutdownNotification 回调）
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
// 驱动详情查询：输入请求（R3 -> R0）
// DriverObject 为选中驱动行的 PDRIVER_OBJECT 地址
// ============================================================
typedef struct _MYARK_DRIVER_DETAIL_REQUEST
{
    ULONG64     DriverObject;
} MYARK_DRIVER_DETAIL_REQUEST;

// ============================================================
// IOCTL_MYARK_QUERY_DRIVER_DEVICES
// 输入：MYARK_DRIVER_DETAIL_REQUEST
// 输出：MYARK_DRIVER_DEVICE_RESPONSE（DriverObject->DeviceObject 链表）
// ============================================================
#define IOCTL_MYARK_QUERY_DRIVER_DEVICES  MYARK_CTL(0x845)

typedef struct _MYARK_DRIVER_DEVICE_ENTRY
{
    ULONG64     DeviceObject;       // PDEVICE_OBJECT
    ULONG64     NextDevice;         // 同驱动设备链表下一个
    ULONG64     AttachedDevice;     // 附加设备（filter）
    ULONG32     Flags;              // DEVICE_OBJECT->Flags
    ULONG32     Characteristics;
    ULONG32     DeviceType;
    LONG        ReferenceCount;
    ULONG32     StackSize;
    ULONG32     Reserved;
    WCHAR       DeviceName[MYARK_MAX_MODULE_PATH];  // 对象管理器中的设备名
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
// 输入：MYARK_DRIVER_DETAIL_REQUEST
// 输出：MYARK_DRIVER_DISPATCH_RESPONSE（DriverObject->MajorFunction[0..27]）
// ============================================================
#define IOCTL_MYARK_QUERY_DRIVER_DISPATCH  MYARK_CTL(0x846)

#define MYARK_IRP_MJ_COUNT  28

typedef struct _MYARK_DRIVER_DISPATCH_ENTRY
{
    ULONG32     MajorFunction;      // IRP_MJ_* 索引
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
// 输入：MYARK_OBJECT_TYPE_REQUEST（ObjectType 指针）
// 输出：MYARK_OBJECT_TYPE_PROCEDURE_RESPONSE（_OBJECT_TYPE_INITIALIZER 的 Procedure）
// ============================================================
#define IOCTL_MYARK_QUERY_OBJECT_TYPE_PROCEDURES  MYARK_CTL(0x847)

typedef struct _MYARK_OBJECT_TYPE_REQUEST
{
    ULONG64     ObjectType;
} MYARK_OBJECT_TYPE_REQUEST;

#define MYARK_OBJECT_PROCEDURE_COUNT  8

typedef struct _MYARK_OBJECT_PROCEDURE_ENTRY
{
    ULONG32     ProcedureId;        // 见 OBJECT_PROCEDURE_ID_*
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
// 输入：MYARK_MEMORY_REQUEST
// 输出：MYARK_MEMORY_RESPONSE（原始字节）
// ============================================================
#define IOCTL_MYARK_READ_MEMORY  MYARK_CTL(0x848)