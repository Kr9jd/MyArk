#pragma once
#define _NTIFS_INCLUDED_
#include <ntddk.h>
#include <intrin.h>
#include "pattern.h"
#include "protocol.h"

// ZwQuerySystemInformation
typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemModuleInformation = 11
} SYSTEM_INFORMATION_CLASS;

typedef struct _RTL_PROCESS_MODULE_INFORMATION {
	HANDLE  Section;
	PVOID   MappedBase;
	PVOID   ImageBase;
	ULONG   ImageSize;
	ULONG   Flags;
	USHORT  LoadOrderIndex;
	USHORT  InitOrderIndex;
	USHORT  LoadCount;
	USHORT  OffsetToFileName;
	UCHAR   FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES {
	ULONG NumberOfModules;
	RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

extern "C" NTSTATUS NTAPI ZwQuerySystemInformation(
	ULONG SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
);

#pragma pack(push, 8)
typedef struct _OBJECT_TYPE_INITIALIZER {
    /* +0x000 */ UCHAR Reserved1[0x030];              // padding for all irrelevant fields from +0x000 to +0x02F

    /* +0x030 */ PVOID DumpProcedure;                 // Ptr64 void
    /* +0x038 */ PVOID OpenProcedure;                 // Ptr64 long
    /* +0x040 */ PVOID CloseProcedure;                // Ptr64 void
    /* +0x048 */ PVOID DeleteProcedure;               // Ptr64 void
    /* +0x050 */ union {
        PVOID ParseProcedure;            // Ptr64 long
        PVOID ParseProcedureEx;          // Ptr64 long
    };
    /* +0x058 */ PVOID SecurityProcedure;             // Ptr64 long
    /* +0x060 */ PVOID QueryNameProcedure;            // Ptr64 long
    /* +0x068 */ PVOID OkayToCloseProcedure;          // Ptr64 unsigned char

    /* +0x070 */ UCHAR Reserved2[0x008];              // padding for the remaining trailing fields from +0x070 to +0x077
} OBJECT_TYPE_INITIALIZER, * POBJECT_TYPE_INITIALIZER;
#pragma pack(pop)

typedef struct _OBJECT_TYPE
{
    LIST_ENTRY              TypeList;                   // +0x000
    UNICODE_STRING          Name;                       // +0x010
    PVOID                   DefaultObject;              // +0x020
    UCHAR                   Index;                      // +0x028
    UCHAR                   Padding0[3];                // +0x029
    ULONG                   TotalNumberOfObjects;       // +0x02c
    ULONG                   TotalNumberOfHandles;       // +0x030
    ULONG                   HighWaterNumberOfObjects;   // +0x034
    ULONG                   HighWaterNumberOfHandles;   // +0x038
    UCHAR                   Padding1[4];                // +0x03c
    _OBJECT_TYPE_INITIALIZER TypeInfo;                   // +0x040  (size = 0x78)
    EX_PUSH_LOCK            TypeLock;                   // +0x0b8
    ULONG                   Key;                        // +0x0c0
    ULONG                   Padding2;                   // +0x0c4
    LIST_ENTRY              CallbackList;               // +0x0c8
    ULONG                   SeMandatoryLabelMask;       // +0x0d8
    ULONG                   SeTrustConstraintMask;      // +0x0dc
} OBJECT_TYPE, * POBJECT_TYPE;

struct OB_OPERATION_ENTRY  // one entry per ObjectType/Operation pair
{
    LIST_ENTRY  CallbackList;       // +0x00  self-referential list (v19->Flink = v19->Blink = v19)
    ULONG       Operations;         // +0x20  OB_OPERATION_HANDLE_CREATE etc.
    ULONG       Padding;            // +0x24
    PVOID* Registration;  // +0x28  points back to the outer structure
    POBJECT_TYPE ObjectType;        // +0x30  *ObjectType (dereferenced value)
    POB_PRE_OPERATION_CALLBACK  PreOperation;   // +0x38
    POB_POST_OPERATION_CALLBACK PostOperation;  // +0x40
    PVOID       Reserved;           // +0x48  = 0
};

struct PROCESS_CALLBACK_ENTRY
{
    PVOID Reserved;
	PVOID Callback;
    UINT64 Status;
};

struct THREAD_CALLBACK_ENTRY
{
    PVOID Reserved;
    PVOID Callback;
    UINT64 Status;
};

struct IMAGE_CALLBACK_ENTRY
{
    PVOID Reserved;
    PVOID Callback;
    UINT64 Status;
};

struct CM_CALLBACK_CONTEXT_BLOCK
{
    LIST_ENTRY  CallbackList;          // +0x00 (v11[0]=Flink, v11[1]=Blink)
    LARGE_INTEGER Cookie;              // +0x10  (v12 self-init is actually a compiler optimization)
    LARGE_INTEGER Cookie2;             // +0x18 (*a6 = v11[3] → cookie output)
    PVOID       Context;               // +0x20 (v11[4] = context)
    PVOID       Function;              // +0x28 (v11[5] = function)
    UNICODE_STRING Altitude;           // +0x30 (Length/MaxLength at +0x30, Buffer at +0x38)
    PVOID       AltitudeBuffer;        // +0x38 (v11[7] = ExAllocatePool2)
    ULONG       Flags;                 // +0x40
    ULONG       Padding[3];            // +0x44
};  // 0x50 total

typedef struct _KTIMER_TABLE_ENTRY
{
    /* +0x000 */ ULONG64          Lock;
    /* +0x008 */ LIST_ENTRY       Entry;
    /* +0x018 */ ULARGE_INTEGER   Time;
} KTIMER_TABLE_ENTRY, * PKTIMER_TABLE_ENTRY;

typedef struct _IO_TIMER
{
    INT16 Type;
    INT16 TimerFlag;
    _LIST_ENTRY TimerList;
    PVOID TimerRoutine;
    PVOID Context;
    PDEVICE_OBJECT DeviceObject;
} IO_TIMER, * PIOTIMER;

typedef struct _SYSTEM_SERVICE_TABLE {
    PLONG ServiceTableBase;
    PVOID ServiceCounterTableBase;
    ULONGLONG NumberOfServices;
    PVOID ParamTableBase;
} SYSTEM_SERVICE_TABLE, * PSYSTEM_SERVICE_TABLE;

typedef union _KIDTENTRY64
{
    struct
    {
        USHORT OffsetLow;               // +0x00
        USHORT Selector;                // +0x02
        USHORT IstIndex : 3;            // +0x04
        USHORT Reserved0 : 5;
        USHORT Type : 5;
        USHORT Dpl : 2;
        USHORT Present : 1;
        USHORT OffsetMiddle;            // +0x06
        ULONG  OffsetHigh;              // +0x08
        ULONG  Reserved1;               // +0x0c
    };
    UINT64 Alignment;
} KIDTENTRY64, *PKIDTENTRY64;

typedef struct _KINTERRUPT
{
    INT16   Type;                       // +0x000
    INT16   Size;                       // +0x002
    UCHAR   Padding0[4];                // +0x004
    LIST_ENTRY InterruptListEntry;      // +0x008
    PVOID   ServiceRoutine;             // +0x018
    PVOID   MessageServiceRoutine;      // +0x020
    ULONG   MessageIndex;               // +0x028
    ULONG   Padding1;                   // +0x02c
    PVOID   ServiceContext;             // +0x030
    ULONG64 SpinLock;                   // +0x038
    ULONG   TickCount;                  // +0x040
    ULONG   Padding2;                   // +0x044
    PULONG64 ActualLock;                // +0x048
    PVOID   DispatchAddress;            // +0x050
    ULONG   Vector;                     // +0x058
    UCHAR   Irql;                       // +0x05c
    UCHAR   SynchronizeIrql;            // +0x05d
    UCHAR   FloatingSave;               // +0x05e
    UCHAR   Connected;                  // +0x05f
    ULONG   Number;                     // +0x060
    UCHAR   ShareVector;                // +0x064
    UCHAR   EmulateActiveBoth;          // +0x065
    USHORT  ActiveCount;                // +0x066
    LONG    InternalState;              // +0x068
    ULONG   Mode;                       // +0x06c
    ULONG   Polarity;                   // +0x070
    ULONG   ServiceCount;               // +0x074
    ULONG   DispatchCount;              // +0x078
    ULONG   Padding3;                   // +0x07c
    PVOID   PassiveEvent;               // +0x080
    PVOID   TrapFrame;                  // +0x088
    PVOID   DisconnectData;             // +0x090
    PVOID   ServiceThread;              // +0x098
    PVOID   ConnectionData;             // +0x0a0
    PVOID   IntTrackEntry;              // +0x0a8
    UCHAR   IsrDpcStats[0x60];          // +0x0b0
    PVOID   RedirectObject;             // +0x110
    UCHAR   Padding[8];                 // +0x118
} KINTERRUPT, *PKINTERRUPT;

typedef struct _KTHREAD {
    /* 0x0000 */ UCHAR Header[0x18];                     // _DISPATCHER_HEADER
    /* 0x0018 */ PVOID SListFaultAddress;
    /* 0x0020 */ ULONG64 QuantumTarget;
    /* 0x0028 */ PVOID InitialStack;
    /* 0x0030 */ PVOID StackLimit;
    /* 0x0038 */ PVOID StackBase;
    /* 0x0040 */ ULONG64 ThreadLock;
    /* 0x0048 */ ULONG64 CycleTime;
    /* 0x0050 */ ULONG CurrentRunTime;
    /* 0x0054 */ ULONG ExpectedRunTime;
    /* 0x0058 */ PVOID KernelStack;
    /* 0x0060 */ PVOID StateSaveArea;
    /* 0x0068 */ PVOID SchedulingGroup;
    /* 0x0070 */ UCHAR WaitRegister;
    /* 0x0071 */ UCHAR Running;
    /* 0x0072 */ UCHAR Alerted[2];
    /* 0x0074 */ ULONG MiscFlags;                        // bitfield set
    /* 0x0078 */ ULONG ThreadFlags;                      // bitfield set
    /* 0x007c */ UCHAR Tag;
    /* 0x007d */ UCHAR SameThreadTransientFlags;
    /* 0x007e */ UCHAR SpecCtrl;
    /* 0x007f */ UCHAR Pad_7F;
    /* 0x0080 */ ULONG SystemCallNumber;
    /* 0x0084 */ ULONG ReadyTime;
    /* 0x0088 */ PVOID FirstArgument;
    /* 0x0090 */ PVOID TrapFrame;
    /* 0x0098 */ UCHAR ApcState[0x2B];                   // _KAPC_STATE
    /* 0x00c3 */ CHAR Priority;
    /* 0x00c4 */ ULONG UserIdealProcessor;
    /* 0x00c8 */ LONG64 WaitStatus;
    /* 0x00d0 */ PVOID WaitBlockList;
    /* 0x00d8 */ UCHAR WaitListEntry[0x10];              // _LIST_ENTRY / _SINGLE_LIST_ENTRY
    /* 0x00e8 */ PVOID Queue;
    /* 0x00f0 */ PVOID Teb;
    /* 0x00f8 */ ULONG64 RelativeTimerBias;
    /* 0x0100 */ UCHAR Timer[0x40];                      // _KTIMER
    /* 0x0140 */ UCHAR WaitBlockPad[0x14];               // _KWAIT_BLOCK padding
    /* 0x0154 */ ULONG ContextSwitches;
    /* 0x0158 */ UCHAR Pad_158[0x10];
    /* 0x0168 */ PVOID ThreadCounters;
    /* 0x0170 */ UCHAR Pad_170[0x14];
    /* 0x0184 */ UCHAR State;
    /* 0x0185 */ CHAR Spare13;
    /* 0x0186 */ UCHAR WaitIrql;
    /* 0x0187 */ CHAR WaitMode;
    /* 0x0188 */ UCHAR Pad_188[0x10];
    /* 0x0198 */ PVOID XStateSave;
    /* 0x01a0 */ UCHAR Pad_1A0[0x14];
    /* 0x01b4 */ ULONG WaitTime;
    /* 0x01b8 */ UCHAR Pad_1B8[0x10];
    /* 0x01c8 */ PVOID Win32Thread;
    /* 0x01d0 */ UCHAR Pad_1D0[0x14];
    /* 0x01e4 */ union {
        struct {
            SHORT KernelApcDisable;
            SHORT SpecialApcDisable;
        };
        ULONG CombinedApcDisable;
    };
    /* 0x01e8 */ UCHAR Pad_1E8[0x8];
    /* 0x01f0 */ ULONG64 Spare18;
    /* 0x01f8 */ ULONG64 LastXStateSaveDebugInfo;
    /* 0x0200 */ ULONG ThreadFlags2;
    /* 0x0204 */ UCHAR BamQosLevel;
    /* 0x0205 */ UCHAR HardwareFeedbackClass;
    /* 0x0206 */ SHORT PriorityDecrement;
    /* 0x0208 */ LIST_ENTRY QueueListEntry;             // _LIST_ENTRY
    /* 0x0218 */ ULONG NextProcessor;
    /* 0x021c */ LONG QueuePriority;
    /* 0x0220 */ PVOID Process;
    /* 0x0228 */ PVOID UserAffinity;
    /* 0x0230 */ USHORT UserAffinityPrimaryGroup;
    /* 0x0232 */ CHAR PreviousMode;
    /* 0x0233 */ CHAR BasePriority;
    /* 0x0234 */ UCHAR Spare24;
    /* 0x0235 */ UCHAR Preempted;
    /* 0x0236 */ UCHAR AdjustReason;
    /* 0x0237 */ CHAR AdjustIncrement;
    /* 0x0238 */ ULONG64 AffinityVersion;
    /* 0x0240 */ PVOID Affinity;
    /* 0x0248 */ USHORT AffinityPrimaryGroup;
    /* 0x024a */ UCHAR ApcStateIndex;
    /* 0x024b */ UCHAR WaitBlockCount;
    /* 0x024c */ ULONG IdealProcessor;
    /* 0x0250 */ ULONG64 NpxState;
    /* 0x0258 */ UCHAR SavedApcState[0x2B];              // _KAPC_STATE
    /* 0x0283 */ UCHAR WaitReason;
    /* 0x0284 */ CHAR SuspendCount;
    /* 0x0285 */ CHAR Saturation;
    /* 0x0286 */ USHORT SListFaultCount;
    /* 0x0288 */ UCHAR SchedulerApc[0x58];               // _KAPC
    /* 0x02e0 */ UCHAR SuspendEvent[0x18];               // _KEVENT
    /* 0x02f8 */ LIST_ENTRY ThreadListEntry;            // _LIST_ENTRY
    /* 0x0308 */ UCHAR MutantListHead[0x10];             // _LIST_ENTRY
    /* 0x0318 */ USHORT AbEntryCountValue;
    /* 0x031a */ UCHAR FreezeFlags;
    /* 0x031b */ CHAR WobPriority;
    /* 0x031c */ ULONG SecureThreadCookie;
    /* 0x0320 */ PVOID SchedulerSharedSystemSlot;
    /* 0x0328 */ UCHAR PropagateBoostsEntry[0x8];        // _SINGLE_LIST_ENTRY
    /* 0x0330 */ UCHAR IoSelfBoostsEntry[0x8];           // _SINGLE_LIST_ENTRY
    /* 0x0338 */ UCHAR PriorityFloorCounts[32];
    /* 0x0358 */ ULONG PriorityFloorSummary;
    /* 0x035c */ LONG AbCompletedIoBoostCount;
    /* 0x0360 */ LONG AbCompletedIoQoSBoostCount;
    /* 0x0364 */ SHORT KeReferenceCount;
    /* 0x0366 */ CHAR DecayBoost;
    /* 0x0367 */ UCHAR Spare6;
    /* 0x0368 */ ULONG ForegroundLossTime;
    /* 0x0370 */ union {
        LIST_ENTRY GlobalForegroundListEntry;           // _LIST_ENTRY
        struct {
            UCHAR ForegroundDpcStackListEntry[0x8];     // _SINGLE_LIST_ENTRY
            ULONG64 InGlobalForegroundList;
        };
    };
    /* 0x0380 */ LONG64 ReadOperationCount;
    /* 0x0388 */ LONG64 WriteOperationCount;
    /* 0x0390 */ LONG64 OtherOperationCount;
    /* 0x0398 */ LONG64 ReadTransferCount;
    /* 0x03a0 */ LONG64 WriteTransferCount;
    /* 0x03a8 */ LONG64 OtherTransferCount;
    /* 0x03b0 */ PVOID QueuedScb;
    /* 0x03b8 */ ULONG ThreadTimerDelay;
    /* 0x03bc */ USHORT Spare26;
    /* 0x03be */ UCHAR PpmPolicy;
    /* 0x03bf */ UCHAR Spare27;
    /* 0x03c0 */ ULONG64 TracingPrivate[1];
    /* 0x03c8 */ PVOID SchedulerAssist;
    /* 0x03d0 */ PVOID AbWaitObject;
    /* 0x03d8 */ ULONG ReservedPreviousReadyTimeValue;
    /* 0x03dc */ UCHAR Pad_3DC[0x4];
    /* 0x03e0 */ ULONG64 KernelWaitTime;
    /* 0x03e8 */ ULONG64 UserWaitTime;
    /* 0x03f0 */ union {
        LIST_ENTRY GlobalUpdateVpThreadPriorityListEntry;   // _LIST_ENTRY
        struct {
            UCHAR UpdateVpThreadPriorityDpcStackListEntry[0x8]; // _SINGLE_LIST_ENTRY
            ULONG64 InGlobalUpdateVpThreadPriorityList;
        };
    };
    /* 0x0400 */ LONG SchedulerAssistPriorityFloor;
    /* 0x0404 */ LONG RealtimePriorityFloor;
    /* 0x0408 */ PVOID KernelShadowStack;
    /* 0x0410 */ PVOID KernelShadowStackInitial;
    /* 0x0418 */ PVOID KernelShadowStackBase;
    /* 0x0420 */ UCHAR KernelShadowStackLimit[0x8];      // _KERNEL_SHADOW_STACK_LIMIT
    /* 0x0428 */ ULONG64 ExtendedFeatureDisableMask;
    /* 0x0430 */ ULONG64 HgsFeedbackStartTime;
    /* 0x0438 */ ULONG64 HgsFeedbackCycles;
    /* 0x0440 */ ULONG HgsInvalidFeedbackCount;
    /* 0x0444 */ ULONG HgsLowerPerfClassFeedbackCount;
    /* 0x0448 */ ULONG HgsHigherPerfClassFeedbackCount;
    /* 0x044c */ ULONG ModeHistory;
    /* 0x0450 */ UCHAR SystemAffinityTokenListHead[0x8]; // _SINGLE_LIST_ENTRY
    /* 0x0458 */ PVOID IptSaveArea;
    /* 0x0460 */ UCHAR ResourceIndex;
    /* 0x0461 */ UCHAR CoreIsolationReasons;
    /* 0x0462 */ UCHAR BamQosLevelFromAssistPage;
    /* 0x0463 */ UCHAR SecureCallCoreIsolationCount;
    /* 0x0464 */ ULONG SchedulerSharedOffset;
    /* 0x0468 */ PVOID SchedulerSharedSwappablePage;
    /* 0x0470 */ PVOID KernelAbEntries;
    /* 0x0478 */ PVOID UserAbEntries;
    /* 0x0480 */ ULONG64 KcsanThread;
    /* 0x0488 */ ULONG SchedulerAssistYieldCounter;
    /* 0x048c */ ULONG SchedulerAssistYieldBoostCount;
    /* 0x0490 */ LONG64 SchedulerAssistLastYieldBoostTime;
    /* 0x0498 */ ULONG64 Padding[5];
} KTHREAD;

// ---------------------------------------------------------------------------
// Simplified _ETHREAD structure
// ---------------------------------------------------------------------------
typedef struct _ETHREAD {
    /* 0x0000 */ KTHREAD Tcb;                            // _KTHREAD occupies the first 0x4C0 bytes
    /* 0x04c0 */ ULONG64 CreateTime;                     // _LARGE_INTEGER
    /* 0x04c8 */ ULONG64 ExitTime;                       // _LARGE_INTEGER / KeyedWaitChain
    /* 0x04d0 */ UCHAR Pad_4D0[0x10];                    // PostBlockList / ForwardLinkShadow padding
    /* 0x04e0 */ PVOID StartAddress;
    /* 0x04e8 */ PVOID KeyedWaitValue;                   // TerminationPort / ReaperLink / KeyedWaitValue
    /* 0x04f0 */ ULONG64 ActiveTimerListLock;
    /* 0x04f8 */ UCHAR ActiveTimerListHead[0x10];        // _LIST_ENTRY
    /* 0x0508 */ ULONG64 UniqueProcessId;                // _CLIENT_ID.UniqueProcess
    /* 0x0510 */ ULONG64 UniqueThreadId;                 // _CLIENT_ID.UniqueThread
    /* 0x0518 */ UCHAR KeyedWaitSemaphore[0x20];         // _KSEMAPHORE / AlpcWaitSemaphore
    /* 0x0538 */ UCHAR ClientSecurity[0x8];              // _PS_CLIENT_SECURITY_CONTEXT
    /* 0x0540 */ UCHAR IrpList[0x10];                    // _LIST_ENTRY
    /* 0x0550 */ ULONG64 TopLevelIrp;
    /* 0x0558 */ PVOID DeviceToVerify;
    /* 0x0560 */ PVOID Win32StartAddress;
    /* 0x0568 */ PVOID ChargeOnlySession;
    /* 0x0570 */ PVOID LegacyPowerObject;
    /* 0x0578 */ LIST_ENTRY ThreadListEntry;            // _LIST_ENTRY
    /* 0x0588 */ ULONG64 RundownProtect;                 // _EX_RUNDOWN_REF
    /* 0x0590 */ ULONG64 ThreadLock;                     // _EX_PUSH_LOCK
    /* 0x0598 */ ULONG ReadClusterSize;
    /* 0x059c */ ULONG MmLockOrdering;
    /* 0x05a0 */ ULONG CrossThreadFlags;                 // bitfield flags
    /* 0x05a4 */ ULONG SameThreadPassiveFlags;           // bitfield flags
    /* 0x05a8 */ ULONG SameThreadApcFlags;               // bitfield flags
    /* 0x05ac */ UCHAR CacheManagerActive;
    /* 0x05ad */ UCHAR DisablePageFaultClustering;
    /* 0x05ae */ UCHAR ActiveFaultCount;
    /* 0x05af */ UCHAR LockOrderState;
    /* 0x05b0 */ ULONG SharedPsModuleLockAcquires;
    /* 0x05b4 */ ULONG MmReserved;
    /* 0x05b8 */ ULONG64 AlpcMessageId;
    /* 0x05c0 */ PVOID AlpcMessage;
    /* 0x05c8 */ UCHAR AlpcWaitListEntry[0x10];          // _LIST_ENTRY
    /* 0x05d8 */ LONG ExitStatus;
    /* 0x05dc */ ULONG CacheManagerCount;
    /* 0x05e0 */ ULONG IoBoostCount;
    /* 0x05e4 */ ULONG IoQoSBoostCount;
    /* 0x05e8 */ ULONG IoQoSThrottleCount;
    /* 0x05ec */ ULONG KernelStackReference;
    /* 0x05f0 */ UCHAR BoostList[0x10];                  // _LIST_ENTRY
    /* 0x0600 */ UCHAR DeboostList[0x10];                // _LIST_ENTRY
    /* 0x0610 */ ULONG64 BoostListLock;
    /* 0x0618 */ ULONG64 IrpListLock;
    /* 0x0620 */ PVOID ReservedForSynchTracking;
    /* 0x0628 */ UCHAR CmCallbackListHead[0x8];          // _SINGLE_LIST_ENTRY
    /* 0x0630 */ PVOID ActivityId;
    /* 0x0638 */ UCHAR SeLearningModeListHead[0x8];      // _SINGLE_LIST_ENTRY
    /* 0x0640 */ PVOID VerifierContext;
    /* 0x0648 */ PVOID AdjustedClientToken;
    /* 0x0650 */ PVOID WorkOnBehalfThread;
    /* 0x0658 */ UCHAR PropertySet[0x18];                // _PS_PROPERTY_SET
    /* 0x0670 */ PVOID PicoContext;
    /* 0x0678 */ ULONG64 UserFsBase;
    /* 0x0680 */ ULONG64 UserGsBase;
    /* 0x0688 */ PVOID EnergyValues;
    /* 0x0690 */ ULONG64 SelectedCpuSets;
    /* 0x0698 */ PVOID Silo;
    /* 0x06a0 */ PVOID ThreadName;                       // PUNICODE_STRING
    /* 0x06a8 */ PVOID SetContextState;
    /* 0x06b0 */ PVOID EtwSupport;
    /* 0x06b8 */ UCHAR OwnerEntryListHead[0x10];         // _LIST_ENTRY
    /* 0x06c8 */ ULONG64 DisownedOwnerEntryListLock;
    /* 0x06d0 */ UCHAR DisownedOwnerEntryListHead[0x10]; // _LIST_ENTRY
    /* 0x06e0 */ PVOID SchedulerSharedDataObject;
    /* 0x06e8 */ PVOID CmThreadInfo;
    /* 0x06f0 */ PVOID FlsData;
    /* 0x06f8 */ ULONG LastExpectedRunTime;
    /* 0x06fc */ ULONG LastSoftParkElectionRunTime;
    /* 0x0700 */ ULONG64 LastSoftParkElectionGeneration;
    /* 0x0708 */ UCHAR LastSoftParkElectionGroupAffinity[0x10]; // _GROUP_AFFINITY
    /* 0x0718 */ ULONG64 UserIsolationDomain;
    /* 0x0720 */ UCHAR UpdateTebApc[0x54];               // _KAPC union block (0x0720 ~ 0x0774)
    /* 0x0774 */ ULONG HeapData;
    /* 0x0778 */ ULONG64 Win32kThreadLock;
    /* 0x0780 */ PVOID ThreadIndex;
    /* 0x0788 */ PVOID MmFaultClusterContext;
    /* 0x0790 */ ULONG64 ProcessFastRef;                 // _EX_FAST_REF
} ETHREAD;


typedef struct _EPARTITION {
    /* 0x000 */ PVOID MmPartition;
    /* 0x008 */ PVOID CcPartition;
    /* 0x010 */ PVOID ExPartition;
    /* 0x018 */ PVOID SmPartition;
    /* 0x020 */ PVOID PfPartition;
    /* 0x028 */ INT64 HardReferenceCount;
    /* 0x030 */ INT64 OpenHandleCount;
    /* 0x038 */ LIST_ENTRY ActivePartitionLinks;
    /* 0x048 */ struct _EPARTITION* ParentPartition;
    /* 0x050 */ WORK_QUEUE_ITEM TeardownWorkItem;
    /* 0x070 */ EX_PUSH_LOCK TeardownLock;
    /* 0x078 */ struct _EPROCESS* SystemProcess;
    /* 0x080 */ PVOID SystemProcessHandle;
    /* 0x088 */ union {
        /* 0x088 */     ULONG PartitionFlags;
        /* 0x088 */     struct {
            /* 0x088 */         ULONG PairedWithJob : 1;
            ULONG Reserved : 31;
        };
    };
} EPARTITION, * PEPARTITION;

typedef struct _EX_PARTITION {
    /* 0x000 */ struct _EPARTITION* PartitionObject;
    /* 0x008 */ struct _EX_WORK_QUEUE*** WorkQueues;
    /* 0x010 */ struct _EX_WORK_QUEUE_MANAGER** WorkQueueManagers;
    /* 0x018 */ INT32 QueueAllocationMask;
} EX_PARTITION, * PEX_PARTITION;

typedef struct _KPRIQUEUE {
    /* 0x000 */ DISPATCHER_HEADER Header;
    /* 0x018 */ LIST_ENTRY EntryListHead[32];
    /* 0x218 */ LONG CurrentCount[32];
    /* 0x298 */ ULONG MaximumCount;
    /* 0x2a0 */ LIST_ENTRY ThreadListHead;
} KPRIQUEUE, * PKPRIQUEUE;

typedef struct _EX_WORK_QUEUE {
    /* 0x000 */ KPRIQUEUE WorkPriQueue;
    /* 0x2b0 */ struct _EX_PARTITION* Partition;
    /* 0x2b8 */ UCHAR Node[8];
    /* 0x2c0 */ ULONG WorkItemsProcessed;
    /* 0x2c4 */ ULONG WorkItemsProcessedLastPass;
    /* 0x2c8 */ LONG ThreadCount;
    /* 0x2cc */ union {
        struct {
            /* 0x2cc */             ULONG MinThreads : 31;
            /* 0x2cc */             ULONG TryFailed : 1;
        };
    };
    /* 0x2d0 */ LONG MaxThreads;
    /* 0x2d4 */ UCHAR QueueIndex[4];
    /* 0x2d8 */ PKEVENT AllThreadsExitedEvent;
} EX_WORK_QUEUE, * PEX_WORK_QUEUE;

typedef struct _KLDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;            // +0x000
    PVOID ExceptionTable;                   // +0x010
    ULONG ExceptionTableSize;               // +0x018
    PVOID GpValue;                          // +0x020
    UCHAR NonPagedDebugInfo[8];             // +0x028 (8-byte pointer slot on x64)
    PVOID DllBase;                          // +0x030
    PVOID EntryPoint;                       // +0x038
    ULONG SizeOfImage;                      // +0x040
    UNICODE_STRING FullDllName;             // +0x048
    UNICODE_STRING BaseDllName;             // +0x058
    ULONG Flags;                            // +0x068
    USHORT LoadCount;                       // +0x06c
    union {
        USHORT u1;                          // +0x06e
    };
    PVOID SectionPointer;                   // +0x070
    ULONG CheckSum;                         // +0x078
    ULONG CoverageSectionSize;              // +0x07c
    PVOID CoverageSection;                  // +0x080
    PVOID LoadedImports;                    // +0x088
    union {                                 // +0x090
        PVOID Spare;
        struct _KLDR_DATA_TABLE_ENTRY* NtDataTableEntry;
    };
    ULONG SizeOfImageNotRounded;            // +0x098
    ULONG TimeDateStamp;                    // +0x09c
} KLDR_DATA_TABLE_ENTRY, * PKLDR_DATA_TABLE_ENTRY;

typedef struct _CUSTOM_CALLOUT_ENTRY {
    /* 0x00 (0)  */ ULONG  Type;           // *(_DWORD *)v14 = v12
    /* 0x04 (4)  */ ULONG  State;          // *(_DWORD *)(v14 + 4) = 1
    /* 0x08 (8)  */ UCHAR   Reserved1[8];   // padding for alignment (0x08 - 0x0F)
    /* 0x10 (16) */ PVOID ClassifyFn1;    // *(_QWORD *)(v14 + 16) = classifyFn (when v12 != 3)
    /* 0x18 (24) */ PVOID NotifyFn;       // *(_QWORD *)(v14 + 24) = notifyFn
    /* 0x20 (32) */ PVOID FlowDeleteFn;   // *(_QWORD *)(v14 + 32) = flowDeleteFn
    /* 0x28 (40) */ PVOID ClassifyFn2;    // *(_QWORD *)(v14 + 40) = classifyFn (when v12 == 3, e.g. ClassifyFnV2)
    /* 0x30 (48) */ ULONG  Flags;          // *(_DWORD *)(v14 + 48) = flags
    /* 0x34 (52) */ UCHAR   Reserved2[20];  // padding for alignment (0x34 - 0x47)
    /* 0x40 (64) */ PDEVICE_OBJECT DeviceObject; // *(_QWORD *)(v14 + 64) = deviceObject
    /* 0x48 (72) */ UCHAR   Reserved3[6];   // padding (0x48 - 0x4D)
    /* 0x4E (78) */ UCHAR   AttributeA6;    // *(_BYTE *)(v14 + 78) = a6
} CUSTOM_CALLOUT_ENTRY, * PCUSTOM_CALLOUT_ENTRY;

typedef struct _CALLOUT_DATA {
    GUID      calloutKey;   // offset: 0x00 (size: 16 bytes)
    ULONG     index;        // offset: 0x10 (corresponds to *((_DWORD *)v6 + 4))
    UCHAR     padding[12];
    ULONG     flags;        // offset: 0x20 (corresponds to *((_DWORD *)v6 + 8))
    UCHAR     padding1[4];
} CALLOUT_DATA, * PCALLOUT_DATA;

typedef struct _OBJECT_HEADER
{
    INT64 PointerCount;                                                     // 0x00
    union
    {
        INT64 HandleCount;                                                  // 0x08
        VOID* NextToFree;                                                   // 0x08
    };
    ULONG_PTR Lock;                                                         // 0x10
    UCHAR TypeIndex;                                                        // 0x18
    union
    {
        UCHAR TraceFlags;                                                   // 0x19
        struct
        {
            UCHAR DbgRefTrace : 1;                                            // 0x19
            UCHAR DbgTracePermanent : 1;                                      // 0x19
        };
    };
    UCHAR InfoMask;                                                         // 0x1a
    union
    {
        UCHAR Flags;                                                        // 0x1b
        struct
        {
            UCHAR NewObject : 1;                                              // 0x1b
            UCHAR KernelObject : 1;                                           // 0x1b
            UCHAR KernelOnlyAccess : 1;                                       // 0x1b
            UCHAR ExclusiveObject : 1;                                        // 0x1b
            UCHAR PermanentObject : 1;                                        // 0x1b
            UCHAR DefaultSecurityQuota : 1;                                   // 0x1b
            UCHAR SingleHandleEntry : 1;                                      // 0x1b
            UCHAR DeletedInline : 1;                                          // 0x1b
        };
    };
    ULONG Reserved;                                                         // 0x1c
    union
    {
        VOID* ObjectCreateInfo;                                             // 0x20
        VOID* QuotaBlockCharged;                                            // 0x20
    };
    VOID* SecurityDescriptor;                                               // 0x28
    QUAD Body;                                                              // 0x30
} OBJECT_HEADER, * POBJECT_HEADER;

typedef struct _OBJECT_DIRECTORY_ENTRY
{
    struct _OBJECT_DIRECTORY_ENTRY* ChainLink;                             // 0x00
    VOID* Object;                                                           // 0x08
    ULONG HashValue;                                                        // 0x10
} OBJECT_DIRECTORY_ENTRY, * POBJECT_DIRECTORY_ENTRY;

typedef struct _OBJECT_DIRECTORY
{
    POBJECT_DIRECTORY_ENTRY HashBuckets[37];                                // 0x000
    ULONG_PTR Lock;                                                         // 0x128
    VOID* DeviceMap;                                                        // 0x130
    struct _OBJECT_DIRECTORY* ShadowDirectory;                              // 0x138
    VOID* NamespaceEntry;                                                   // 0x140
    VOID* SessionObject;                                                    // 0x148
    ULONG Flags;                                                            // 0x150
    ULONG SessionId;                                                        // 0x154
} OBJECT_DIRECTORY, * POBJECT_DIRECTORY;

typedef struct _OBJECT_HEADER_NAME_INFO
{
    POBJECT_DIRECTORY Directory;                                            // 0x000
    UNICODE_STRING Name;                                                    // 0x008
    LONG ReferenceCount;                                                    // 0x018
    ULONG Reserved;                                                         // 0x01c
} OBJECT_HEADER_NAME_INFO, * POBJECT_HEADER_NAME_INFO;

typedef struct _LASTCHANCE_SHUTDOWN_ENTRY
{
    LIST_ENTRY Entry;
    _DEVICE_OBJECT* Device;
}LASTCHANCE_SHUTDOWN_ENTRY,*PLASTCHANCE_SHUTDOWN_ENTRY;

// PiDDB (Driver Database) cache entry
typedef struct _PiDDBCacheEntry
{
    LIST_ENTRY      List;               // +0x00
    UNICODE_STRING  DriverName;         // +0x10
    LARGE_INTEGER   TimeDateStamp;      // +0x20
    NTSTATUS        LoadStatus;         // +0x28
    ULONG           Reserved;           // +0x2c
    ULONGLONG       LoadId;             // +0x30
} PiDDBCacheEntry, * PPiDDBCacheEntry;
