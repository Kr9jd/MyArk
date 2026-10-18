#include "callback.h"
#include "utils.h"

static PVOID g_PspCreateProcessNotifyRoutine = nullptr;
static PVOID g_PspCreateThreadNotifyRoutine = nullptr;
static PVOID g_PspLoadImageNotifyRoutine = nullptr;
static PVOID g_CallbackListHead = nullptr;
static PVOID g_BugCheckReasonListHead = nullptr;
static PVOID g_BugCheckListHead = nullptr;
static PVOID g_LastChanceShutdownQueueHead = nullptr;
static PVOID g_IopNotifyShutdownQueueHead = nullptr;

void CallbackInit()
{
	// 解析 PspCreateProcessNotifyRoutine 地址
	PUCHAR hit = (PUCHAR)PatternScan((PVOID)PsSetCreateProcessNotifyRoutine, 0x80, "E8 ?? ?? ?? ??");
	if (hit) {
		INT32 offset = *(INT32*)(hit + 1);
		PVOID psp = (PVOID)((ULONG_PTR)hit + 5 + (INT64)offset);
		PUCHAR hit2 = (PUCHAR)PatternScan(psp, 0x300, "4C 8D 2D ?? ?? ?? ??");
		if (hit2) {
			INT32 offset = *(INT32*)(hit2 + 3);
			g_PspCreateProcessNotifyRoutine = (PVOID)((ULONG_PTR)hit2 + 7 + (INT64)offset);
		}
	}
	DbgPrint("[MyArkDrv] PspCreateProcessNotifyRoutine: 0x%p\n", g_PspCreateProcessNotifyRoutine);

	PUCHAR hit1 = (PUCHAR)PatternScan((PVOID)PsSetCreateThreadNotifyRoutine, 0x80, "E8 ?? ?? ?? ??");
	if (hit1) {
		INT32 offset = *(INT32*)(hit1 + 1);
		PVOID psp = (PVOID)((ULONG_PTR)hit1 + 5 + (INT64)offset);
		PUCHAR hit2 = (PUCHAR)PatternScan(psp, 0x300, "48 8d 0d ?? ?? ?? ??");
		if (hit2) {
			INT32 offset = *(INT32*)(hit2 + 3);
			g_PspCreateThreadNotifyRoutine = (PVOID)((ULONG_PTR)hit2 + 7 + (INT64)offset);
		}
	}
	DbgPrint("[MyArkDrv] PspCreateThreadNotifyRoutine: 0x%p\n", g_PspCreateThreadNotifyRoutine);

	PUCHAR hit3 = (PUCHAR)PatternScan((PVOID)PsSetLoadImageNotifyRoutine, 0x80, "48 8d 0d ?? ?? ?? ??");
	if (hit3) {
		INT32 offset3 = *(INT32*)(hit3 + 3);
		g_PspLoadImageNotifyRoutine = (PVOID)((ULONG_PTR)hit3 + 7 + (INT64)offset3);
	}
	DbgPrint("[MyArkDrv] PspLoadImageNotifyRoutine: 0x%p\n", g_PspLoadImageNotifyRoutine);

	PUCHAR hit4 = (PUCHAR)PatternScan((PVOID)CmRegisterCallback, 0x80, "E8 ?? ?? ?? ??");
	if (hit4) {
		INT32 offset = *(INT32*)(hit4 + 1);
		PVOID internal = (PVOID)((ULONG_PTR)hit4 + 5 + (INT64)offset);
		PUCHAR hit2 = (PUCHAR)PatternScan(internal, 0x300, "40 8a d6 48 8b cb e8 ?? ?? ?? ??");
		if (hit2) {
			INT32 offset2 = *(INT32*)(hit2 + 7);
			PVOID insert = (PVOID)((ULONG_PTR)hit2 + 11 + (INT64)offset2);
			PUCHAR hit3 = (PUCHAR)PatternScan(insert, 0x80, "4c 8d 3d ?? ?? ?? ??");
			if (hit3) {
				INT32 offset3 = *(INT32*)(hit3 + 3);
				g_CallbackListHead = (PVOID)((ULONG_PTR)hit3 + 7 + (INT64)offset3);
			}
		}
	}
	DbgPrint("[MyArkDrv] CallbackListHead: 0x%p\n", g_CallbackListHead);

	PUCHAR hit5 = (PUCHAR)PatternScan((PVOID)KeRegisterBugCheckReasonCallback, 0x200, "48 8d 3d ?? ?? ?? ??");
	if (hit5) {
		INT32 offset = *(INT32*)(hit5 + 3);
		g_BugCheckReasonListHead = (PVOID)((ULONG_PTR)hit5 + 7 + (INT64)offset);
	}

	PUCHAR hit6 = (PUCHAR)PatternScan((PVOID)IoRegisterLastChanceShutdownNotification, 0x100, "48 8d 0d ?? ?? ?? ??");
	if (hit6) {
		INT32 offset = *(INT32*)(hit6 + 3);
		g_LastChanceShutdownQueueHead = (PVOID)((ULONG_PTR)hit6 + 7 + (INT64)offset);
	}

	PUCHAR hit7 = (PUCHAR)PatternScan((PVOID)KeRegisterBugCheckCallback, 0x100, "48 8b 05 ?? ?? ?? ??");
	if (hit7) {
		INT32 offset = *(INT32*)(hit7 + 3);
		g_BugCheckListHead = (PVOID)((ULONG_PTR)hit7 + 7 + (INT64)offset);
	}

	PUCHAR hit8 = (PUCHAR)PatternScan((PVOID)IoRegisterShutdownNotification, 0x100, "48 8d 0d ?? ?? ?? ??");
	if (hit8) {
		INT32 offset = *(INT32*)(hit8 + 3);
		g_IopNotifyShutdownQueueHead = (PVOID)((ULONG_PTR)hit8 + 7 + (INT64)offset);
	}
}

