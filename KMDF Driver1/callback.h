#pragma once
#include "header.h"

void CallbackInit();

NTSTATUS CallbackQueryReg(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS CallbackQueryOb(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS CallbackQueryProcess(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS CallbackQueryBugCheckReason(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS CallbackQueryLastChanceShutdown(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS CallbackQueryBugCheck(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS CallbackQueryNotifyShutdown(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
