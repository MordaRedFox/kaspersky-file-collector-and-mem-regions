#pragma once

#include "memregions/common.h"
#include <windows.h>
#include <functional>

namespace memregions
{

    /**
     * @brief Абстракция "обойди адресное пространство и отдай читаемые регионы".
     * Отделяет обход памяти от оркестрации. WindowsRegionScanner - реальный
     * обход через VirtualQueryEx.
     */
    class IRegionScanner
    {
    public:

        IRegionScanner() = default;
        IRegionScanner(const IRegionScanner&) = delete;
        IRegionScanner& operator=(const IRegionScanner&) = delete;
        IRegionScanner(IRegionScanner&&) = delete;
        IRegionScanner& operator=(IRegionScanner&&) = delete;
        virtual ~IRegionScanner() = default;

        /**
         * @brief Обходит адресное пространство и уведомляет о читаемых регионах.
         * @param process Валидный HANDLE процесса, открытый с правами
         * PROCESS_QUERY_INFORMATION (нужен VirtualQueryEx).
         * @param visitor Колбэк, вызываемый для каждого читаемого региона
         * в порядке возрастания адресов. Получает MemoryRegion
         * с заполненными полями base, end, size, protect.
         * @return Число регионов, переданных в visitor.
         */
        virtual std::size_t Scan(HANDLE process,
            const std::function<void(const MemoryRegion&)>& visitor) const = 0;
    };

    /**
     * @brief Реальная реализация IRegionScanner через VirtualQueryEx.
     *
     * Поведение:
     *   - обход идет от lpMinimumApplicationAddress до
     *     lpMaximumApplicationAddress (границы из GetSystemInfo);
     *   - на каждом шаге вызывается VirtualQueryEx;
     *   - регион передается в visitor только если
     *     IsReadableRegion(state, protect) (см. protection.h);
     *   - переход к следующему региону: address = BaseAddress + RegionSize;
     *   - при RegionSize == 0 - защита от зацикливания (шаг на одну
     *     страницу через si.dwPageSize);
     *   - при written == 0 (конец адресного пространства или ошибка
     *     доступа к информации о страницах) - обход прерывается,
     *     возвращается число уже найденных регионов.
     */
    class WindowsRegionScanner final : public IRegionScanner
    {
    public:

        WindowsRegionScanner() = default;
        WindowsRegionScanner(const WindowsRegionScanner&) = delete;
        WindowsRegionScanner& operator=(const WindowsRegionScanner&) = delete;
        WindowsRegionScanner(WindowsRegionScanner&&) = delete;
        WindowsRegionScanner& operator=(WindowsRegionScanner&&) = delete;
        ~WindowsRegionScanner() override = default;

        /**
         * @brief Обходит адресное пространство процесса через VirtualQueryEx.
         * @param process Валидный HANDLE процесса, открытый с правами
         * PROCESS_QUERY_INFORMATION. Если HANDLE невалиден - VirtualQueryEx
         * вернет 0 на первом же шаге, и метод вернет 0.
         * @param visitor Колбэк, вызываемый для каждого читаемого региона
         * в порядке возрастания адресов.
         * @return Число читаемых регионов, переданных в visitor.
         */
        std::size_t Scan(HANDLE process,
            const std::function<void(const MemoryRegion&)>& visitor) const override;
    };

}
