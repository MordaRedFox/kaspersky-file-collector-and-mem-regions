#pragma once

#include <windows.h>
#include <cstdint>
#include <cstdarg>

namespace memregions
{

    /**
     * @brief Описание одного читаемого региона виртуальной памяти.
     * Заполняется в WindowsRegionScanner на основе MEMORY_BASIC_INFORMATION
     * и передается в IReporter. Не привязан к WinAPI - можно тестировать
     * и сериализовать без <windows.h> в unit-тестах.
     */
    struct MemoryRegion
    {
        std::uintptr_t base{};
        std::uintptr_t end{};
        size_t size{};
        DWORD protect{};
    };

    /**
     * @brief RAII-обертка над Win32 HANDLE для процессов.
     * Владеет хендлом и вызывает CloseHandle в деструкторе - так
     * исключаются утечки при ранних return, исключениях и т.п.
     */
    class UniqueProcessHandle
    {
    public:

        UniqueProcessHandle() noexcept = default;
        explicit UniqueProcessHandle(HANDLE h) noexcept : h_(h) {}
        UniqueProcessHandle(const UniqueProcessHandle&) = delete;
        UniqueProcessHandle& operator=(const UniqueProcessHandle&) = delete;

        /**
         * @brief Перемещающий конструктор: забирает владение у other.
         * @param other Обертка, которая после перемещения станет "пустой".
         */
        UniqueProcessHandle(UniqueProcessHandle&& other) noexcept
            : h_(other.Release()) {}

        /**
         * @brief Перемещающее присваивание с освобождением текущего хендла.
         * @param other Обертка, чье владение забираем.
         * @return *this.
         */
        UniqueProcessHandle& operator=(UniqueProcessHandle&& other) noexcept
        {
            if (this != &other)
                Reset(other.Release());
            return *this;
        }

        ~UniqueProcessHandle() noexcept { Reset(); }

        /**
         * @brief Возвращает хендл без передачи владения.
         * @return Внутренний HANDLE (может быть невалидным).
         */
        HANDLE Get() const noexcept { return h_; }

        /**
         * @brief Проверяет, что хендл валиден.
         * @return true, если h_ != nullptr && h_ != INVALID_HANDLE_VALUE.
         */
        bool IsValid() const noexcept
        {
            return h_ != nullptr && h_ != INVALID_HANDLE_VALUE;
        }

        /**
         * @brief Закрывает текущий хендл (если валиден) и принимает новый.
         * @param h Новый хендл. По умолчанию - "пустой".
         */
        void Reset(HANDLE h = {}) noexcept
        {
            if (IsValid())
                ::CloseHandle(h_);
            h_ = h;
        }

        /**
         * @brief Отдает владение наружу и обнуляет обертку.
         * @return Раньше принадлежавший хендл (может быть невалидным).
         */
        HANDLE Release() noexcept
        {
            HANDLE tmp{ h_ };
            h_ = nullptr;
            return tmp;
        }

    private:
        HANDLE h_{};
    };

}
