#pragma once

#include <windows.h>
#include <string>

namespace memregions
{

    /**
     * @brief Маска флагов защиты, при которых ReadProcessMemory успешен
     */
    inline constexpr DWORD kReadableMask =
        PAGE_READONLY
        | PAGE_READWRITE
        | PAGE_WRITECOPY
        | PAGE_EXECUTE_READ
        | PAGE_EXECUTE_READWRITE
        | PAGE_EXECUTE_WRITECOPY;

    /**
     * @brief Проверяет, можно ли прочитать регион по его состоянию и защите.
     * Регион считается читаемым, если выполнены все условия:
     *   1. state == MEM_COMMIT - страница закоммичена;
     *   2. protect & kReadableMask - базовый флаг из разрешающей маски;
     *   3. !(protect & PAGE_GUARD) - не guard-страница.
     * @param state Поле State из MEMORY_BASIC_INFORMATION.
     * @param protect Поле Protect из MEMORY_BASIC_INFORMATION.
     * @return true, если ReadProcessMemory для региона должен завершиться успешно
     */
    bool IsReadableRegion(DWORD state, DWORD protect);

    /**
     * @brief Возвращает имя базового флага защиты (без модификаторов).
     * Сравнение идет по protect & 0xFF - модификаторы (PAGE_GUARD,
     * PAGE_NOCACHE, PAGE_WRITECOMBINE) отбрасываются.
     * @param protect Поле Protect из MEMORY_BASIC_INFORMATION.
     * @return Статическая строка с именем константы (например, "PAGE_READWRITE")
     * или "UNKNOWN" для неизвестного значения.
     */
    const char* ProtectionName(DWORD protect);

    /**
     * @brief Возвращает полное человекочитаемое описание защиты.
     * Формат: <Имя> (0x<NNN>) с добавлением модификаторов через " | ".
     * @param protect Поле Protect из MEMORY_BASIC_INFORMATION.
     * @return Строка с описанием; безопасна для печати и логирования.
     */
    std::string ProtectionDescription(DWORD protect);

}
