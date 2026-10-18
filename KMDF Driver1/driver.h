#pragma once
#include "header.h"

// KLDR traversal of kernel driver modules.
NTSTATUS DriverQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS UnloadDriverQuery1(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS UnloadDriverQuery2(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS UnloadDriverQuery3(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS ReadMemory(PVOID inputBuffer, ULONG inputLength, PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS DriverDeviceQuery(PVOID inputBuffer, ULONG inputLength, PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS DriverDispatchQuery(PVOID inputBuffer, ULONG inputLength, PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);

struct UnloadDriverEntry
{
	UNICODE_STRING driverName;
	PVOID driverStartAddress;
	PVOID driverEndAddress;
	PVOID unknown2;
};

typedef struct KernelHashBucketEntry
{
	struct KernelHashBucketEntry* next;
	UNICODE_STRING name;
	ULONG hash[5];
};