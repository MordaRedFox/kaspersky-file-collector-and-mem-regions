#include "memregions/process_locator.h"
#include <tlhelp32.h>
#include <algorithm>
#include <cwctype>
#include <Windows.h>
#include <optional>
#include <cctype>
#include <string>

namespace memregions
{
    namespace
    {

        std::optional<DWORD> ParsePid(const std::wstring& s)
        {
            if (s.empty())
                return std::nullopt;
            if (!std::all_of(s.begin(), s.end(), [](wchar_t c) { return std::iswdigit(c) != 0; }))
                return std::nullopt;
            try
            {
                const unsigned long v{ std::stoul(s) };
                if (v > MAXDWORD)
                    return std::nullopt;
                return static_cast<DWORD>(v);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        std::wstring ToLowerCopy(std::wstring s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            return s;
        }

        std::wstring NormalizeExeName(const std::wstring& name)
        {
            std::wstring n{ ToLowerCopy(name) };
            if (n.size() < 4 || n.compare(n.size() - 4, 4, L".exe") != 0)
                n += L".exe";
            return n;
        }

    }

    std::optional<DWORD> WindowsProcessLocator::Locate(const std::wstring& query) const
    {
        const auto pid{ ParsePid(query) };
        if (pid)
            return pid;
        HANDLE snap{ ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
        if (snap == INVALID_HANDLE_VALUE)
            return std::nullopt;
        const std::wstring target{ NormalizeExeName(query) };
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        std::optional<DWORD> result{};
        if (::Process32FirstW(snap, &pe))
        {
            do
            {
                if (ToLowerCopy(pe.szExeFile) == target)
                {
                    result = pe.th32ProcessID;
                    break;
                }
            } while (::Process32NextW(snap, &pe));
        }
        ::CloseHandle(snap);
        return result;
    }

}
