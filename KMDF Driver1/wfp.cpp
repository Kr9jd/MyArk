
#include "header.h"
#include "utils.h"
#include "protocol.h"
#include "wfp.h"

static PVOID g_WfpGlobal = nullptr;
static PVOID g_CalloutSrc = nullptr;
static ULONGLONG g_CalloutCount = 0;

static void GuidToWchar(const GUID* guid, WCHAR* out, ULONG maxChars)
{
	if (!guid || !out || maxChars < 40)
	{
		if (out && maxChars)
			out[0] = 0;
		return;
	}
	RtlStringCbPrintfW(out, maxChars * sizeof(WCHAR),
		L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		guid->Data1, guid->Data2, guid->Data3,
		guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
		guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
}

static void WideCopy(WCHAR* dst, ULONG maxChars, PCWSTR src)
{
	if (!src)
	{
		dst[0] = 0;
		return;
	}
	ULONG len = 0;
	while (src[len] && len < maxChars - 1)
	{
		dst[len] = src[len];
		len++;
	}
	dst[len] = 0;
}

// Enumerate WFP filters.
NTSTATUS WfpFilterQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	NTSTATUS status;

	HANDLE engineHandle = NULL;
	status = FwpmEngineOpen0(NULL, RPC_C_AUTHN_DEFAULT, NULL, NULL, &engineHandle);
	DbgPrint("[MyArkDrv] WFP Filter: EngineOpen=0x%08X\n", status);
	if (!NT_SUCCESS(status))
		return STATUS_UNSUCCESSFUL;

	HANDLE enumHandle = NULL;
	status = FwpmFilterCreateEnumHandle0(engineHandle, NULL, &enumHandle);
	DbgPrint("[MyArkDrv] WFP Filter: CreateEnum=0x%08X\n", status);
	if (!NT_SUCCESS(status))
	{
		FwpmEngineClose0(engineHandle);
		return STATUS_UNSUCCESSFUL;
	}

	ULONG capacity = 64;
	MYARK_WFP_FILTER_ENTRY* tmp = (MYARK_WFP_FILTER_ENTRY*)ExAllocatePool2(
		POOL_FLAG_PAGED, sizeof(MYARK_WFP_FILTER_ENTRY) * capacity, 'pfwM');
	if (!tmp)
	{
		FwpmFilterDestroyEnumHandle0(engineHandle, enumHandle);
		FwpmEngineClose0(engineHandle);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	FWPM_FILTER0** entries = NULL;
	UINT32 numReturned = 0;
	UINT32 batchCount = 32;
	ULONG i = 0;
	WCHAR guidBuf[64];

	while (TRUE)
	{
		status = FwpmFilterEnum0(engineHandle, enumHandle, batchCount, &entries, &numReturned);
		if (!NT_SUCCESS(status) || numReturned == 0 || !entries)
			break;

		for (UINT32 k = 0; k < numReturned; k++)
		{
			if (i >= capacity)
			{
				ULONG newCap = capacity * 2;
				MYARK_WFP_FILTER_ENTRY* newBuf = (MYARK_WFP_FILTER_ENTRY*)ExAllocatePool2(
					POOL_FLAG_PAGED, sizeof(MYARK_WFP_FILTER_ENTRY) * newCap, 'pfwM');
				if (!newBuf)
					break;
				RtlCopyMemory(newBuf, tmp, sizeof(MYARK_WFP_FILTER_ENTRY) * capacity);
				ExFreePool(tmp);
				tmp = newBuf;
				capacity = newCap;
			}

			FWPM_FILTER0* filter = entries[k];
			MYARK_WFP_FILTER_ENTRY* entry = &tmp[i];

			entry->FilterId = filter->filterId;
			WideCopy(entry->Name, 64, filter->displayData.name);
			WideCopy(entry->Description, 128, filter->displayData.description);
			entry->ActionType = filter->action.type;
			entry->Reserved = 0;

			GuidToWchar(&filter->layerKey, guidBuf, 64);
			WideCopy(entry->LayerKey, 64, guidBuf);

			GuidToWchar(&filter->action.calloutKey, guidBuf, 64);
			WideCopy(entry->CalloutKey, 64, guidBuf);

			i++;
		}

		FwpmFreeMemory0((void**)&entries);
		entries = NULL;
	}

	DbgPrint("[MyArkDrv] WFP Filter: collected=%u lastStatus=0x%08X\n", i, status);

	FwpmFilterDestroyEnumHandle0(engineHandle, enumHandle);
	FwpmEngineClose0(engineHandle);

	if (!i)
	{
		ExFreePool(tmp);
		return STATUS_UNSUCCESSFUL;
	}

	ULONG needed = (ULONG)MYARK_WFP_FILTER_RESPONSE_SIZE(i);
	if (outputBufferLength < needed)
	{
		ExFreePool(tmp);
		return STATUS_BUFFER_TOO_SMALL;
	}

	MYARK_WFP_FILTER_RESPONSE* resp = (MYARK_WFP_FILTER_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = i;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	for (ULONG k = 0; k < i; k++)
		resp->Entries[k] = tmp[k];

	ExFreePool(tmp);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}

// Enumerate WFP callouts.
NTSTATUS WfpCalloutQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	NTSTATUS status;

	PVOID fwpkclntBase = GetModuleBase(g_DriverObject, L"fwpkclnt.sys");
	if (!fwpkclntBase)
		return STATUS_UNSUCCESSFUL;

	PVOID regEntry = GetExportByName(fwpkclntBase, "FwpsCalloutRegister0");
	if (!regEntry)
		return STATUS_UNSUCCESSFUL;


	PUCHAR hit = (PUCHAR)PatternScan(regEntry, 0x200, "48 8b d7 48 8b ce e8 ?? ?? ?? ??");
	if (hit) {
		INT32 offset = *(INT32*)(hit + 7);
		PVOID p_FwppCalloutRegister = (PVOID)((ULONG_PTR)hit + 11 + (INT64)offset);

		
		if (p_FwppCalloutRegister) {
			PUCHAR hit2 = (PUCHAR)PatternScan(p_FwppCalloutRegister, 0x200, "44 8d 7f 63 e8 ?? ?? ?? ??");
			if (hit2) {
				INT32 offset2 = *(INT32*)(hit2 + 5);
				PVOID p_FwppCalloutFindByKey = (PVOID)((ULONG_PTR)hit2 + 9 + (INT64)offset2);

				if (p_FwppCalloutFindByKey) {
					PUCHAR hit3 = (PUCHAR)PatternScan(p_FwppCalloutFindByKey, 0x200, "48 8b 05 ?? ?? ?? ??");
					PUCHAR hit4 = (PUCHAR)PatternScan(p_FwppCalloutFindByKey, 0x400, "ba 00 01 00 00 3b 15 ?? ?? ?? ??");

					if (hit3 && hit4) {
						INT32 offset3 = *(INT32*)(hit3 + 3);
						INT32 offset4 = *(INT32*)(hit4 + 7);

						g_CalloutSrc = *(PVOID*)((ULONG_PTR)hit3 + 7 + (INT64)offset3);
						g_CalloutCount = *(ULONGLONG*)((ULONG_PTR)hit4 + 11 + (INT64)offset4);
					}
				}
			}
		}
	}

	ULONG count = 0;

	// First pass
	for (int i = 0;i < 51;i++) {
		PCALLOUT_DATA calloutData = (PCALLOUT_DATA)((ULONG_PTR)g_CalloutSrc + 40 * i);
		if (calloutData->flags & 1) {
			count++;
		}
	}

	// Second pass
	for (int i = 256;i < g_CalloutCount;i++) {
		PCALLOUT_DATA calloutData = (PCALLOUT_DATA)((ULONG_PTR)g_CalloutSrc + 40 * i);
		if (calloutData->flags & 1) {
			count++;
		}
	}

	ULONG needed = (ULONG)MYARK_WFP_CALLOUT_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();
	MYARK_WFP_CALLOUT_RESPONSE* resp = (MYARK_WFP_CALLOUT_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	ULONG index = 0;
	for (int i = 0;i < 51;i++) {
		PCALLOUT_DATA calloutData = (PCALLOUT_DATA)((ULONG_PTR)g_CalloutSrc + 40 * i);
		if (calloutData->flags & 1) {
			MYARK_WFP_CALLOUT_ENTRY* entry = &resp->Entries[index];
			entry->CalloutId = i;
			entry->Reserved = 0;
			entry->ClassifyFn = 0;
			entry->NotifyFn = 0;
			entry->FlowDeleteFn = 0;
			entry->ModuleName[0] = 0;

			PCUSTOM_CALLOUT_ENTRY custom = (PCUSTOM_CALLOUT_ENTRY)FindCallbackEntryById(i);
			if (custom && MmIsAddressValid(custom))
			{
				entry->ClassifyFn = (custom->Type == 3) ? (ULONG64)custom->ClassifyFn2 : (ULONG64)custom->ClassifyFn1;
				entry->NotifyFn = (ULONG64)custom->NotifyFn;
				entry->FlowDeleteFn = (ULONG64)custom->FlowDeleteFn;

				ULONG64 modAddr = entry->ClassifyFn ? entry->ClassifyFn : entry->NotifyFn;
				ResolveAddressToModuleName(modAddr, modInfo, entry->ModuleName, MYARK_MAX_MODULE_NAME);
				ResolveAddressToModulePath(modAddr, modInfo, entry->ModulePath, MYARK_MAX_MODULE_PATH);
			}
			index++;
		}
	}

	for (int i = 256;i < g_CalloutCount;i++) {
		PCALLOUT_DATA calloutData = (PCALLOUT_DATA)((ULONG_PTR)g_CalloutSrc + 40 * i);
		if (calloutData->flags & 1) {
			MYARK_WFP_CALLOUT_ENTRY* entry = &resp->Entries[index];
			entry->CalloutId = i;
			entry->Reserved = 0;
			entry->ClassifyFn = 0;
			entry->NotifyFn = 0;
			entry->FlowDeleteFn = 0;
			entry->ModuleName[0] = 0;

			PCUSTOM_CALLOUT_ENTRY custom = (PCUSTOM_CALLOUT_ENTRY)FindCallbackEntryById(i);
			if (custom && MmIsAddressValid(custom))
			{
				entry->ClassifyFn = (custom->Type == 3) ? (ULONG64)custom->ClassifyFn2 : (ULONG64)custom->ClassifyFn1;
				entry->NotifyFn = (ULONG64)custom->NotifyFn;
				entry->FlowDeleteFn = (ULONG64)custom->FlowDeleteFn;

				ULONG64 modAddr = entry->ClassifyFn ? entry->ClassifyFn : entry->NotifyFn;
				ResolveAddressToModuleName(modAddr, modInfo, entry->ModuleName, MYARK_MAX_MODULE_NAME);
				ResolveAddressToModulePath(modAddr, modInfo, entry->ModulePath, MYARK_MAX_MODULE_PATH);
			}
			index++;
		}
	}

	FreeSystemModules(modInfo);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}

PVOID FindCallbackEntryById(UINT32 calloutId)
{
	PVOID netioBase = GetModuleBase(g_DriverObject, L"netio.sys");
	if (!netioBase) {
		DbgPrint("[MyArkDrv] WFP Callout: netio base not found\n");
		return nullptr;
	}

	PVOID kfdEntry = GetExportByName(netioBase, "KfdRegisterCalloutEntry");
	if (!kfdEntry)
		return nullptr;


	PUCHAR hit = (PUCHAR)PatternScan(kfdEntry, 0x200, "48 8b 0d ?? ?? ?? ??");
	if (hit) {
		INT32 offset = *(INT32*)(hit + 3);
		g_WfpGlobal = *(PVOID*)((ULONG_PTR)hit + 7 + (INT64)offset);
	}

	if (!g_WfpGlobal || !MmIsAddressValid(g_WfpGlobal)) {
		return nullptr;
	}

	ULONG_PTR offset = (ULONG_PTR)(calloutId + calloutId * 2) << 5;
	ULONG_PTR base = *(ULONG_PTR*)((ULONG_PTR)g_WfpGlobal + 0x1A0);
	if (!base) {
		return nullptr;
	}

	return (PVOID)(base + offset);
}
