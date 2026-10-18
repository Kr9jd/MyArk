#pragma once
#include <fltkernel.h>

NTSTATUS FilterQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);

typedef struct _FLT_FILTER {
    UCHAR Base[0x038];                                  // +0x000 (原 _FLT_OBJECT)
    PVOID Frame;                                        // +0x038 (原 Ptr64 _FLTP_FRAME)
    UNICODE_STRING Name;                                // +0x040
    UNICODE_STRING DefaultAltitude;                     // +0x050
    UCHAR Flags[0x008];                                 // +0x060 (原 _FLT_FILTER_FLAGS)
    PDRIVER_OBJECT DriverObject;                        // +0x068
    UCHAR InstanceList[0x080];                          // +0x070 (原 _FLT_RESOURCE_LIST_HEAD)
    PVOID VerifierExtension;                            // +0x0f0 (原 Ptr64 _FLT_VERIFIER_EXTENSION)
    LIST_ENTRY VerifiedFiltersLink;                     // +0x0f8
    PVOID FilterUnload;                                 // +0x108
    PVOID InstanceSetup;                                // +0x110
    PVOID InstanceQueryTeardown;                        // +0x118
    PVOID InstanceTeardownStart;                        // +0x120
    PVOID InstanceTeardownComplete;                     // +0x128
    PVOID SupportedContextsListHead;                    // +0x130 (原 Ptr64 _ALLOCATE_CONTEXT_HEADER)
    PVOID SupportedContexts[7];                         // +0x138 (原 [7] Ptr64 _ALLOCATE_CONTEXT_HEADER)
    PVOID PreVolumeMount;                               // +0x170
    PVOID PostVolumeMount;                              // +0x178
    PVOID GenerateFileName;                             // +0x180
    PVOID NormalizeNameComponent;                       // +0x188
    PVOID NormalizeNameComponentEx;                     // +0x190
    PVOID NormalizeContextCleanup;                      // +0x198
    PVOID KtmNotification;                              // +0x1a0
    PVOID SectionNotification;                          // +0x1a8
    PFLT_OPERATION_REGISTRATION Operations;             // +0x1b0
    PVOID OldDriverUnload;                              // +0x1b8
    UCHAR ActiveOpens[0x050];                           // +0x1c0 (原 _FLT_MUTEX_LIST_HEAD)
    UCHAR ConnectionList[0x050];                        // +0x210 (原 _FLT_MUTEX_LIST_HEAD)
    UCHAR PortList[0x050];                              // +0x260 (原 _FLT_MUTEX_LIST_HEAD)
    UCHAR PortLock[0x030];                              // +0x2b0 (原 _EX_PUSH_LOCK_AUTO_EXPAND)
} FLT_FILTER, * PFLT_FILTER;