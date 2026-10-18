#include "utils.h"
#include <ntimage.h>

PRTL_PROCESS_MODULES QuerySystemModules()
{
	ULONG size = 0;
	ZwQuerySystemInformation(SystemModuleInformation, nullptr, 0, &size);
	if (size == 0)
		return nullptr;

	PRTL_PROCESS_MODULES modInfo = (PRTL_PROCESS_MODULES)ExAllocatePool2(
		POOL_FLAG_NON_PAGED, size, 'kMrA');
	if (!modInfo)
		return nullptr;

	if (!NT_SUCCESS(ZwQuerySystemInformation(SystemModuleInformation, modInfo, size, &size)))
	{
		ExFreePoolWithTag(modInfo, 'kMrA');
		return nullptr;
	}
	return modInfo;
}

void FreeSystemModules(PRTL_PROCESS_MODULES modInfo)
{
	if (modInfo)
		ExFreePoolWithTag(modInfo, 'kMrA');
}

void ResolveAddressToModuleName(ULONG64 addr, PRTL_PROCESS_MODULES modInfo, WCHAR* outName, ULONG maxChars)
{
	RtlZeroMemory(outName, maxChars * sizeof(WCHAR));
	if (!modInfo || !addr)
		return;

	for (ULONG m = 0; m < modInfo->NumberOfModules; m++)
	{
		ULONG64 base = (ULONG64)modInfo->Modules[m].ImageBase;
		ULONG64 size = modInfo->Modules[m].ImageSize;
		if (addr >= base && addr < base + size)
		{
			const char* fullPath = (const char*)modInfo->Modules[m].FullPathName;
			const char* name = fullPath + modInfo->Modules[m].OffsetToFileName;
			ANSI_STRING ansi;
			RtlInitAnsiString(&ansi, name);
			UNICODE_STRING uni = {};
			if (NT_SUCCESS(RtlAnsiStringToUnicodeString(&uni, &ansi, TRUE)))
			{
				ULONG copyLen = uni.Length / sizeof(WCHAR);
				if (copyLen >= maxChars)
					copyLen = maxChars - 1;
				RtlCopyMemory(outName, uni.Buffer, copyLen * sizeof(WCHAR));
				RtlFreeUnicodeString(&uni);
			}
			break;
		}
	}
}

void ResolveAddressToModulePath(ULONG64 addr, PRTL_PROCESS_MODULES modInfo, WCHAR* outPath, ULONG maxChars)
{
	RtlZeroMemory(outPath, maxChars * sizeof(WCHAR));
	if (!modInfo || !addr)
		return;

	for (ULONG m = 0; m < modInfo->NumberOfModules; m++)
	{
		ULONG64 base = (ULONG64)modInfo->Modules[m].ImageBase;
		ULONG64 size = modInfo->Modules[m].ImageSize;
		if (addr >= base && addr < base + size)
		{
			const char* fullPath = (const char*)modInfo->Modules[m].FullPathName;
			ANSI_STRING ansi;
			RtlInitAnsiString(&ansi, fullPath);
			UNICODE_STRING uni = {};
			if (NT_SUCCESS(RtlAnsiStringToUnicodeString(&uni, &ansi, TRUE)))
			{
				ULONG srcLen = uni.Length / sizeof(WCHAR);
				ULONG copyLen = srcLen;
				if (copyLen >= maxChars)
					copyLen = maxChars - 1;
				RtlCopyMemory(outPath, uni.Buffer, copyLen * sizeof(WCHAR));
				outPath[copyLen] = 0;
				RtlFreeUnicodeString(&uni);
			}
			break;
		}
	}
}

void CopyNameToWchar(PWCHAR out, ULONG maxChars, PCUNICODE_STRING name)
{
	if (!name || !name->Buffer)
	{
		out[0] = 0;
		return;
	}
	ULONG chars = name->Length / sizeof(WCHAR);
	if (chars > maxChars - 1)
		chars = maxChars - 1;
	for (ULONG i = 0; i < chars; i++)
		out[i] = name->Buffer[i];
	out[chars] = 0;
}

