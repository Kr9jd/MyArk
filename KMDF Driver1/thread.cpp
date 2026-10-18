#include "thread.h"
#include "utils.h"

static PEPARTITION g_PspSystemPartition = nullptr;
extern "C" NTSTATUS NTAPI PsLookupProcessByProcessId(HANDLE ProcessId, PEPROCESS* Process);

#define EPROCESS_THREAD_LIST_HEAD_OFFSET 0x370

// Enumerate system process threads.
NTSTATUS SystemThreadQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	PEPROCESS systemProcess = nullptr;
	NTSTATUS status = PsLookupProcessByProcessId((HANDLE)4, &systemProcess);
	if (!NT_SUCCESS(status) || !systemProcess)
		return status;

	LIST_ENTRY* threadListHead = (LIST_ENTRY*)((ULONG_PTR)systemProcess + EPROCESS_THREAD_LIST_HEAD_OFFSET);
	if (!MmIsAddressValid(threadListHead))
	{
		ObDereferenceObject(systemProcess);
		return STATUS_UNSUCCESSFUL;
	}

	// First pass: count threads
	ULONG count = 0;
	for (LIST_ENTRY* p = threadListHead->Flink;
		p != threadListHead && MmIsAddressValid(p);
		p = p->Flink)
		count++;

	DbgPrint("[MyArkDrv] SysThread: count=%u\n", count);

	if (!count)
	{
		ObDereferenceObject(systemProcess);
		return STATUS_UNSUCCESSFUL;
	}

	ULONG needed = (ULONG)MYARK_SYSTEM_THREAD_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
	{
		ObDereferenceObject(systemProcess);
		return STATUS_BUFFER_TOO_SMALL;
	}

	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();

	MYARK_SYSTEM_THREAD_RESPONSE* resp = (MYARK_SYSTEM_THREAD_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	ULONG i = 0;
	for (LIST_ENTRY* p = threadListHead->Flink;
		p != threadListHead && i < count && MmIsAddressValid(p);
		p = p->Flink)
	{
		ETHREAD *ethread = CONTAINING_RECORD(p, ETHREAD, ThreadListEntry);
		if (!MmIsAddressValid(ethread))
			break;

		MYARK_SYSTEM_THREAD_ENTRY* entry = &resp->Entries[i];

		entry->Tid = ethread->UniqueThreadId;
		entry->StartAddress = (ULONG64)ethread->StartAddress;
		entry->Win32StartAddress = (ULONG64)ethread->Win32StartAddress;
		entry->Priority = ethread->Tcb.Priority;
		entry->BasePriority = ethread->Tcb.BasePriority;
		entry->State = ethread->Tcb.State;
		entry->Reserved = 0;

		ULONG64 addr = entry->Win32StartAddress ? entry->Win32StartAddress : entry->StartAddress;
		ResolveAddressToModuleName(addr, modInfo, entry->ModuleName, MYARK_MAX_MODULE_NAME);
		ResolveAddressToModulePath(addr, modInfo, entry->ModulePath, MYARK_MAX_MODULE_PATH);

		i++;
	}

	resp->Header.Count = i;

	FreeSystemModules(modInfo);
	ObDereferenceObject(systemProcess);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}
