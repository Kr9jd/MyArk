#pragma once
#include <wdm.h>

#define PATTERN_MAX_LENGTH 256

// 在 [base, base+size) 范围内搜索特征码
// pattern 格式: "48 8B 05 ?? ?? ?? ?? 48 85 C0"
// 返回第一个匹配地址，未找到返回 nullptr
PVOID PatternScan(PVOID base, ULONG size, const CHAR* pattern);
