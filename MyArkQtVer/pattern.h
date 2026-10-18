#pragma once
#include <windows.h>

#define PATTERN_MAX_LENGTH 256

// 在 [base, base+size) 范围内搜索特征码
// pattern 格式: "48 8B 05 ?? ?? ?? ?? 48 85 C0"
// 返回第一个匹配地址，未找到返回 nullptr
void* PatternScan(void* base, ULONG size, const char* pattern);

// 返回所有匹配地址（new[] 分配，调用者 delete[]）
// outCount 输出匹配数量
void** PatternScanAll(void* base, ULONG size, const char* pattern, ULONG* outCount);
