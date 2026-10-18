#include "pattern.h"

static BOOLEAN ParsePattern(const CHAR* pattern, UCHAR* bytes, BOOLEAN* wildcards, ULONG* length)
{
    ULONG count = 0;
    const CHAR* p = pattern;

    while (*p)
    {
        while (*p == ' ') p++;
        if (!*p) break;

        if (p[0] == '?' && p[1] == '?')
        {
            wildcards[count] = TRUE;
            bytes[count] = 0;
            count++;
            p += 2;
        }
        else if (p[1] && p[0] != ' ' && p[1] != ' ')
        {
            auto hexVal = [](CHAR c) -> UCHAR {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return 0;
            };
            wildcards[count] = FALSE;
            bytes[count] = (hexVal(p[0]) << 4) | hexVal(p[1]);
            count++;
            p += 2;
        }
        else
        {
            p++;
        }

        if (count >= PATTERN_MAX_LENGTH)
            break;
    }

    *length = count;
    return count > 0;
}

// Scan a memory range for a byte pattern with wildcards.
PVOID PatternScan(PVOID base, ULONG size, const CHAR* pattern)
{
    if (!base || !size || !pattern)
        return nullptr;

    UCHAR  bytes[PATTERN_MAX_LENGTH]    = {};
    BOOLEAN wilds[PATTERN_MAX_LENGTH]   = {};
    ULONG  patLen = 0;

    if (!ParsePattern(pattern, bytes, wilds, &patLen) || patLen == 0)
        return nullptr;

    ULONG scanSize = (size >= patLen) ? (size - patLen) : 0;
    PUCHAR mem = (PUCHAR)base;

    for (ULONG i = 0; i <= scanSize; i++)
    {
        BOOLEAN found = TRUE;
        for (ULONG j = 0; j < patLen; j++)
        {
            if (!wilds[j] && mem[i + j] != bytes[j])
            {
                found = FALSE;
                break;
            }
        }
        if (found)
            return (PVOID)(mem + i);
    }

    return nullptr;
}
