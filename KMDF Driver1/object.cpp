#include "object.h"
#include "utils.h"

// Enumerate kernel object types.
NTSTATUS KernelObjectQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten) {
	PVOID* objectTable = nullptr;


	UNICODE_STRING funcName = { 0 };
	RtlInitUnicodeString(&funcName, L"ObCreateObjectTypeEx");
	PVOID obCreateObjectTypeEx = MmGetSystemRoutineAddress(&funcName);
	if (!obCreateObjectTypeEx) {
		return STATUS_UNSUCCESSFUL;
	}

	PVOID ntModuleAddress = GetNtosBase(g_DriverObject);
	if (!ntModuleAddress) {
		return STATUS_UNSUCCESSFUL;
	}

	INT32 offset;
	PUCHAR patternIndex = (PUCHAR)PatternScan((PVOID)obCreateObjectTypeEx, 0x1000,
		"49 89 9c c4 ?? ?? ?? ??");
	if (patternIndex) {
		offset = *(INT32*)(patternIndex + 4);
		objectTable = (PVOID*)(offset + (ULONG_PTR)ntModuleAddress);
	}

	ULONG count = 0;
	for (PVOID* pCur = objectTable; *pCur != NULL; pCur++) {
		count++;
	}

	DbgPrint("[MyArkDrv] Kernel Object: count=%u\n", count);
	if (!count)
	{
		return STATUS_UNSUCCESSFUL;
	}

	ULONG needed = (ULONG)MYARK_KERNEL_OBJECT_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	MYARK_KERNEL_OBJECT_RESPONSE* resp = (MYARK_KERNEL_OBJECT_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	int index = 0;
	for (PVOID* pCur = objectTable; *pCur != NULL; pCur++,index++) {
		POBJECT_TYPE type = (POBJECT_TYPE)*pCur;
		if (!MmIsAddressValid(type)) {
			break;
		}

		MYARK_KERNEL_OBJECT_ENTRY* entry = &resp->Entries[index];

		CopyNameToWchar(entry->ObjectName, MYARK_MAX_MODULE_NAME, &type->Name);
		entry->Index = type->Index;
		entry->HandleCount = type->TotalNumberOfHandles;
		entry->ObjectCount = type->TotalNumberOfObjects;
		entry->ObjectType = (ULONG64)type;
	}

	*bytesWritten = needed;
	return STATUS_SUCCESS;
}

// Enumerate the procedures of an object type initializer.
NTSTATUS ObjectTypeProcedureQuery(PVOID inputBuffer, ULONG inputLength, PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	if (!inputBuffer || inputLength < sizeof(MYARK_OBJECT_TYPE_REQUEST))
		return STATUS_INVALID_PARAMETER;

	MYARK_OBJECT_TYPE_REQUEST* req = (MYARK_OBJECT_TYPE_REQUEST*)inputBuffer;
	POBJECT_TYPE type = (POBJECT_TYPE)req->ObjectType;
	if (!type || !MmIsAddressValid(type))
		return STATUS_INVALID_PARAMETER;

	ULONG count = MYARK_OBJECT_PROCEDURE_COUNT;
	ULONG needed = (ULONG)MYARK_OBJECT_TYPE_PROCEDURE_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	MYARK_OBJECT_TYPE_PROCEDURE_RESPONSE* resp = (MYARK_OBJECT_TYPE_PROCEDURE_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	ULONG64 procedures[MYARK_OBJECT_PROCEDURE_COUNT];
	procedures[OBJECT_PROCEDURE_ID_DUMP]         = (ULONG64)type->TypeInfo.DumpProcedure;
	procedures[OBJECT_PROCEDURE_ID_OPEN]         = (ULONG64)type->TypeInfo.OpenProcedure;
	procedures[OBJECT_PROCEDURE_ID_CLOSE]        = (ULONG64)type->TypeInfo.CloseProcedure;
	procedures[OBJECT_PROCEDURE_ID_DELETE]       = (ULONG64)type->TypeInfo.DeleteProcedure;
	procedures[OBJECT_PROCEDURE_ID_PARSE]        = (ULONG64)type->TypeInfo.ParseProcedure;
	procedures[OBJECT_PROCEDURE_ID_SECURITY]     = (ULONG64)type->TypeInfo.SecurityProcedure;
	procedures[OBJECT_PROCEDURE_ID_QUERY_NAME]   = (ULONG64)type->TypeInfo.QueryNameProcedure;
	procedures[OBJECT_PROCEDURE_ID_OKAY_TO_CLOSE]= (ULONG64)type->TypeInfo.OkayToCloseProcedure;

	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();

	for (ULONG i = 0; i < count; i++)
	{
		MYARK_OBJECT_PROCEDURE_ENTRY* e = &resp->Entries[i];
		e->ProcedureId = i;
		e->Reserved = 0;
		e->Address = procedures[i];
		ResolveAddressToModuleName(procedures[i], modInfo, e->ModuleName, MYARK_MAX_MODULE_NAME);
		ResolveAddressToModulePath(procedures[i], modInfo, e->ModulePath, MYARK_MAX_MODULE_PATH);
	}

	FreeSystemModules(modInfo);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}