#include "memregions/application.h"
#include "memregions/common.h"
#include <memregions/process_locator.h>
#include <memregions/region_scanner.h>
#include <memregions/reporter.h>
#include <cstdio>
#include <utility>
#include <memory>
#include <string>
#include <Windows.h>

namespace memregions
{

    Application::Application(std::shared_ptr<IProcessLocator> locator,
        std::shared_ptr<IRegionScanner> scanner,
        std::shared_ptr<IReporter> reporter) noexcept
        : locator_(std::move(locator)), scanner_(std::move(scanner)),
        reporter_(std::move(reporter)) {}

    int Application::Run(const std::wstring& query)
    {
        const auto pid{ locator_->Locate(query) };
        if (!pid)
        {
            std::fwprintf(stderr, L"Process not found: %ls\n", query.c_str());
            return 2;
        }
        UniqueProcessHandle process{
            ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, *pid) };
        if (!process.IsValid())
        {
            const DWORD err{ ::GetLastError() };
            std::fwprintf(stderr, L"OpenProcess(PID %lu) failed: %lu\n",
                static_cast<unsigned long>(*pid),
                static_cast<unsigned long>(err));
            if (err == ERROR_ACCESS_DENIED)
                std::fwprintf(stderr, L"Hint: try running as Administrator.\n");
            return 3;
        }
        reporter_->OnBegin(*pid);
        const std::size_t total{ scanner_->Scan(
            process.Get(),
            [this](const MemoryRegion& r) { reporter_->OnRegion(r); }) };
        reporter_->OnEnd(total);
        return 0;
    }

}
