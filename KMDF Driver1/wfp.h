#pragma once
#include <ntddk.h>
#include <ntstrsafe.h>
#define NDIS630 1
#include <fwpsk.h>
#include <fwpmk.h>

extern PDRIVER_OBJECT g_DriverObject;

NTSTATUS WfpFilterQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS WfpCalloutQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
PVOID FindCallbackEntryById(UINT32 calloutId);