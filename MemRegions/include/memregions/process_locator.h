#pragma once

#include <optional>
#include <string>
#include <windows.h>

namespace memregions
{

    /**
     * @brief Абстракция "дай PID по строке запроса".
     * Отделяет поиск процесса от его использования. WindowsProcessLocator -
     * реальный поиск через WinAPI.
     */
    class IProcessLocator
    {
    public:

        IProcessLocator() = default;
        IProcessLocator(const IProcessLocator&) = delete;
        IProcessLocator& operator=(const IProcessLocator&) = delete;
        IProcessLocator(IProcessLocator&&) = delete;
        IProcessLocator& operator=(IProcessLocator&&) = delete;
        virtual ~IProcessLocator() = default;

        /**
         * @brief Находит PID по строке запроса.
         * @param query PID в виде десятичной строки или имя образа.
         * @return PID при успехе; std::nullopt, если процесс не найден
         * или запрос некорректен.
         */
        virtual std::optional<DWORD> Locate(const std::wstring& query) const = 0;
    };

    /**
     * @brief Реальная реализация IProcessLocator через WinAPI.
     * Поведение:
     *   - если query - десятичное число, оно трактуется как PID;
     *   - иначе - поиск по имени образа через CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS);
     *   - сравнение имени - без учета регистра и с нормализацией расширения .exe.
     */
    class WindowsProcessLocator final : public IProcessLocator
    {
    public:
        WindowsProcessLocator() = default;
        WindowsProcessLocator(const WindowsProcessLocator&) = delete;
        WindowsProcessLocator& operator=(const WindowsProcessLocator&) = delete;
        WindowsProcessLocator(WindowsProcessLocator&&) = delete;
        WindowsProcessLocator& operator=(WindowsProcessLocator&&) = delete;
        ~WindowsProcessLocator() override = default;

        /**
         * @brief Находит PID процесса через WinAPI.
         * @param query PID в виде строки цифр или имя образа процесса.
         * @return PID найденного процесса.
         */
        std::optional<DWORD> Locate(const std::wstring& query) const override;
    };

}