void GetDeviceObjectName(PVOID object, PWCHAR out, ULONG maxChars)
{
	out[0] = 0;
	if (!object)
		return;

	// OBJECT_HEADER 位于对象前方，NAME_INFO 位于 OBJECT_HEADER 前方
	POBJECT_HEADER_NAME_INFO nameInfo =
		(POBJECT_HEADER_NAME_INFO)((ULONG_PTR)object - 0x30 - sizeof(OBJECT_HEADER_NAME_INFO));
	if (nameInfo && MmIsAddressValid(nameInfo) && nameInfo->Name.Buffer && nameInfo->Name.Length > 0)
		CopyNameToWchar(out, maxChars, &nameInfo->Name);
}

PVOID GetNtosBase(PDRIVER_OBJECT DrvObj)
{
	if (!DrvObj || !DrvObj->DriverSection)
		return NULL;

	PKLDR_DATA_TABLE_ENTRY pCur = (PKLDR_DATA_TABLE_ENTRY)DrvObj->DriverSection;
	PLIST_ENTRY head = &pCur->InLoadOrderLinks;
	PLIST_ENTRY iter = head->Flink;

	while (iter != head && iter != NULL)
	{
		PKLDR_DATA_TABLE_ENTRY entry = CONTAINING_RECORD(iter, KLDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

		if (entry && entry->BaseDllName.Buffer && entry->BaseDllName.Length > 0)
		{
			if (_wcsicmp(entry->BaseDllName.Buffer, L"ntoskrnl.exe") == 0 ||
				_wcsicmp(entry->BaseDllName.Buffer, L"ntkrnlmp.exe") == 0)
			{
				return entry->DllBase;
			}
		}

		iter = iter->Flink;
	}

	return NULL;
}

PVOID GetModuleBase(PDRIVER_OBJECT DrvObj, PCWSTR moduleName)
{
	if (!DrvObj || !DrvObj->DriverSection)
		return NULL;

	PKLDR_DATA_TABLE_ENTRY pCur = (PKLDR_DATA_TABLE_ENTRY)DrvObj->DriverSection;
	PLIST_ENTRY head = &pCur->InLoadOrderLinks;
	PLIST_ENTRY iter = head->Flink;

	while (iter != head && iter != NULL)
	{
		PKLDR_DATA_TABLE_ENTRY entry = CONTAINING_RECORD(iter, KLDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

		if (entry && entry->BaseDllName.Buffer && entry->BaseDllName.Length > 0)
		{
			if (_wcsicmp(entry->BaseDllName.Buffer, moduleName) == 0)
			{
				return entry->DllBase;
			}
		}

		iter = iter->Flink;
	}

	return NULL;
}

static LONG CompareAnsiString(PCSTR a, PCSTR b)
{
	while (*a && *a == *b)
	{
		a++;
		b++;
	}
	return (LONG)(UCHAR)*a - (LONG)(UCHAR)*b;
}

PVOID GetExportByName(PVOID moduleBase, PCSTR functionName)
{
	if (!moduleBase || !functionName || !MmIsAddressValid(moduleBase)) {
		return nullptr;
	}

	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)moduleBase;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
		return nullptr;
	}

	PIMAGE_NT_HEADERS64 nth = (PIMAGE_NT_HEADERS64)((ULONG_PTR)moduleBase + dos->e_lfanew);
	if (nth->Signature != IMAGE_NT_SIGNATURE) {
		return nullptr;
	}

	ULONG exportRva = nth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	if (!exportRva) {
		return nullptr;
	}

	PIMAGE_EXPORT_DIRECTORY expDir = (PIMAGE_EXPORT_DIRECTORY)((ULONG_PTR)moduleBase + exportRva);
	ULONG numNames = expDir->NumberOfNames;
	if (!numNames) {
		return nullptr;
	}
	PULONG eat = (PULONG)((ULONG_PTR)moduleBase + expDir->AddressOfFunctions);
	PULONG ent = (PULONG)((ULONG_PTR)moduleBase + expDir->AddressOfNames);
	PUSHORT eot = (PUSHORT)((ULONG_PTR)moduleBase + expDir->AddressOfNameOrdinals);


	for (ULONG i = 0; i < numNames; i++)
	{
		PCSTR name = (PCSTR)((ULONG_PTR)moduleBase + ent[i]);
		if (CompareAnsiString(name, functionName) == 0)
			return (PVOID)((ULONG_PTR)moduleBase + eat[eot[i]]);
	}
	DbgPrint("exit");
	return nullptr;
}