NTSTATUS CallbackQueryReg(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;
	if (!g_CallbackListHead || !MmIsAddressValid(g_CallbackListHead))
		return STATUS_UNSUCCESSFUL;

	CM_CALLBACK_CONTEXT_BLOCK* pContextBlock = (CM_CALLBACK_CONTEXT_BLOCK*)g_CallbackListHead;
	LIST_ENTRY* pHead = &pContextBlock->CallbackList;
	LIST_ENTRY* pEntry = pHead->Flink;
	ULONG count = 0;
	while (pEntry != pHead) { count++; pEntry = pEntry->Flink; }

	ULONG needed = (ULONG)MYARK_REGISTER_CALLBACK_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	MYARK_REGISTER_CALLBACK_RESPONSE* resp = (MYARK_REGISTER_CALLBACK_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();

	pEntry = pHead->Flink;
	ULONG i = 0;
	while (pEntry != pHead && i < count)
	{
		CM_CALLBACK_CONTEXT_BLOCK* op = CONTAINING_RECORD(pEntry, CM_CALLBACK_CONTEXT_BLOCK, CallbackList);
		MYARK_REGISTER_CALLBACK_ENTRY* entry = &resp->Entries[i];

		entry->Callback = (ULONG64)op->Function;

		// Altitude
		ULONG altChars = 0;
		if (op->Altitude.Buffer && MmIsAddressValid(op->Altitude.Buffer))
		{
			altChars = op->Altitude.Length / sizeof(WCHAR);
			if (altChars > 31)
				altChars = 31;
			for (ULONG c = 0; c < altChars; c++)
				entry->Altitude[c] = op->Altitude.Buffer[c];
		}
		entry->Altitude[altChars] = 0;

		ResolveAddressToModuleName(entry->Callback, modInfo, entry->ModuleName, MYARK_MAX_MODULE_NAME);
		ResolveAddressToModulePath(entry->Callback, modInfo, entry->ModulePath, MYARK_MAX_MODULE_PATH);

		i++;
		pEntry = pEntry->Flink;
	}

	FreeSystemModules(modInfo);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}

NTSTATUS CallbackQueryOb(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	POBJECT_TYPE objTypes[2] = { *PsProcessType, *PsThreadType };
	ULONG typeCounts[2] = { 0, 0 };
	ULONG count = 0;
	for (int t = 0; t < 2; t++)
	{
		LIST_ENTRY* pHead = &objTypes[t]->CallbackList;
		LIST_ENTRY* pEntry = pHead->Flink;
		while (pEntry != pHead) { typeCounts[t]++; pEntry = pEntry->Flink; }
		count += typeCounts[t];
	}

	ULONG needed = (ULONG)MYARK_OB_CALLBACK_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	MYARK_OB_CALLBACK_RESPONSE* resp = (MYARK_OB_CALLBACK_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	// 第二遍：填充条目
	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();

	ULONG i = 0;
	for (int t = 0; t < 2; t++)
	{
		LIST_ENTRY* pHead = &objTypes[t]->CallbackList;
		LIST_ENTRY* pEntry = pHead->Flink;
		while (pEntry != pHead && i < count)
		{
			OB_OPERATION_ENTRY* op = CONTAINING_RECORD(pEntry, OB_OPERATION_ENTRY, CallbackList);
			MYARK_OB_CALLBACK_ENTRY* entry = &resp->Entries[i];

			entry->PreOperation = (ULONG64)op->PreOperation;
			entry->PostOperation = (ULONG64)op->PostOperation;
			entry->RegistrationHandle = (ULONG64)op->Registration;
			entry->Operations = op->Operations;
			entry->Type = t;

			ULONG64 addr = entry->PreOperation ? entry->PreOperation : entry->PostOperation;
			ResolveAddressToModuleName(addr, modInfo, entry->ModuleName, MYARK_MAX_MODULE_NAME);
			ResolveAddressToModulePath(addr, modInfo, entry->ModulePath, MYARK_MAX_MODULE_PATH);

			i++;
			pEntry = pEntry->Flink;
		}
	}

	FreeSystemModules(modInfo);

	*bytesWritten = needed;
	return STATUS_SUCCESS;
}

NTSTATUS CallbackQueryProcess(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	INT64* pProcArray = (INT64*)g_PspCreateProcessNotifyRoutine;
	INT64* pThreadArray = (INT64*)g_PspCreateThreadNotifyRoutine;
	INT64* pImageArray = (INT64*)g_PspLoadImageNotifyRoutine;
	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();

	// 第一遍：统计有效条目
	ULONG count = 0;
	if (pProcArray) {
		for (int i = 0; i < 64; i++) {
			PROCESS_CALLBACK_ENTRY* e = (PROCESS_CALLBACK_ENTRY*)(pProcArray[i] & 0xFFFFFFFFFFFFFFF0);
			if (MmIsAddressValid(e) && e->Callback) count++;
		}
	}
	if (pThreadArray) {
		for (int i = 0; i < 64; i++) {
			THREAD_CALLBACK_ENTRY* e = (THREAD_CALLBACK_ENTRY*)(pThreadArray[i] & 0xFFFFFFFFFFFFFFF0);
			if (MmIsAddressValid(e) && e->Callback) count++;
		}
	}

	if (pImageArray) {
		for (int i = 0; i < 64; i++) {
			IMAGE_CALLBACK_ENTRY* e = (IMAGE_CALLBACK_ENTRY*)(pImageArray[i] & 0xFFFFFFFFFFFFFFF0);
			if (MmIsAddressValid(e) && e->Callback) count++;
		}
	}

	ULONG needed = (ULONG)MYARK_PROCESS_CALLBACK_RESPONSE_SIZE(count);
	if (outputBufferLength < needed) {
		FreeSystemModules(modInfo);
		return STATUS_BUFFER_TOO_SMALL;
	}

	MYARK_PROCESS_CALLBACK_RESPONSE* resp = (MYARK_PROCESS_CALLBACK_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	// 第二遍：填充条目
	ULONG idx = 0;
	if (pProcArray) {
		for (int i = 0; i < 64 && idx < count; i++) {
			PROCESS_CALLBACK_ENTRY* e = (PROCESS_CALLBACK_ENTRY*)(pProcArray[i] & 0xFFFFFFFFFFFFFFF0);
			if (!MmIsAddressValid(e) || !e->Callback) continue;
			resp->Entries[idx].Callback = (ULONG64)e->Callback;
			resp->Entries[idx].Type = 0; // Process
			ResolveAddressToModuleName((ULONG64)e->Callback, modInfo,
				resp->Entries[idx].ModuleName, MYARK_MAX_MODULE_NAME);
			ResolveAddressToModulePath((ULONG64)e->Callback, modInfo,
				resp->Entries[idx].ModulePath, MYARK_MAX_MODULE_PATH);
			idx++;
		}
	}
	if (pThreadArray) {
		for (int i = 0; i < 64 && idx < count; i++) {
			THREAD_CALLBACK_ENTRY* e = (THREAD_CALLBACK_ENTRY*)(pThreadArray[i] & 0xFFFFFFFFFFFFFFF0);
			if (!MmIsAddressValid(e) || !e->Callback) continue;
			resp->Entries[idx].Callback = (ULONG64)e->Callback;
			resp->Entries[idx].Type = 1; // Thread
			ResolveAddressToModuleName((ULONG64)e->Callback, modInfo,
				resp->Entries[idx].ModuleName, MYARK_MAX_MODULE_NAME);
			ResolveAddressToModulePath((ULONG64)e->Callback, modInfo,
				resp->Entries[idx].ModulePath, MYARK_MAX_MODULE_PATH);
			idx++;
		}
	}

	if (pImageArray) {
		for (int i = 0; i < 64 && idx < count; i++) {
			IMAGE_CALLBACK_ENTRY* e = (IMAGE_CALLBACK_ENTRY*)(pImageArray[i] & 0xFFFFFFFFFFFFFFF0);
			if (!MmIsAddressValid(e) || !e->Callback) continue;
			resp->Entries[idx].Callback = (ULONG64)e->Callback;
			resp->Entries[idx].Type = 2; // Image
			ResolveAddressToModuleName((ULONG64)e->Callback, modInfo,
				resp->Entries[idx].ModuleName, MYARK_MAX_MODULE_NAME);
			ResolveAddressToModulePath((ULONG64)e->Callback, modInfo,
				resp->Entries[idx].ModulePath, MYARK_MAX_MODULE_PATH);
			idx++;
		}
	}

	FreeSystemModules(modInfo);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}

NTSTATUS CallbackQueryBugCheckReason(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;
	if (!g_BugCheckReasonListHead || !MmIsAddressValid(g_BugCheckReasonListHead))
		return STATUS_UNSUCCESSFUL;

	PKBUGCHECK_REASON_CALLBACK_RECORD pCallbackRecord = (PKBUGCHECK_REASON_CALLBACK_RECORD)g_BugCheckReasonListHead;
	LIST_ENTRY* pHead = &pCallbackRecord->Entry;
	LIST_ENTRY* pEntry = pHead->Flink;
	ULONG count = 0;
	while (pEntry != pHead) { count++; pEntry = pEntry->Flink; }

	ULONG needed = (ULONG)MYARK_BUGCHECK_CALLBACK_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	MYARK_BUGCHECK_CALLBACK_RESPONSE* resp = (MYARK_BUGCHECK_CALLBACK_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();

	pEntry = pHead->Flink;
	ULONG i = 0;
	while (pEntry != pHead && i < count)
	{
		PKBUGCHECK_REASON_CALLBACK_RECORD record = CONTAINING_RECORD(pEntry, KBUGCHECK_REASON_CALLBACK_RECORD, Entry);
		MYARK_BUGCHECK_CALLBACK_ENTRY* entry = &resp->Entries[i];

		entry->Callback = (ULONG64)record->CallbackRoutine;
		entry->Reason = (ULONG)record->Reason;
		ResolveAddressToModuleName(entry->Callback, modInfo, entry->ModuleName, MYARK_MAX_MODULE_NAME);
		ResolveAddressToModulePath(entry->Callback, modInfo, entry->ModulePath, MYARK_MAX_MODULE_PATH);

		i++;
		pEntry = pEntry->Flink;
	}

	FreeSystemModules(modInfo);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}

NTSTATUS CallbackQueryLastChanceShutdown(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;
	if (!g_LastChanceShutdownQueueHead || !MmIsAddressValid(g_LastChanceShutdownQueueHead))
		return STATUS_UNSUCCESSFUL;

	PLASTCHANCE_SHUTDOWN_ENTRY shutdownEntry = (PLASTCHANCE_SHUTDOWN_ENTRY)g_LastChanceShutdownQueueHead;
	LIST_ENTRY* pHead = &shutdownEntry->Entry;
	LIST_ENTRY* pEntry = pHead->Flink;
	ULONG count = 0;
	while (pEntry != pHead) { count++; pEntry = pEntry->Flink; }

	ULONG needed = (ULONG)MYARK_SHUTDOWN_CALLBACK_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	MYARK_SHUTDOWN_CALLBACK_RESPONSE* resp = (MYARK_SHUTDOWN_CALLBACK_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();

	pEntry = pHead->Flink;
	ULONG i = 0;
	while (pEntry != pHead && i < count)
	{
		PLASTCHANCE_SHUTDOWN_ENTRY record = CONTAINING_RECORD(pEntry, LASTCHANCE_SHUTDOWN_ENTRY, Entry);
		MYARK_SHUTDOWN_CALLBACK_ENTRY* entry = &resp->Entries[i];

		PVOID routine = nullptr;
		if (record->Device && MmIsAddressValid(record->Device) &&
			record->Device->DriverObject && MmIsAddressValid(record->Device->DriverObject))
			routine = (PVOID)record->Device->DriverObject->MajorFunction[IRP_MJ_SHUTDOWN];

		entry->Callback = (ULONG64)routine;
		ResolveAddressToModuleName(entry->Callback, modInfo, entry->ModuleName, MYARK_MAX_MODULE_NAME);
		ResolveAddressToModulePath(entry->Callback, modInfo, entry->ModulePath, MYARK_MAX_MODULE_PATH);

		i++;
		pEntry = pEntry->Flink;
	}

	FreeSystemModules(modInfo);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}

NTSTATUS CallbackQueryNotifyShutdown(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;
	if (!g_IopNotifyShutdownQueueHead || !MmIsAddressValid(g_IopNotifyShutdownQueueHead))
		return STATUS_UNSUCCESSFUL;

	PLASTCHANCE_SHUTDOWN_ENTRY shutdownEntry = (PLASTCHANCE_SHUTDOWN_ENTRY)g_IopNotifyShutdownQueueHead;
	LIST_ENTRY* pHead = &shutdownEntry->Entry;
	LIST_ENTRY* pEntry = pHead->Flink;
	ULONG count = 0;
	while (pEntry != pHead) { count++; pEntry = pEntry->Flink; }

	ULONG needed = (ULONG)MYARK_NOTIFY_SHUTDOWN_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	MYARK_NOTIFY_SHUTDOWN_RESPONSE* resp = (MYARK_NOTIFY_SHUTDOWN_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();

	pEntry = pHead->Flink;
	ULONG i = 0;
	while (pEntry != pHead && i < count)
	{
		PLASTCHANCE_SHUTDOWN_ENTRY record = CONTAINING_RECORD(pEntry, LASTCHANCE_SHUTDOWN_ENTRY, Entry);
		MYARK_NOTIFY_SHUTDOWN_ENTRY* entry = &resp->Entries[i];

		PVOID routine = nullptr;
		if (record->Device && MmIsAddressValid(record->Device) &&
			record->Device->DriverObject && MmIsAddressValid(record->Device->DriverObject))
			routine = (PVOID)record->Device->DriverObject->MajorFunction[IRP_MJ_SHUTDOWN];

		entry->Callback = (ULONG64)routine;
		ResolveAddressToModuleName(entry->Callback, modInfo, entry->ModuleName, MYARK_MAX_MODULE_NAME);
		ResolveAddressToModulePath(entry->Callback, modInfo, entry->ModulePath, MYARK_MAX_MODULE_PATH);

		i++;
		pEntry = pEntry->Flink;
	}

	FreeSystemModules(modInfo);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}

NTSTATUS CallbackQueryBugCheck(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;
	if (!g_BugCheckListHead || !MmIsAddressValid(g_BugCheckListHead))
		return STATUS_UNSUCCESSFUL;

	PKBUGCHECK_CALLBACK_RECORD pCallbackRecord = (PKBUGCHECK_CALLBACK_RECORD)g_BugCheckListHead;
	LIST_ENTRY* pHead = &pCallbackRecord->Entry;
	LIST_ENTRY* pEntry = pHead->Flink;
	ULONG count = 0;
	while (pEntry != pHead) { count++; pEntry = pEntry->Flink; }

	ULONG needed = (ULONG)MYARK_BUGCHECK_REGISTER_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	MYARK_BUGCHECK_REGISTER_RESPONSE* resp = (MYARK_BUGCHECK_REGISTER_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();

	pEntry = pHead->Flink;
	ULONG i = 0;
	while (pEntry != pHead && i < count)
	{
		PKBUGCHECK_CALLBACK_RECORD record = CONTAINING_RECORD(pEntry, KBUGCHECK_CALLBACK_RECORD, Entry);
		MYARK_BUGCHECK_REGISTER_ENTRY* entry = &resp->Entries[i];

		entry->Callback = (ULONG64)record->CallbackRoutine;

		ULONG compChars = 0;
		if (record->Component && MmIsAddressValid(record->Component))
		{
			compChars = (ULONG)strlen((const char*)record->Component);
			if (compChars > MYARK_MAX_MODULE_NAME - 1)
				compChars = MYARK_MAX_MODULE_NAME - 1;
			for (ULONG c = 0; c < compChars; c++)
				entry->Component[c] = (WCHAR)record->Component[c];
		}
		entry->Component[compChars] = 0;

		ResolveAddressToModuleName(entry->Callback, modInfo, entry->ModuleName, MYARK_MAX_MODULE_NAME);
		ResolveAddressToModulePath(entry->Callback, modInfo, entry->ModulePath, MYARK_MAX_MODULE_PATH);

		i++;
		pEntry = pEntry->Flink;
	}

	FreeSystemModules(modInfo);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}
