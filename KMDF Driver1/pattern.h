#pragma once
#include <wdm.h>

#define PATTERN_MAX_LENGTH 256

// Scan for the signature within [base, base+size)
// Pattern format: "48 8B 05 ?? ?? ?? ?? 48 85 C0"
// Return the first match, or nullptr if not found
PVOID PatternScan(PVOID base, ULONG size, const CHAR* pattern);
