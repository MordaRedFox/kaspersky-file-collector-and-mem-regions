#pragma once

#include "memregions/process_locator.h"
#include "memregions/region_scanner.h"
#include "memregions/reporter.h"
#include <memory>
#include <string>

namespace memregions
{

    /**
     * @brief Оркестратор сценария "найти процесс -> открыть ->
     * обойти память -> напечатать".
     * Класс знает только о трех абстракциях (IProcessLocator, IRegionScanner,
     * IReporter) и не зависит ни от WinAPI (tlhelp32.h, VirtualQueryEx),
     * ни от способа вывода (printf/JSON/лог).
     */
    class Application
    {
    public:

        Application(std::shared_ptr<IProcessLocator> locator,
            std::shared_ptr<IRegionScanner> scanner,
            std::shared_ptr<IReporter> reporter) noexcept;
        Application(const Application&) = default;
        Application& operator=(const Application&) = default;
        Application(Application&&) noexcept = default;
        Application& operator=(Application&&) noexcept = default;
        ~Application() = default;

        /**
         * @brief Выполняет полный сценарий по строковому запросу.
         *
         * Шаги:
         *   1. Найти PID через locator_->Locate(query).
         *   2. Открыть процесс через OpenProcess(PROCESS_QUERY_INFORMATION
         *      | PROCESS_VM_READ).
         *   3. reporter_->OnBegin(pid).
         *   4. scanner_->Scan(...) с колбэком reporter_->OnRegion(...).
         *   5. reporter_->OnEnd(total).
         *   6. Закрыть HANDLE (через RAII).
         * @param query PID (в виде строки цифр) или имя образа процесса.
         * @return Код возврата:
         *          0 - успех;
         *          2 - процесс не найден;
         *          3 - OpenProcess не удался (включая access denied).
         */
        int Run(const std::wstring& query);

    private:
        std::shared_ptr<IProcessLocator> locator_{};
        std::shared_ptr<IRegionScanner> scanner_{};
        std::shared_ptr<IReporter> reporter_{};
    };

}
