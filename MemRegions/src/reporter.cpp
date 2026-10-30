#include "memregions/reporter.h"
#include "memregions/protection.h"
#include <memregions/common.h>
#include <string>
#include <cstdio>
#include <Windows.h>

namespace memregions
{

    ConsoleReporter::ConsoleReporter(std::FILE* out) noexcept : out_(out) {}

    void ConsoleReporter::OnBegin(DWORD pid)
    {
        std::fprintf(out_, "Readable memory regions of PID %lu:\n",
            static_cast<unsigned long>(pid));
        std::fprintf(out_, "%-18s %-18s %-12s %-28s\n",
            "Base", "End", "Size", "Protection");
        std::fprintf(out_, "%s\n", std::string(82, '-').c_str());
    }

    void ConsoleReporter::OnRegion(const MemoryRegion& r)
    {
        std::fprintf(out_, "0x%016llX 0x%016llX %-12llu %s\n",
            static_cast<unsigned long long>(r.base),
            static_cast<unsigned long long>(r.end),
            static_cast<unsigned long long>(r.size),
            ProtectionDescription(r.protect).c_str());
    }

    void ConsoleReporter::OnEnd(std::size_t total)
    {
        std::fprintf(out_, "%s\n", std::string(82, '-').c_str());
        std::fprintf(out_, "Total readable regions: %zu\n", total);
    }

}
