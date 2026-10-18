#pragma once
#include <windows.h>

#define PATTERN_MAX_LENGTH 256

// Scan for the signature within [base, base+size)
// Pattern format: "48 8B 05 ?? ?? ?? ?? 48 85 C0"
// Return the first match, or nullptr if not found
void* PatternScan(void* base, ULONG size, const char* pattern);

// Return all matches (allocated with new[], caller must delete[])
// outCount receives the number of matches
void** PatternScanAll(void* base, ULONG size, const char* pattern, ULONG* outCount);
