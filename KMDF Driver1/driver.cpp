#include "driver.h"
#include "utils.h"

extern PDRIVER_OBJECT g_DriverObject;

PVOID g_MiRememberUnloadedDriver;
PVOID I_PEUpdateHashCache;
PVOID g_MmUnloadedDrivers;

PVOID g_PiLookupInDDBCache;
PVOID g_PiDDBCacheTable;
PVOID g_KernelHashBucketList;

// Enumerate loaded driver objects from the object directory.
NTSTATUS DriverQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	if (!g_DriverObject || !g_DriverObject->DriverSection)
		return STATUS_UNSUCCESSFUL;

	// First pass: count entries
	ULONG count = 0;
	POBJECT_HEADER header = (POBJECT_HEADER)((ULONG_PTR)g_DriverObject - 0x30);
	POBJECT_HEADER_NAME_INFO headerNameInfo = (POBJECT_HEADER_NAME_INFO)((ULONG_PTR)header - sizeof(OBJECT_HEADER_NAME_INFO));
	POBJECT_DIRECTORY directory = headerNameInfo->Directory;
	for (int i = 0;i < 37;i++) {
		POBJECT_DIRECTORY_ENTRY entry = directory->HashBuckets[i];
		if (!entry) {
			continue;
		}

		POBJECT_DIRECTORY_ENTRY subEntry = entry;
		while (subEntry != nullptr) {
			count++;
			subEntry = subEntry->ChainLink;
		}
	}

	if (!count)
		return STATUS_UNSUCCESSFUL;

	ULONG needed = (ULONG)MYARK_DRIVER_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	MYARK_DRIVER_RESPONSE* resp = (MYARK_DRIVER_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	// Second pass: fill entries
	ULONG index = 0;
	for (int i = 0;i < 37;i++) {
		POBJECT_DIRECTORY_ENTRY entry = directory->HashBuckets[i];
		if (!entry) {
			continue;
		}

		POBJECT_DIRECTORY_ENTRY subEntry = entry;
		while (subEntry != nullptr && index < count) {
			PDRIVER_OBJECT object = (PDRIVER_OBJECT)subEntry->Object;
			if (object && MmIsAddressValid(object) && object->DriverSection && MmIsAddressValid(object->DriverSection))
			{
				PKLDR_DATA_TABLE_ENTRY tableEntry = (PKLDR_DATA_TABLE_ENTRY)object->DriverSection;
				POBJECT_HEADER_NAME_INFO nameInfo = (POBJECT_HEADER_NAME_INFO)((ULONG_PTR)object - 0x30 - sizeof(OBJECT_HEADER_NAME_INFO));
				MYARK_DRIVER_ENTRY* e = &resp->Entries[index];

				CopyNameToWchar(e->Name, 64, &tableEntry->BaseDllName);
				CopyNameToWchar(e->Path, 260, &tableEntry->FullDllName);
				e->BaseAddress = (ULONG64)tableEntry->DllBase;
				e->Size = tableEntry->SizeOfImage;
				e->Reserved = 0;
				e->DriverObject = (ULONG64)object;
				if (nameInfo && MmIsAddressValid(nameInfo))
					CopyNameToWchar(e->ObjectName, 64, &nameInfo->Name);

				index++;
			}
			subEntry = subEntry->ChainLink;
		}
	}

	resp->Header.Count = index;
	*bytesWritten = (ULONG)MYARK_DRIVER_RESPONSE_SIZE(index);
	return STATUS_SUCCESS;
}

