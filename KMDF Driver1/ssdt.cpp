#include "ssdt.h"
#include <ntimage.h>
#include "utils.h"

typedef struct _EXPORT_ENTRY {
	ULONG64 Address;
	PCSTR   Name;
} EXPORT_ENTRY;

static MYARK_SSDT_ENTRY* g_SsdtEntries = nullptr;
static ULONG g_SsdtCount = 0;
static MYARK_SSDT_ENTRY* g_ShadowSsdtEntries = nullptr;
static ULONG g_ShadowSsdtCount = 0;
static PVOID g_KernelBase = nullptr;
static PVOID g_Win32kBase = nullptr;
static UINT64 g_ShadowSelfTestAddress = 0;

static UINT64 GetSsdtBase()
{
	UINT64 searchBase = NULL;
	UINT64 ssdtBase = NULL;
	UINT64 kiSystemServiceUser = NULL;
	UINT64 msr = __readmsr(0xC0000082);

	searchBase = msr;
	PUCHAR hit = (PUCHAR)PatternScan((PVOID)msr, 0x50, "0f 22 dc");//mov cr3,rsp
	if (hit) {//kernel isolation enabled
		PUCHAR patternIndex = (PUCHAR)PatternScan(
			(PVOID)msr, 0x500, "e9 ?? ?? ?? ??");
		if (patternIndex) {
			INT32 offset = *(INT32*)(patternIndex + 1);
			kiSystemServiceUser = ((ULONG_PTR)patternIndex + 5 + (INT64)offset);
		}

		searchBase = kiSystemServiceUser;
	}

	PUCHAR patternIndex = (PUCHAR)PatternScan((PVOID)searchBase, 0x500,
		"4c 8d 15 ?? ?? ?? ?? 4c 8d 1d ?? ?? ?? ?? f7 43");
	if (patternIndex) {
		INT32 offset = *(INT32*)(patternIndex + 3);
		ssdtBase = ((ULONG_PTR)patternIndex + 7 + (INT64)offset);
	}
	return ssdtBase;
}

static UINT64 GetShadowSsdtBase()
{
	UINT64 searchBase = NULL;
	UINT64 ssdtBase = NULL;
	UINT64 kiSystemServiceUser = NULL;
	UINT64 msr = __readmsr(0xC0000082);

	searchBase = msr;
	PUCHAR hit = (PUCHAR)PatternScan((PVOID)msr, 0x50, "0f 22 dc");//mov cr3,rsp
	if (hit) {//kernel isolation enabled
		PUCHAR patternIndex = (PUCHAR)PatternScan(
			(PVOID)msr, 0x500, "e9 ?? ?? ?? ??");
		if (patternIndex) {
			INT32 offset = *(INT32*)(patternIndex + 1);
			kiSystemServiceUser = ((ULONG_PTR)patternIndex + 5 + (INT64)offset);
		}

		searchBase = kiSystemServiceUser;
	}

	PUCHAR patternIndex = (PUCHAR)PatternScan((PVOID)searchBase, 0x500,
		"4c 8d 15 ?? ?? ?? ?? 4c 8d 1d ?? ?? ?? ?? f7 43");
	if (patternIndex) {
		INT32 offset = *(INT32*)(patternIndex + 10);
		ssdtBase = ((ULONG_PTR)patternIndex + 14 + (INT64)offset);
	}
	return ssdtBase;
}

static PVOID GetKernelBase()
{
	ULONG_PTR addr = (ULONG_PTR)KeSetTimerEx & ~(ULONG_PTR)0xFFF;
	while (addr > 0x100000) {
		if (MmIsAddressValid((PVOID)addr) && *(USHORT*)addr == 0x5A4D) // "MZ"
			return (PVOID)addr;
		addr -= 0x1000;
	}
	return nullptr;
}

static PVOID FindModuleBaseByName(PRTL_PROCESS_MODULES mods, const WCHAR* name)
{
	if (!mods)
		return nullptr;
	for (ULONG i = 0; i < mods->NumberOfModules; i++) {
		PUCHAR fullPath = mods->Modules[i].FullPathName;
		PWCHAR fileName = (PWCHAR)(fullPath + mods->Modules[i].OffsetToFileName);
		ULONG nameLen = 0;
		while (name[nameLen]) nameLen++;
		ULONG j = 0;
		for (; fileName[j] && j < 256; j++) {
			ULONG k = 0;
			while (k < nameLen &&
				(RtlUpcaseUnicodeChar(fileName[j + k]) == RtlUpcaseUnicodeChar(name[k])))
				k++;
			if (k == nameLen)
				return mods->Modules[i].ImageBase;
		}
	}
	return nullptr;
}

static void SwapExports(EXPORT_ENTRY* a, EXPORT_ENTRY* b)
{
	EXPORT_ENTRY t = *a;
	*a = *b;
	*b = t;
}

