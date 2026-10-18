#include "filter.h"
#include "utils.h"


#define MYARK_MAX_OPS_PER_FILTER 128

// Enumerate minifilters and their callbacks.
NTSTATUS FilterQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	ULONG numFilters = 0;
	NTSTATUS status = FltEnumerateFilters(nullptr, 0, &numFilters);
	if (status != STATUS_BUFFER_TOO_SMALL)
		return status;

	if (!numFilters)
		return STATUS_SUCCESS;

	PFLT_FILTER* ppFilters = (PFLT_FILTER*)ExAllocatePool2(
		POOL_FLAG_PAGED, sizeof(PFLT_FILTER) * numFilters, 'kram');
	if (!ppFilters)
		return STATUS_INSUFFICIENT_RESOURCES;

	status = FltEnumerateFilters(ppFilters, numFilters, &numFilters);
	if (!NT_SUCCESS(status))
	{
		ExFreePool(ppFilters);
		return status;
	}

	ULONG totalOps = 0;
	for (ULONG i = 0; i < numFilters; i++)
	{
		PFLT_FILTER f = ppFilters[i];
		if (!f || !MmIsAddressValid(f))
			continue;
		PFLT_OPERATION_REGISTRATION opReg = f->Operations;
		if (!opReg || !MmIsAddressValid(opReg))
			continue;
		for (ULONG n = 0; n < MYARK_MAX_OPS_PER_FILTER; n++, opReg++)
		{
			if (!MmIsAddressValid(opReg) || opReg->MajorFunction == IRP_MJ_OPERATION_END)
				break;
			if (opReg->MajorFunction < IRP_MJ_MAXIMUM_FUNCTION)
				totalOps++;
		}
	}

	ULONG needed = sizeof(MYARK_RESPONSE_HEADER)
		+ numFilters * sizeof(MYARK_FILTER_ENTRY)
		+ totalOps * sizeof(MYARK_FILTER_OPERATION_ENTRY);
	if (outputBufferLength < needed)
	{
		ExFreePool(ppFilters);
		return STATUS_BUFFER_TOO_SMALL;
	}

	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();

	MYARK_FILTER_RESPONSE* resp = (MYARK_FILTER_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = numFilters;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	UCHAR* p = (UCHAR*)outputBuffer + sizeof(MYARK_RESPONSE_HEADER);
	for (ULONG i = 0; i < numFilters; i++)
	{
		PFLT_FILTER f = ppFilters[i];
		MYARK_FILTER_ENTRY* entry = (MYARK_FILTER_ENTRY*)p;

		if (!f || !MmIsAddressValid(f))
		{
			RtlZeroMemory(entry, sizeof(MYARK_FILTER_ENTRY));
			p += sizeof(MYARK_FILTER_ENTRY);
			continue;
		}

		CopyNameToWchar(entry->Name, MYARK_MAX_MODULE_NAME, &f->Name);

		ULONG64 moduleAddr = 0;
		if (f->DriverObject && MmIsAddressValid(f->DriverObject))
			moduleAddr = (ULONG64)f->DriverObject->DriverStart;
		ResolveAddressToModuleName(moduleAddr, modInfo, entry->ModuleName, MYARK_MAX_MODULE_NAME);
		ResolveAddressToModulePath(moduleAddr, modInfo, entry->ModulePath, MYARK_MAX_MODULE_PATH);

		entry->FilterAddress = (ULONG64)f;
		entry->Callbacks.FilterUnloadCallback = (ULONG64)f->FilterUnload;
		entry->Callbacks.InstanceSetupCallback = (ULONG64)f->InstanceSetup;
		entry->Callbacks.InstanceQueryTeardownCallback = (ULONG64)f->InstanceQueryTeardown;
		entry->Callbacks.InstanceTeardownStartCallback = (ULONG64)f->InstanceTeardownStart;
		entry->Callbacks.InstanceTeardownCompleteCallback = (ULONG64)f->InstanceTeardownComplete;
		entry->Callbacks.GenerateFileNameCallback = (ULONG64)f->GenerateFileName;
		entry->Callbacks.NormalizeNameComponentCallback = (ULONG64)f->NormalizeNameComponent;
		entry->Callbacks.NormalizeContextCleanupCallback = (ULONG64)f->NormalizeContextCleanup;
		entry->Callbacks.TransactionNotificationCallback = (ULONG64)f->KtmNotification;
		entry->Callbacks.NormalizeNameComponentExCallback = (ULONG64)f->NormalizeNameComponentEx;
		entry->Callbacks.SectionNotificationCallback = (ULONG64)f->SectionNotification;
		entry->Reserved = 0;

		MYARK_FILTER_OPERATION_ENTRY* ops = (MYARK_FILTER_OPERATION_ENTRY*)(entry + 1);
		entry->OperationCount = 0;

		PFLT_OPERATION_REGISTRATION opReg = f->Operations;
		if (opReg && MmIsAddressValid(opReg))
		{
			for (ULONG n = 0; n < MYARK_MAX_OPS_PER_FILTER; n++, opReg++)
			{
				if (!MmIsAddressValid(opReg) || opReg->MajorFunction == IRP_MJ_OPERATION_END)
					break;
				if (opReg->MajorFunction >= IRP_MJ_MAXIMUM_FUNCTION)
					continue;
				ops[entry->OperationCount].MajorFunction = opReg->MajorFunction;
				ops[entry->OperationCount].Reserved = 0;
				ops[entry->OperationCount].PreOperation = (ULONG64)opReg->PreOperation;
				ops[entry->OperationCount].PostOperation = (ULONG64)opReg->PostOperation;
				entry->OperationCount++;
			}
		}

		p = (UCHAR*)(ops + entry->OperationCount);
	}

	FreeSystemModules(modInfo);
	ExFreePool(ppFilters);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}
