#include "timer.h"
#include "utils.h"

#define MYARK_MAX_TIMER_ENTRIES 8192

static PVOID g_KiSetTimerEx;
static UINT64 g_KiWaitNever;
static UINT64 g_KiWaitAlways;
static PVOID g_IopTimerQueueHead;

void TimerInit()
{
	UCHAR* hit = (UCHAR*)PatternScan(KeSetTimerEx, 0x30, "e8 ?? ?? ?? ??");
	if (hit) {
		INT32 offset = *(INT32*)(hit + 1);
		g_KiSetTimerEx = (PVOID)((ULONG_PTR)hit + 5 + offset);
	}

	UCHAR* hit1 = (UCHAR*)PatternScan(g_KiSetTimerEx, 0x100, "48 8b f9 48 8b 05 ?? ?? ?? ??");
	if (hit1) {
		INT32 offset = *(INT32*)(hit1 + 6);
		g_KiWaitNever = *(UINT64*)((ULONG_PTR)hit1 + 10 + offset);
	}

	UCHAR* hit2 = (UCHAR*)PatternScan(g_KiSetTimerEx, 0x100, "48 33 2d ?? ?? ?? ??");
	if (hit2) {
		INT32 offset = *(INT32*)(hit2 + 3);
		g_KiWaitAlways = *(UINT64*)((ULONG_PTR)hit2 + 7 + offset);
	}

	UCHAR* hit3 = (UCHAR*)PatternScan(IoInitializeTimer, 0x1000, "48 8d 0d ?? ?? ?? ??");
	if (hit3) {
		INT32 offset = *(INT32*)(hit3 + 3);
		g_IopTimerQueueHead = (PVOID)((ULONG_PTR)hit3 + 7 + offset);
	}
}

static ULONG CollectDpcTimers(MYARK_TIMER_ENTRY* entries, ULONG maxEntries)
{
	if (!g_KiSetTimerEx || !g_KiWaitNever || !g_KiWaitAlways)
		return 0;

	ULONG count = 0;
	int coreNum = KeNumberProcessors;
	for (int i = 0; i < coreNum && count < maxEntries; i++) {
		KeSetSystemAffinityThread(i + 1);

		PKPCR pcr = KeGetPcr();
		PVOID pcrb = (PVOID)pcr->CurrentPrcb;
		KeRevertToUserAffinityThread();

		PVOID timerTable = (PVOID)((ULONG_PTR)pcrb + 0x4100);
		PKTIMER_TABLE_ENTRY pTableEntry = (PKTIMER_TABLE_ENTRY)((ULONG_PTR)timerTable + 0x200);
		for (int j = 0; j < 512 && count < maxEntries; j++) {

			PLIST_ENTRY p_ListEntryHead = &(pTableEntry[j].Entry);
			PLIST_ENTRY p_ListEntryEntry = p_ListEntryHead->Flink;
			do {
				if (!MmIsAddressValid(p_ListEntryEntry)) {
					break;
				}

				KTIMER* ktimer = CONTAINING_RECORD(p_ListEntryEntry, KTIMER, TimerListEntry);

				if (MmIsAddressValid(ktimer) && ktimer->Dpc != NULL) {
					UINT64 dpc = (UINT64)ktimer->Dpc;

					UINT64 var1 = dpc ^ g_KiWaitNever;
					UINT64 var2 = _rotl64(var1, g_KiWaitNever & 0xFF);
					UINT64 var3 = (UINT64)ktimer ^ var2;
					UINT64 var4 = _byteswap_uint64(var3);
					PKDPC realDpc = (PKDPC)(var4 ^ g_KiWaitAlways);

					if (!MmIsAddressValid(realDpc)) {
						break;
					}
					entries[count].Type = 0; // DPC Timer
					entries[count].TimerAddress = (ULONG64)ktimer;
					entries[count].RoutineAddress = (ULONG64)realDpc->DeferredRoutine;
					count++;
				}

				p_ListEntryEntry = p_ListEntryEntry->Flink;
			} while (p_ListEntryHead != p_ListEntryEntry);
		}
	}
	return count;
}

static ULONG CollectIoTimers(MYARK_TIMER_ENTRY* entries, ULONG maxEntries)
{
	if (!g_IopTimerQueueHead || !MmIsAddressValid(g_IopTimerQueueHead))
		return 0;

	ULONG count = 0;
	KIRQL oldIrql = KeRaiseIrqlToDpcLevel();

	LIST_ENTRY* ioTimerHead = (LIST_ENTRY*)g_IopTimerQueueHead;
	LIST_ENTRY* ioTimerEntry = ioTimerHead->Flink;

	do {
		if (!MmIsAddressValid(ioTimerEntry))
			break;

		PIO_TIMER timer = CONTAINING_RECORD(ioTimerEntry, IO_TIMER, TimerList);
		if (MmIsAddressValid(timer) && count < maxEntries) {
			entries[count].Type = 1; // IO Timer
			entries[count].TimerAddress = (ULONG64)timer;
			entries[count].RoutineAddress = (ULONG64)timer->TimerRoutine;
			count++;
		}
		ioTimerEntry = ioTimerEntry->Flink;
	} while (ioTimerEntry != ioTimerHead);

	KeLowerIrql(oldIrql);
	return count;
}

// Enumerate DPC and IO timers.
NTSTATUS TimerQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	MYARK_TIMER_ENTRY* tmp = (MYARK_TIMER_ENTRY*)ExAllocatePool2(
		POOL_FLAG_NON_PAGED, sizeof(MYARK_TIMER_ENTRY) * MYARK_MAX_TIMER_ENTRIES, 'yrak');
	if (!tmp)
		return STATUS_INSUFFICIENT_RESOURCES;

	ULONG count = 0;
	count += CollectDpcTimers(&tmp[count], MYARK_MAX_TIMER_ENTRIES - count);
	count += CollectIoTimers(&tmp[count], MYARK_MAX_TIMER_ENTRIES - count);

	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();
	for (ULONG i = 0; i < count; i++) {
		ResolveAddressToModuleName(tmp[i].RoutineAddress, modInfo, tmp[i].ModuleName, MYARK_MAX_MODULE_NAME);
		ResolveAddressToModulePath(tmp[i].RoutineAddress, modInfo, tmp[i].ModulePath, MYARK_MAX_MODULE_PATH);
	}
	FreeSystemModules(modInfo);

	ULONG needed = (ULONG)MYARK_TIMER_RESPONSE_SIZE(count);
	if (outputBufferLength < needed) {
		ExFreePool(tmp);
		return STATUS_BUFFER_TOO_SMALL;
	}

	MYARK_TIMER_RESPONSE* resp = (MYARK_TIMER_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	for (ULONG i = 0; i < count; i++)
		resp->Entries[i] = tmp[i];

	ExFreePool(tmp);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}
