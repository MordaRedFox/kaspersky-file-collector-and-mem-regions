#include "memregions/protection.h"
#include <cstdio>
#include <Windows.h>
#include <string>

namespace memregions
{

    bool IsReadableRegion(DWORD state, DWORD protect)
    {
        if (state != MEM_COMMIT)
            return false;
        if ((protect & kReadableMask) == 0)
            return false;
        if (protect & PAGE_GUARD)
            return false;
        return true;
    }

    const char* ProtectionName(DWORD protect)
    {
        switch (protect & 0xFF)
        {
        case PAGE_NOACCESS:
            return "PAGE_NOACCESS";
        case PAGE_READONLY:
            return "PAGE_READONLY";
        case PAGE_READWRITE:
            return "PAGE_READWRITE";
        case PAGE_WRITECOPY:
            return "PAGE_WRITECOPY";
        case PAGE_EXECUTE:
            return "PAGE_EXECUTE";
        case PAGE_EXECUTE_READ:
            return "PAGE_EXECUTE_READ";
        case PAGE_EXECUTE_READWRITE:
            return "PAGE_EXECUTE_READWRITE";
        case PAGE_EXECUTE_WRITECOPY:
            return "PAGE_EXECUTE_WRITECOPY";
        default:
            return "UNKNOWN";
        }
    }

    std::string ProtectionDescription(DWORD protect)
    {
        char buf[128]{};
        const int n{ std::snprintf(buf, sizeof(buf), "%s (0x%03lX)",
            ProtectionName(protect),
            static_cast<unsigned long>(protect & 0xFF)) };
        std::string result(buf, n > 0 ? static_cast<std::size_t>(n) : 0);
        if (protect & PAGE_GUARD)
            result += " | PAGE_GUARD";
        if (protect & PAGE_NOCACHE)
            result += " | PAGE_NOCACHE";
        if (protect & PAGE_WRITECOMBINE)
            result += " | PAGE_WRITECOMBINE";
        return result;
    }

}
