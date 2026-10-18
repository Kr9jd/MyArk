#pragma once
#include "header.h"
PRTL_PROCESS_MODULES QuerySystemModules();
void FreeSystemModules(PRTL_PROCESS_MODULES modInfo);
void ResolveAddressToModuleName(ULONG64 addr, PRTL_PROCESS_MODULES modInfo, WCHAR* outName, ULONG maxChars);
void ResolveAddressToModulePath(ULONG64 addr, PRTL_PROCESS_MODULES modInfo, WCHAR* outPath, ULONG maxChars);
void CopyNameToWchar(PWCHAR out, ULONG maxChars, PCUNICODE_STRING name);
void GetDeviceObjectName(PVOID object, PWCHAR out, ULONG maxChars);
PVOID GetNtosBase(PDRIVER_OBJECT DrvObj);
PVOID GetExportByName(PVOID moduleBase, PCSTR functionName);
PVOID GetModuleBase(PDRIVER_OBJECT DrvObj, PCWSTR moduleName);