// Enumerate unloaded drivers from MmUnloadedDrivers.
NTSTATUS UnloadDriverQuery1(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	if (!g_DriverObject)
		return STATUS_UNSUCCESSFUL;

	PVOID ntBase = GetNtosBase(g_DriverObject);
	if (!ntBase)
		return STATUS_UNSUCCESSFUL;

	if (!g_MiRememberUnloadedDriver)
	{
		UCHAR* hit = (UCHAR*)PatternScan(ntBase, 0x1000000,
			"48 8b c4 48 89 58 08 48 89 68 10 48 89 70 18 48 89 78 20 41 56 48 83 ec 20 45 33 f6 41 8b e8 48 8b f2 48 8b f9 66 44 39 31 0f");
		if (hit)
		{
			g_MiRememberUnloadedDriver = (PVOID)hit;
		}
	}

	if (!g_MmUnloadedDrivers)
	{
		UCHAR* hit2 = (UCHAR*)PatternScan(g_MiRememberUnloadedDriver, 0x50, "4c 8b 0d ?? ?? ?? ??");
		if (hit2)
		{
			INT32 offset = *(INT32*)(hit2 + 3);
			PVOID rawSym = (PVOID)((ULONG_PTR)hit2 + 7 + offset);
			PVOID deref = (PVOID)0;
			if (MmIsAddressValid(rawSym))
				deref = (PVOID) * (UINT64*)rawSym;
			g_MmUnloadedDrivers = (deref && MmIsAddressValid(deref)) ? deref : rawSym;
		}
	}

	if (!g_MmUnloadedDrivers || !MmIsAddressValid(g_MmUnloadedDrivers))
		return STATUS_UNSUCCESSFUL;

	// First pass: count valid entries (validate each UnloadDriverEntry and its name pointer)
	ULONG count = 0;
	for (ULONG i = 0; i < 32; i++)
	{
		UnloadDriverEntry* entry = (UnloadDriverEntry*)((ULONG_PTR)g_MmUnloadedDrivers + i * sizeof(UnloadDriverEntry));
		if (!MmIsAddressValid(entry))
			break;
		if (!entry->driverName.Buffer || entry->driverName.Length == 0)
			continue;
		if (!MmIsAddressValid(entry->driverName.Buffer))
			continue;
		count++;
	}

	if (!count)
		return STATUS_UNSUCCESSFUL;

	ULONG needed = (ULONG)MYARK_UNLOADED_DRIVER_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	RtlZeroMemory(outputBuffer, needed);

	MYARK_UNLOADED_DRIVER_RESPONSE* resp = (MYARK_UNLOADED_DRIVER_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	// Second pass: fill entries
	ULONG index = 0;
	for (ULONG i = 0; i < 32 && index < count; i++)
	{
		UnloadDriverEntry* entry = (UnloadDriverEntry*)((ULONG_PTR)g_MmUnloadedDrivers + i * sizeof(UnloadDriverEntry));
		if (!MmIsAddressValid(entry))
			break;
		if (!entry->driverName.Buffer || entry->driverName.Length == 0)
			continue;
		if (!MmIsAddressValid(entry->driverName.Buffer))
			continue;

		MYARK_UNLOADED_DRIVER_ENTRY* e = &resp->Entries[index];
		e->DriverStart = (ULONG64)entry->driverStartAddress;
		e->DriverEnd = (ULONG64)entry->driverEndAddress;

		ULONG chars = entry->driverName.Length / sizeof(WCHAR);
		if (chars > MYARK_MAX_MODULE_NAME - 1)
			chars = MYARK_MAX_MODULE_NAME - 1;
		for (ULONG k = 0; k < chars; k++)
			e->DriverName[k] = entry->driverName.Buffer[k];
		e->DriverName[chars] = 0;

		index++;
	}

	resp->Header.Count = index;
	*bytesWritten = (ULONG)MYARK_UNLOADED_DRIVER_RESPONSE_SIZE(index);
	return STATUS_SUCCESS;
}

// Enumerate unloaded drivers from the PiDDBCacheTable.
NTSTATUS UnloadDriverQuery2(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	if (!g_DriverObject)
		return STATUS_UNSUCCESSFUL;

	PVOID ntBase = GetNtosBase(g_DriverObject);
	if (!ntBase)
		return STATUS_UNSUCCESSFUL;

	if (!g_PiDDBCacheTable)
	{
		UCHAR* hit = (UCHAR*)PatternScan(ntBase, 0x1000000,
			"40 53 55 56 57 41 56 48 83 ec 60 48 8b 05 ?? ?? ?? ?? 48 33 c4 48 89 44 24 58 0f 57 c0 48 8b f9 33 c0 48 8d 0d");
		if (hit)
		{
			INT32 offset = *(INT32*)(hit + 37);
			g_PiDDBCacheTable = (PVOID)((ULONG_PTR)hit + 41 + offset);
		}
	}

	if (!g_PiDDBCacheTable || !MmIsAddressValid(g_PiDDBCacheTable))
		return STATUS_UNSUCCESSFUL;

	ULONG count = 0;
	for (PiDDBCacheEntry* p = (PiDDBCacheEntry*)RtlEnumerateGenericTableAvl((PRTL_AVL_TABLE)g_PiDDBCacheTable, TRUE);
		p != NULL;
		p = (PiDDBCacheEntry*)RtlEnumerateGenericTableAvl((PRTL_AVL_TABLE)g_PiDDBCacheTable, FALSE)) {
		count++;
	}

	if (!count)
		return STATUS_UNSUCCESSFUL;

	ULONG needed = (ULONG)MYARK_UNLOADED_DRIVER_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	RtlZeroMemory(outputBuffer, needed);

	MYARK_UNLOADED_DRIVER_RESPONSE* resp = (MYARK_UNLOADED_DRIVER_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	// Second pass: fill entries
	ULONG index = 0;
	for (PiDDBCacheEntry* p = (PiDDBCacheEntry*)RtlEnumerateGenericTableAvl((PRTL_AVL_TABLE)g_PiDDBCacheTable, TRUE);
		p != NULL;
		p = (PiDDBCacheEntry*)RtlEnumerateGenericTableAvl((PRTL_AVL_TABLE)g_PiDDBCacheTable, FALSE)) {

		MYARK_UNLOADED_DRIVER_ENTRY* e = &resp->Entries[index];
		e->DriverStart = 0;
		e->DriverEnd = 0;
		
		ULONG chars = p->DriverName.Length / sizeof(WCHAR);
		if (chars > MYARK_MAX_MODULE_NAME - 1)
			chars = MYARK_MAX_MODULE_NAME - 1;
		for (ULONG k = 0; k < chars; k++)
			e->DriverName[k] = p->DriverName.Buffer[k];
		e->DriverName[chars] = 0;

		index++;
	}

	resp->Header.Count = index;
	*bytesWritten = (ULONG)MYARK_UNLOADED_DRIVER_RESPONSE_SIZE(index);
	return STATUS_SUCCESS;
}

// Enumerate unloaded drivers from the ci.dll kernel hash bucket list.
NTSTATUS UnloadDriverQuery3(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	if (!g_DriverObject)
		return STATUS_UNSUCCESSFUL;

	PVOID ciBase = GetModuleBase(g_DriverObject, L"ci.dll");
	if (!ciBase)
		return STATUS_UNSUCCESSFUL;

	if (!I_PEUpdateHashCache)
	{
		UCHAR* hit = (UCHAR*)PatternScan(ciBase, 0x1000000,
			"48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 57 41 54 41 55 41 56 41 57 48 83 EC 30 33 FF 48 8D 68 C8 21 78 C8 41 8B F1 4D 8B F8");
		if (hit)
		{
			I_PEUpdateHashCache = (PVOID)hit;
		}
	}

	if (!g_KernelHashBucketList) {
		UCHAR* hit = (UCHAR*)PatternScan(I_PEUpdateHashCache, 0x200,
			"48 8B 1B EB ?? 4C 8D 35");
		if (hit)
		{
			INT32 offset = *(INT32*)(hit + 8);
			g_KernelHashBucketList = (PVOID)((ULONG_PTR)hit + 12 + offset);
		}

	}

	if (!g_KernelHashBucketList || !MmIsAddressValid(g_KernelHashBucketList))
		return STATUS_UNSUCCESSFUL;

	ULONG count = 0;
	for (KernelHashBucketEntry* p = (KernelHashBucketEntry*)g_KernelHashBucketList;
		p != NULL;
		p = p->next) {
		count++;
	}

	if (!count)
		return STATUS_UNSUCCESSFUL;

	ULONG needed = (ULONG)MYARK_UNLOADED_DRIVER_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	RtlZeroMemory(outputBuffer, needed);

	MYARK_UNLOADED_DRIVER_RESPONSE* resp = (MYARK_UNLOADED_DRIVER_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	// Second pass: fill entries
	ULONG index = 0;
	for (KernelHashBucketEntry* p = (KernelHashBucketEntry*)g_KernelHashBucketList;
		p != NULL;
		p = p->next) {

		MYARK_UNLOADED_DRIVER_ENTRY* e = &resp->Entries[index];
		e->DriverStart = 0;
		e->DriverEnd = 0;

		ULONG chars = p->name.Length / sizeof(WCHAR);
		if (chars > MYARK_MAX_MODULE_NAME - 1)
			chars = MYARK_MAX_MODULE_NAME - 1;
		for (ULONG k = 0; k < chars; k++)
			e->DriverName[k] = p->name.Buffer[k];
		e->DriverName[chars] = 0;

		index++;
	}

	resp->Header.Count = index;
	*bytesWritten = (ULONG)MYARK_UNLOADED_DRIVER_RESPONSE_SIZE(index);
	return STATUS_SUCCESS;
}

// Get the device object name from its OBJECT_HEADER NAME_INFO (same approach as DriverQuery)
NTSTATUS DriverDeviceQuery(PVOID inputBuffer, ULONG inputLength, PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	if (!inputBuffer || inputLength < sizeof(MYARK_DRIVER_DETAIL_REQUEST))
		return STATUS_INVALID_PARAMETER;

	MYARK_DRIVER_DETAIL_REQUEST* req = (MYARK_DRIVER_DETAIL_REQUEST*)inputBuffer;
	PDRIVER_OBJECT driverObject = (PDRIVER_OBJECT)req->DriverObject;
	if (!driverObject || !MmIsAddressValid(driverObject))
		return STATUS_INVALID_PARAMETER;

	// First pass: count devices
	ULONG count = 0;
	PDEVICE_OBJECT device = driverObject->DeviceObject;
	while (device && MmIsAddressValid(device))
	{
		count++;
		device = device->NextDevice;
	}

	if (!count)
		return STATUS_UNSUCCESSFUL;

	ULONG needed = (ULONG)MYARK_DRIVER_DEVICE_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	MYARK_DRIVER_DEVICE_RESPONSE* resp = (MYARK_DRIVER_DEVICE_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	// Second pass: fill entries
	ULONG index = 0;
	device = driverObject->DeviceObject;
	while (device && MmIsAddressValid(device) && index < count)
	{
		MYARK_DRIVER_DEVICE_ENTRY* e = &resp->Entries[index];
		e->DeviceObject = (ULONG64)device;
		e->NextDevice = (ULONG64)device->NextDevice;
		e->AttachedDevice = (ULONG64)device->AttachedDevice;
		e->Flags = device->Flags;
		e->Characteristics = device->Characteristics;
		e->DeviceType = device->DeviceType;
		e->ReferenceCount = device->ReferenceCount;
		e->StackSize = device->StackSize;
		e->Reserved = 0;
		GetDeviceObjectName(device, e->DeviceName, MYARK_MAX_MODULE_PATH);

		index++;
		device = device->NextDevice;
	}

	resp->Header.Count = index;
	*bytesWritten = (ULONG)MYARK_DRIVER_DEVICE_RESPONSE_SIZE(index);
	return STATUS_SUCCESS;
}

// Enumerate DriverObject->MajorFunction dispatch table.
NTSTATUS DriverDispatchQuery(PVOID inputBuffer, ULONG inputLength, PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	if (!inputBuffer || inputLength < sizeof(MYARK_DRIVER_DETAIL_REQUEST))
		return STATUS_INVALID_PARAMETER;

	MYARK_DRIVER_DETAIL_REQUEST* req = (MYARK_DRIVER_DETAIL_REQUEST*)inputBuffer;
	PDRIVER_OBJECT driverObject = (PDRIVER_OBJECT)req->DriverObject;
	if (!driverObject || !MmIsAddressValid(driverObject))
		return STATUS_INVALID_PARAMETER;

	ULONG count = MYARK_IRP_MJ_COUNT;
	ULONG needed = (ULONG)MYARK_DRIVER_DISPATCH_RESPONSE_SIZE(count);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	MYARK_DRIVER_DISPATCH_RESPONSE* resp = (MYARK_DRIVER_DISPATCH_RESPONSE*)outputBuffer;
	resp->Header.Status = 0;
	resp->Header.Count = count;
	resp->Header.TotalSize = needed;
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	PRTL_PROCESS_MODULES modInfo = QuerySystemModules();

	for (ULONG i = 0; i < count; i++)
	{
		MYARK_DRIVER_DISPATCH_ENTRY* e = &resp->Entries[i];
		e->MajorFunction = i;
		e->Reserved = 0;
		ULONG64 addr = (ULONG64)driverObject->MajorFunction[i];
		e->DispatchAddress = addr;
		ResolveAddressToModuleName(addr, modInfo, e->ModuleName, MYARK_MAX_MODULE_NAME);
		ResolveAddressToModulePath(addr, modInfo, e->ModulePath, MYARK_MAX_MODULE_PATH);
	}

	FreeSystemModules(modInfo);
	*bytesWritten = needed;
	return STATUS_SUCCESS;
}

// Read a block of kernel memory (page-safe).
NTSTATUS ReadMemory(PVOID inputBuffer, ULONG inputLength, PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten)
{
	*bytesWritten = 0;

	if (!inputBuffer || inputLength < sizeof(MYARK_MEMORY_REQUEST))
		return STATUS_INVALID_PARAMETER;

	MYARK_MEMORY_REQUEST* req = (MYARK_MEMORY_REQUEST*)inputBuffer;
	PVOID readAddress = (PVOID)req->Address;
	ULONG64 readSize = req->ReadSize;
	if (!readAddress)
		return STATUS_INVALID_PARAMETER;

	if (readSize == 0 || readSize > 0x10000)
		return STATUS_INVALID_PARAMETER;

	ULONG needed = (ULONG)MYARK_MEMORY_RESPONSE_SIZE(readSize);
	if (outputBufferLength < needed)
		return STATUS_BUFFER_TOO_SMALL;

	MYARK_MEMORY_RESPONSE* resp = (MYARK_MEMORY_RESPONSE*)outputBuffer;

	ULONG64 copied = 0;
	__try
	{
		UCHAR* src = (UCHAR*)readAddress;
		UCHAR* dst = resp->Buffer;
		while (copied < readSize)
		{
			UCHAR* cur = src + copied;
			if (!MmIsAddressValid(cur))
				break;
			ULONG64 remain = 0x1000 - ((ULONG_PTR)cur & 0xFFF);
			ULONG64 chunk = readSize - copied;
			if (chunk > remain)
				chunk = remain;
			RtlCopyMemory(dst + copied, cur, (SIZE_T)chunk);
			copied += chunk;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}

	if (copied == 0)
		return STATUS_UNSUCCESSFUL;

	resp->Header.Status = 0;
	resp->Header.Count = (ULONG)copied;
	resp->Header.TotalSize = (ULONG)MYARK_MEMORY_RESPONSE_SIZE(copied);
	resp->Header.ProtocolVersion = MYARK_PROTOCOL_VERSION;

	*bytesWritten = (ULONG)MYARK_MEMORY_RESPONSE_SIZE(copied);
	return STATUS_SUCCESS;
}