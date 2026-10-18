#pragma once
#include "header.h"

void WorkerInit();

NTSTATUS WorkerQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
