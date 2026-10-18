#pragma once
#include "header.h"

NTSTATUS SystemThreadQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS SystemWorkThreadQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