static void QuickSortExports(EXPORT_ENTRY* arr, LONG left, LONG right)
{
	if (left >= right)
		return;
	ULONG64 pivot = arr[(left + right) / 2].Address;
	LONG i = left, j = right;
	while (i <= j) {
		while (arr[i].Address < pivot) i++;
		while (arr[j].Address > pivot) j--;
		if (i <= j) {
			SwapExports(&arr[i], &arr[j]);
			i++;
			j--;
		}
	}
	if (left < j)  QuickSortExports(arr, left, j);
	if (i < right) QuickSortExports(arr, i, right);
}

static PCSTR FindExportName(EXPORT_ENTRY* exports, ULONG count, ULONG64 addr)
{
	LONG left = 0, right = (LONG)count - 1;
	while (left <= right) {
		LONG mid = (left + right) / 2;
		if (exports[mid].Address == addr)
			return exports[mid].Name;
		if (exports[mid].Address < addr)
			left = mid + 1;
		else
			right = mid - 1;
	}
	return nullptr;
}

static BOOLEAN BuildExportTable(PVOID moduleBase, EXPORT_ENTRY** outExports, ULONG* outCount)
{
	*outExports = nullptr;
	*outCount = 0;
	if (!moduleBase || !MmIsAddressValid(moduleBase))
		return FALSE;

	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)moduleBase;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		return FALSE;

	PIMAGE_NT_HEADERS64 nth = (PIMAGE_NT_HEADERS64)((ULONG_PTR)moduleBase + dos->e_lfanew);
	if (nth->Signature != IMAGE_NT_SIGNATURE)
		return FALSE;

	ULONG exportRva = nth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	if (!exportRva || !MmIsAddressValid((PVOID)((ULONG_PTR)moduleBase + exportRva)))
		return FALSE;

	PIMAGE_EXPORT_DIRECTORY expDir = (PIMAGE_EXPORT_DIRECTORY)((ULONG_PTR)moduleBase + exportRva);
	ULONG count = expDir->NumberOfNames;
	if (!count)
		return FALSE;

	EXPORT_ENTRY* exports = (EXPORT_ENTRY*)ExAllocatePool2(
		POOL_FLAG_NON_PAGED, sizeof(EXPORT_ENTRY) * count, 'yrak');
	if (!exports)
		return FALSE;

	PULONG eat = (PULONG)((ULONG_PTR)moduleBase + expDir->AddressOfFunctions);
	PULONG ent = (PULONG)((ULONG_PTR)moduleBase + expDir->AddressOfNames);
	PUSHORT eot = (PUSHORT)((ULONG_PTR)moduleBase + expDir->AddressOfNameOrdinals);

	for (ULONG i = 0; i < count; i++) {
		exports[i].Name = (PCSTR)((ULONG_PTR)moduleBase + ent[i]);
		exports[i].Address = (ULONG64)moduleBase + eat[eot[i]];
	}
	QuickSortExports(exports, 0, (LONG)count - 1);

	*outExports = exports;
	*outCount = count;
	return TRUE;
}

static void CopyNameToWchar(PCSTR name, WCHAR* out, ULONG maxChars)
{
	if (!name) {
		out[0] = 0;
		return;
	}
	ULONG i = 0;
	for (; i < maxChars - 1 && name[i]; i++)
		out[i] = (WCHAR)(UCHAR)name[i];
	out[i] = 0;
}

static UINT64 DecodeServiceAddress(PULONG lpBase, ULONG index)
{
	ULONG dwOffset = lpBase[index];
	if (dwOffset & 0x80000000)
		dwOffset = (dwOffset >> 4) | 0xF0000000;
	else
		dwOffset >>= 4;
	return (UINT64)((ULONG_PTR)lpBase + (LONG)dwOffset);
}

