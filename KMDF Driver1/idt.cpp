#include "idt.h"

#define MYARK_IDT_ENTRIES_PER_CPU      256
#define MYARK_MAX_CPUS                 64
#define KPRCB_INTERRUPT_OBJECT_OFFSET  0x3900

static UINT64 IdtEntryToAddress(PKIDTENTRY64 entry)
{
	UINT64 lowOffset = (UINT64)entry->OffsetLow;
	UINT64 middleOffset = (UINT64)entry->OffsetMiddle << 16;
	UINT64 highOffset = (UINT64)entry->OffsetHigh << 32;
	return lowOffset | middleOffset | highOffset;
}

// Enumerate the IDT entries per CPU.
NTSTATUS IdtQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	int coreNum = KeNumberProcessors;
	if (coreNum <= 0 || coreNum > MYARK_MAX_CPUS)
		return STATUS_UNSUCCESSFUL;

	MYARK_IDT_ENTRY* tmp = (MYARK_IDT_ENTRY*)ExAllocatePool2(
		POOL_FLAG_NON_PAGED,
		sizeof(MYARK_IDT_ENTRY) * MYARK_IDT_ENTRIES_PER_CPU * coreNum, 'yrak');
	if (!tmp)
		return STATUS_INSUFFICIENT_RESOURCES;

	KIRQL oldIrql = KeRaiseIrqlToDpcLevel();

	ULONG count = 0;
	for (int cpu = 0; cpu < coreNum; cpu++) {
		KeSetSystemAffinityThread((KAFFINITY)cpu + 1);

		PKPCR pcr = KeGetPcr();
		PKIDTENTRY64 idt = pcr->IdtBase;
		PKINTERRUPT* slots = (PKINTERRUPT*)((PUCHAR)pcr->CurrentPrcb + KPRCB_INTERRUPT_OBJECT_OFFSET);
		KeRevertToUserAffinityThread();

		for (ULONG i = 0; i < MYARK_IDT_ENTRIES_PER_CPU; i++) {
			PKIDTENTRY64 entry = &idt[i];
			if (!MmIsAddressValid(entry))
				continue;

			tmp[count].Cpu = cpu;
			tmp[count].Index = i;
			tmp[count].Selector = entry->Selector;
			tmp[count].Dpl = entry->Dpl;
			tmp[count].Address = IdtEntryToAddress(entry);
			tmp[count].IsrCount = 0;
			RtlZeroMemory(tmp[count].IsrAddress, sizeof(tmp[count].IsrAddress));

			PKINTERRUPT slot = slots[i];
			if (slot && MmIsAddressValid(slot)) {
				if (!slot->ShareVector) {
					tmp[count].IsrAddress[0] = (ULONG64)slot->ServiceRoutine;
					tmp[count].IsrCount = 1;
				}
				else {
					PKINTERRUPT first = slot;
					PKINTERRUPT k = first;
					do {
						if (!MmIsAddressValid(k))
							break;
						if (tmp[count].IsrCount < 4)
							tmp[count].IsrAddress[tmp[count].IsrCount] = (ULONG64)k->ServiceRoutine;
						tmp[count].IsrCount++;
						k = CONTAINING_RECORD(k->InterruptListEntry.Flink, KINTERRUPT, InterruptListEntry);
					} while (k != first);
				}
			}
			count++;
		}
	}

	KeLowerIrql(oldIrql);

	ULONG needed = (ULONG)MYARK_IDT_RESPONSE_SIZE(count);
	if (outputBufferLength < needed) {
		ExFreePool(tmp);
		return STATUS_BUFFER_TOO_SMALL;
	}

	MYARK_IDT_RESPONSE* resp = (MYARK_IDT_RESPONSE*)outputBuffer;
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
