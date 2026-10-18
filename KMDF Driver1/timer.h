#pragma once
#include "header.h"

void TimerInit();

NTSTATUS TimerQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
