#pragma once
#include "header.h"

extern PDRIVER_OBJECT g_DriverObject;
NTSTATUS KernelObjectQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS ObjectTypeProcedureQuery(PVOID inputBuffer, ULONG inputLength, PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);