static void ShadowSsdtInitInternal()
{
	if (g_ShadowSsdtEntries && g_ShadowSsdtCount)
		return;

	PSYSTEM_SERVICE_TABLE shadow = (PSYSTEM_SERVICE_TABLE)GetShadowSsdtBase();
	if (!shadow || !MmIsAddressValid(shadow))
		return;

	PSYSTEM_SERVICE_TABLE win32kTable = &shadow[1];
	if (!MmIsAddressValid(win32kTable))
		return;

	ULONG numServices = (ULONG)win32kTable->NumberOfServices;
	PULONG lpBase = (PULONG)win32kTable->ServiceTableBase;
	if (!numServices || !MmIsAddressValid(lpBase))
		return;

	// win32k service addresses live in win32kfull.sys (session space)
	PRTL_PROCESS_MODULES mods = QuerySystemModules();
	PVOID win32kBase = FindModuleBaseByName(mods, L"win32kfull.sys");
	FreeSystemModules(mods);
	g_Win32kBase = win32kBase;

	EXPORT_ENTRY* exports = nullptr;
	ULONG exportCount = 0;
	BuildExportTable(win32kBase, &exports, &exportCount);

	g_ShadowSsdtEntries = (MYARK_SSDT_ENTRY*)ExAllocatePool2(
		POOL_FLAG_NON_PAGED, sizeof(MYARK_SSDT_ENTRY) * numServices, 'yrak');
	if (!g_ShadowSsdtEntries) {
		if (exports)
			ExFreePool(exports);
		return;
	}
	RtlZeroMemory(g_ShadowSsdtEntries, sizeof(MYARK_SSDT_ENTRY) * numServices);
	g_ShadowSsdtCount = numServices;

	for (ULONG i = 0; i < numServices; i++) {
		UINT64 lpAddr = DecodeServiceAddress(lpBase, i);

		g_ShadowSsdtEntries[i].Index = i;
		g_ShadowSsdtEntries[i].Reserved = 0;
		g_ShadowSsdtEntries[i].Address = lpAddr;
		CopyNameToWchar(FindExportName(exports, exportCount, lpAddr),
			g_ShadowSsdtEntries[i].FunctionName, MYARK_MAX_MODULE_NAME);

		if (!g_ShadowSelfTestAddress && g_ShadowSsdtEntries[i].FunctionName[0])
			g_ShadowSelfTestAddress = lpAddr;
	}

	if (exports)
		ExFreePool(exports);

	DbgPrint("[MyArkDrv] Shadow SSDT initialized: %u win32k services, %u exports.\n",
		numServices, exportCount);
}

void SsdtInit()
{
	PSYSTEM_SERVICE_TABLE ssdt = (PSYSTEM_SERVICE_TABLE)GetSsdtBase();
	if (!ssdt || !MmIsAddressValid(ssdt))
		return;

	ULONG numServices = (ULONG)ssdt->NumberOfServices;
	PULONG lpBase = (PULONG)ssdt->ServiceTableBase;

	g_SsdtEntries = (MYARK_SSDT_ENTRY*)ExAllocatePool2(
		POOL_FLAG_NON_PAGED, sizeof(MYARK_SSDT_ENTRY) * numServices, 'yrak');
	if (!g_SsdtEntries)
		return;
	RtlZeroMemory(g_SsdtEntries, sizeof(MYARK_SSDT_ENTRY) * numServices);
	g_SsdtCount = numServices;

	g_KernelBase = GetKernelBase();
	EXPORT_ENTRY* exports = nullptr;
	ULONG exportCount = 0;
	BuildExportTable(g_KernelBase, &exports, &exportCount);

	for (ULONG i = 0; i < numServices; i++) {
		UINT64 lpAddr = DecodeServiceAddress(lpBase, i);

		g_SsdtEntries[i].Index = i;
		g_SsdtEntries[i].Reserved = 0;
		g_SsdtEntries[i].Address = lpAddr;
		CopyNameToWchar(FindExportName(exports, exportCount, lpAddr),
			g_SsdtEntries[i].FunctionName, MYARK_MAX_MODULE_NAME);
	}

	if (exports)
		ExFreePool(exports);

	DbgPrint("[MyArkDrv] SSDT initialized: %u services, %u exports.\n", numServices, exportCount);

	ShadowSsdtInitInternal();
}

NTSTATUS SsdtQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;
	if (!g_SsdtEntries || !g_SsdtCount)
		return STATUS_UNSUCCESSFUL;

	ULONG needed = (ULONG)MYARK_SSDT_RESPONSE_SIZE(g_SsdtCount);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	MYARK_SSDT_RESPONSE* resp = (MYARK_SSDT_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = g_SsdtCount;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;
	resp->ModuleBase = (ULONG64)g_KernelBase;
	resp->SelfTestAddress = (ULONG64)KeSetTimerEx;

	for (ULONG i = 0; i < g_SsdtCount; i++)
		resp->Entries[i] = g_SsdtEntries[i];

	*bytesWritten = needed;
	return STATUS_SUCCESS;
}

NTSTATUS SsdtQueryShadow(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	ShadowSsdtInitInternal();
	if (!g_ShadowSsdtEntries || !g_ShadowSsdtCount)
		return STATUS_UNSUCCESSFUL;

	ULONG needed = (ULONG)MYARK_SSDT_RESPONSE_SIZE(g_ShadowSsdtCount);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	MYARK_SSDT_RESPONSE* resp = (MYARK_SSDT_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = g_ShadowSsdtCount;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;
	resp->ModuleBase = (ULONG64)g_Win32kBase;
	resp->SelfTestAddress = g_ShadowSelfTestAddress;

	for (ULONG i = 0; i < g_ShadowSsdtCount; i++)
		resp->Entries[i] = g_ShadowSsdtEntries[i];

	*bytesWritten = needed;
	return STATUS_SUCCESS;
}
