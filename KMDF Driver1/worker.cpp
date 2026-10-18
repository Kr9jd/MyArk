#include "worker.h"
#include "utils.h"

static PEPARTITION g_PspSystemPartition = nullptr;

void WorkerInit()
{
	PUCHAR hit = (PUCHAR)PatternScan((PVOID)IoQueueWorkItem, 0x100, "48 8B C8 E8 ?? ?? ?? ??");
	if (hit) {
		INT32 offset = *(INT32*)(hit + 4);
		PVOID psp = (PVOID)((ULONG_PTR)hit + 8 + (INT64)offset);
		PUCHAR hit2 = (PUCHAR)PatternScan(psp, 0x300, "48 8B 05 ?? ?? ?? ??");
		if (hit2) {
			INT32 offset2 = *(INT32*)(hit2 + 3);
			g_PspSystemPartition = *(PEPARTITION*)((ULONG_PTR)hit2 + 7 + (INT64)offset2);
		}
	}
	DbgPrint("[MyArkDrv] PspSystemPartition: 0x%p\n", g_PspSystemPartition);
}

static PEX_WORK_QUEUE GetWorkQueue(PEX_PARTITION exPartition, ULONG index0, ULONG index1)
{
	if (!exPartition || !MmIsAddressValid(exPartition))
		return nullptr;

	PEX_WORK_QUEUE* row = exPartition->WorkQueues[index0];
	if (!row || !MmIsAddressValid(row))
		return nullptr;

	PEX_WORK_QUEUE queue = row[index1];
	if (!queue || !MmIsAddressValid(queue))
		return nullptr;
	return queue;
}

NTSTATUS WorkerQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	if (!g_PspSystemPartition || !MmIsAddressValid(g_PspSystemPartition))
		return STATUS_UNSUCCESSFUL;

	PEX_PARTITION exPartition = (PEX_PARTITION)g_PspSystemPartition->ExPartition;
	if (!exPartition || !MmIsAddressValid(exPartition))
		return STATUS_UNSUCCESSFUL;

	ULONG count = 0;
	for (ULONG type = 0; type < 2; type++)
	{
		PEX_WORK_QUEUE workQueue = GetWorkQueue(exPartition, 0, type);
		if (!workQueue)
			continue;
		LIST_ENTRY* threadHead = &workQueue->WorkPriQueue.ThreadListHead;
		if (!MmIsAddressValid(threadHead))
			continue;
		for (LIST_ENTRY* e = threadHead->Flink;
			e != threadHead && MmIsAddressValid(e);
			e = e->Flink)
			count++;
	}

	DbgPrint("[MyArkDrv] WorkThreads: count=%u\n", count);

	if (!count)
		return STATUS_UNSUCCESSFUL;

	ULONG needed = (ULONG)MYARK_WORK_THREAD_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();

	MYARK_WORK_THREAD_RESPONSE* resp = (MYARK_WORK_THREAD_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	ULONG i = 0;
	for (ULONG type = 0; type < 2 && i < count; type++)
	{
		PEX_WORK_QUEUE workQueue = GetWorkQueue(exPartition, 0, type);
		if (!workQueue)
			continue;
		LIST_ENTRY* threadHead = &workQueue->WorkPriQueue.ThreadListHead;
		for (LIST_ENTRY* e = threadHead->Flink;
			e != threadHead && i < count && MmIsAddressValid(e);
			e = e->Flink)
		{
			KTHREAD* kthread = CONTAINING_RECORD(e, KTHREAD, QueueListEntry);
			if (!MmIsAddressValid(kthread))
				break;

			ETHREAD* ethread = (ETHREAD*)kthread;
			MYARK_WORK_THREAD_ENTRY* entry = &resp->Entries[i];

			entry->PoolType = type;
			entry->Reserved = 0;
			entry->Tid = ethread->UniqueThreadId;
			entry->StartAddress = (ULONG64)ethread->StartAddress;
			entry->Priority = ethread->Tcb.Priority;
			entry->BasePriority = ethread->Tcb.BasePriority;
			entry->State = ethread->Tcb.State;
			entry->Pad = 0;

			ResolveAddressToModuleName(entry->StartAddress, modInfo, entry->ModuleName, MYARK_MAX_MODULE_NAME);
			ResolveAddressToModulePath(entry->StartAddress, modInfo, entry->ModulePath, MYARK_MAX_MODULE_PATH);

			i++;
		}
	}

	resp->Header.Count = i;

	FreeSystemModules(modInfo);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}
