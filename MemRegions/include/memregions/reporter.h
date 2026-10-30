#pragma once

#include "memregions/common.h"
#include <windows.h>
#include <cstdio>

namespace memregions
{

    /**
     * @brief Абстракция вывода результата обхода памяти.
     * Отделяет форматирование от сбора данных. ConsoleReporter - печать в
     * консоль (текущая реализация); потенциально - JsonReporter, FileReporter,
     * NullReporter.
     */
    class IReporter
    {
    public:

        IReporter() = default;
        IReporter(const IReporter&) = delete;
        IReporter& operator=(const IReporter&) = delete;
        IReporter(IReporter&&) = delete;
        IReporter& operator=(IReporter&&) = delete;
        virtual ~IReporter() = default;

        /**
         * @brief Начало вывода: заголовок с PID и колонками.
         * @param pid Идентификатор процесса, для которого печатаются регионы.
         */
        virtual void OnBegin(DWORD pid) = 0;

        /**
         * @brief Уведомление об одном читаемом регионе.
         * @param region Описание региона (base, end, size, protect).
         */
        virtual void OnRegion(const MemoryRegion& region) = 0;

        /**
         * @brief Конец вывода: итоговая строка со счетчиком.
         * @param total Общее число регионов, переданных в OnRegion.
         */
        virtual void OnEnd(std::size_t total) = 0;
    };

    /**
     * @brief Реализация IReporter, печатающая таблицу в заданный FILE*.
     * Форматирование делегировано ProtectionDescription (см. protection.h),
     * чтобы не дублировать логику разбора модификаторов PAGE_GUARD /
     * PAGE_NOCACHE / PAGE_WRITECOMBINE.
     */
    class ConsoleReporter final : public IReporter
    {
    public:

        explicit ConsoleReporter(std::FILE* out = stdout) noexcept;
        ConsoleReporter(const ConsoleReporter&) = delete;
        ConsoleReporter& operator=(const ConsoleReporter&) = delete;
        ConsoleReporter(ConsoleReporter&&) = delete;
        ConsoleReporter& operator=(ConsoleReporter&&) = delete;
        ~ConsoleReporter() override = default;

        /**
         * @brief Печатает заголовок таблицы и имя процесса.
         * @param pid Идентификатор процесса.
         */
        void OnBegin(DWORD pid) override;

        /**
         * @brief Печатает одну строку с описанием региона.
         * @param region Регион с заполненными полями base, end, size, protect.
         */
        void OnRegion(const MemoryRegion& region) override;

        /**
         * @brief Печатает итоговую строку с числом регионов.
         * @param total Общее число регионов.
         */
        void OnEnd(std::size_t total) override;

    private:
        std::FILE* out_{};
    };

}
