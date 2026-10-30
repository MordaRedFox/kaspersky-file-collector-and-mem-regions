#include "memregions/region_scanner.h"
#include "memregions/protection.h"
#include <memregions/common.h>
#include <functional>
#include <cstdarg>
#include <Windows.h>

namespace memregions
{

    std::size_t WindowsRegionScanner::Scan(HANDLE process,
        const std::function<void(const MemoryRegion&)>& visitor) const
    {
        SYSTEM_INFO si{};
        ::GetSystemInfo(&si);
        std::uintptr_t address{
            reinterpret_cast<std::uintptr_t>(si.lpMinimumApplicationAddress) };
        const std::uintptr_t maxAddress{
            reinterpret_cast<std::uintptr_t>(si.lpMaximumApplicationAddress) };
        std::size_t count{};
        MEMORY_BASIC_INFORMATION mbi{};
        while (address < maxAddress)
        {
            const SIZE_T written{ ::VirtualQueryEx(
                process,
                reinterpret_cast<LPCVOID>(address),
                &mbi,
                sizeof(mbi)) };
            if (written == 0)
                break;
            const std::uintptr_t base{
                reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) };
            const std::uintptr_t end{ base + mbi.RegionSize };
            if (IsReadableRegion(mbi.State, mbi.Protect))
            {
                MemoryRegion region{};
                region.base = base;
                region.end = end;
                region.size = mbi.RegionSize;
                region.protect = mbi.Protect;
                visitor(region);
                ++count;
            }
            address = (mbi.RegionSize == 0) ? address + si.dwPageSize : end;
        }
        return count;
    }

}
