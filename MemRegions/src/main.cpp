#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "memregions/application.h"
#include "memregions/process_locator.h"
#include "memregions/region_scanner.h"
#include "memregions/reporter.h"
#include <cstdio>
#include <cwctype>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <cctype>

namespace
{

    void Trim(std::wstring& s)
    {
        while (!s.empty() && std::iswspace(s.back()))
            s.pop_back();
        while (!s.empty() && std::iswspace(s.front()))
            s.erase(s.begin());
    }

    std::optional<std::wstring> AskInteractive()
    {
        std::wcout << L"Enter PID or process name (e.g., 1234 or notepad): ";
        std::wcout.flush();
        std::wstring input{};
        if (!std::getline(std::wcin, input))
            return std::nullopt;
        Trim(input);
        if (input.empty())
            return std::nullopt;
        return input;
    }

    void WaitForEnter()
    {
        std::fflush(stdout);
        std::wcout << L"\nPress Enter to exit...";
        std::wcout.flush();
        std::wstring dummy{};
        std::getline(std::wcin, dummy);
    }

    void PrintUsage()
    {
        std::fwprintf(stderr, L"Usage: MemRegions <PID | process-name>\n");
        std::fwprintf(stderr, L"Examples:\n");
        std::fwprintf(stderr, L"  MemRegions 1234\n");
        std::fwprintf(stderr, L"  MemRegions notepad\n");
        std::fwprintf(stderr, L"  MemRegions notepad.exe\n");
    }

}

int wmain(int argc, wchar_t** argv)
{
    std::wstring query{};
    bool interactive{};
    if (argc == 2)
    {
        query = argv[1];
    }
    else if (argc == 1)
    {
        interactive = true;
        auto q{ AskInteractive() };
        if (!q)
        {
            std::fwprintf(stderr, L"No input provided.\n");
            WaitForEnter();
            return 1;
        }
        query = *q;
    }
    else
    {
        PrintUsage();
        return 1;
    }
    auto locator{ std::make_shared<memregions::WindowsProcessLocator>() };
    auto scanner{ std::make_shared<memregions::WindowsRegionScanner>() };
    auto reporter{ std::make_shared<memregions::ConsoleReporter>() };
    memregions::Application app(locator, scanner, reporter);
    const int rc{ app.Run(query) };
    if (interactive)
        WaitForEnter();
    return rc;
}
