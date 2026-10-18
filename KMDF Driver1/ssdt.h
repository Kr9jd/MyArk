#pragma once
#include "header.h"

void SsdtInit();

NTSTATUS SsdtQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS SsdtQueryShadow(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
