# MemRegions - читаемые регионы памяти процесса (Windows, C++)

Консольная утилита, которая по **PID** или **имени процесса** выводит в консоль только те регионы виртуальной памяти целевого процесса, которые можно прочитать через `ReadProcessMemory` без ошибки доступа. Для каждого региона печатаются адрес начала, адрес конца, размер и параметры защиты.

Запись в чужой процесс не выполняется - утилита решает только задачу инспекции памяти.

---

## Содержание

1. [Требования ТЗ](#требования-тз)
2. [Быстрый старт](#быстрый-старт)
3. [Сборка](#сборка)
4. [Запуск](#запуск)
5. [Исследование параметров защиты](#исследование-параметров-защиты)
6. [Архитектура](#архитектура)
7. [Как проверялось](#как-проверялось)
8. [Структура проекта](#структура-проекта)
9. [Ограничения](#ограничения)

---

## Требования ТЗ

По условию задачи:

- Программа принимает на вход **PID** или **имя процесса**.
- Целевой процесс уже запущен и не связан с утилитой.
- Регионы памяти целевого процесса могут иметь **любые** параметры защиты.
- Возможность чтения определяется **сравнением с константными флагами защиты**, а не попытками чтения.
- Выводятся только те регионы, которые `ReadProcessMemory` может прочитать без ошибки доступа.
- Для каждого региона выводятся **адреса начала и конца**, параметры защиты.
- Рекомендуется **самостоятельное исследование** параметров защиты, при которых чтение разрешено.

Все пункты выполнены. Обоснование выбранных флагов - в разделе [«Исследование параметров защиты»](#исследование-параметров-защиты).

---

## Быстрый старт

В терминале откройте любое окно `cmd.exe` (в качестве примера процесса, который мы можем инспектировать) и оставьте его открытым. Затем из другого терминала:

```bash
MemRegions.exe cmd
```

Пример вывода:

```bash
Readable memory regions of PID 14224:
Base               End                Size         Protection
----------------------------------------------------------------------------------
0x000000007FFE0000 0x000000007FFE1000 4096         PAGE_READONLY (0x002)
0x000000007FFE2000 0x000000007FFE3000 4096         PAGE_READONLY (0x002)
0x000000C5EEE0F000 0x000000C5EEE12000 12288        PAGE_READWRITE (0x004)
0x00007FF748201000 0x00007FF74823A000 233472       PAGE_EXECUTE_READ (0x020)
0x00007FF748245000 0x00007FF748246000 4096         PAGE_WRITECOPY (0x008)
...
----------------------------------------------------------------------------------
Total readable regions: 130
```

---

## Сборка

### Требования

- **Windows** (WinAPI: `VirtualQueryEx`, `ReadProcessMemory`, `CreateToolhelp32Snapshot`).
- **CMake ≥ 3.21** (для `CMakePresets.json`; без пресетов - ≥ 3.16).
- **Компилятор C++20**: MSVC 19.29+ (Visual Studio 2019 16.10+).

### Visual Studio 2022

1. Workload **«Desktop development with C++»** + компонент **«C++ CMake tools for Windows»**.
2. **File → Open → Folder…** → выбрать корень проекта (где `CMakeLists.txt`).
3. В выпадающем списке сверху выбрать **VS 2022 x64 (Release)**.
4. `Ctrl+Shift+B` - сборка.
5. Готовый `.exe` - в `out/build/vs2022-x64-release/Release/MemRegions.exe`.

### Командная строка

Debug:

```bash
cmake --preset vs2022-x64-debug
cmake --build --preset vs2022-x64-debug
```

Release:

```bash
cmake --preset vs2022-x64-release
cmake --build --preset vs2022-x64-release
```

### Без presets (классический CMake)

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Стоит отметить, что приложен готовый `.exe` - `bin/MemRegions.exe`.

---

## Запуск

### Форматы

```bash
MemRegions.exe <PID>
MemRegions.exe <process-name>
MemRegions.exe <process-name.exe>
```

- Имя сравнивается **без учета регистра** (`CMD`, `Cmd`, `cmd` - одно и то же).
- Расширение `.exe` опционально (`cmd` == `cmd.exe`).
- Если аргумент не передан - включается **интерактивный режим** с приглашением ввести PID/имя и паузой в конце, чтобы консоль не закрылась.

### Коды возврата

| Код | Значение |
|---|---|
| `0` | Успех |
| `1` | Неверные аргументы (Usage) или пустой интерактивный ввод |
| `2` | Процесс не найден (по имени или по PID) |
| `3` | `OpenProcess` не удался (включая access denied, error 5) |

### Если процесс защищен

Для системных процессов (`System`, `csrss.exe`, `winlogon.exe` и т.п.) `OpenProcess` может вернуть `ERROR_ACCESS_DENIED`. В этом случае утилита печатает подсказку:

```bash
OpenProcess(PID 4) failed: 5
Hint: try running as Administrator.
```

Запустите терминал **от имени администратора**, если хотите инспектировать защищенные процессы.

---

## Исследование параметров защиты

ТЗ прямо требует определить параметры защиты, при которых чтение разрешено, путем сравнения с **константными значениями в коде**, а не "пробным чтением". Ниже - результат исследования (документация MSDN + экспериментальная проверка через `ReadProcessMemory` на тестовом процессе).

### Базовые флаги `Protect` (`MEMORY_BASIC_INFORMATION.Protect`)

| Флаг | Значение | ReadProcessMemory |
|---|---|---|
| `PAGE_NOACCESS` | 0x01 | **Ошибка** `ERROR_NOACCESS` |
| `PAGE_READONLY` | 0x02 | Читается |
| `PAGE_READWRITE` | 0x04 | Читается |
| `PAGE_WRITECOPY` | 0x08 | Читается |
| `PAGE_EXECUTE` | 0x10 | **Ошибка** - только исполнение, без права чтения |
| `PAGE_EXECUTE_READ` | 0x20 | Читается |
| `PAGE_EXECUTE_READWRITE` | 0x40 | Читается |
| `PAGE_EXECUTE_WRITECOPY` | 0x80 | Читается |

### Модификаторы

| Модификатор | Значение | Влияние на чтение |
|---|---|---|
| `PAGE_GUARD` | 0x100 | **Ошибка** `ERROR_GUARD_PAGE_VIOLATION` - первое чтение триггерит guard-исключение |
| `PAGE_NOCACHE` | 0x200 | Не влияет на чтение |
| `PAGE_WRITECOMBINE` | 0x400 | Не влияет на чтение |

### Маска читаемых флагов (в коде)

```cpp
inline constexpr DWORD kReadableMask =
      PAGE_READONLY
    | PAGE_READWRITE
    | PAGE_WRITECOPY
    | PAGE_EXECUTE_READ
    | PAGE_EXECUTE_READWRITE
    | PAGE_EXECUTE_WRITECOPY;
```

### Правило "регион читаем"

```cpp
bool IsReadableRegion(DWORD state, DWORD protect)
{
    // Только закоммиченные
    if (state != MEM_COMMIT)
        return false;
    // Базовый флаг из маски
    if ((protect & kReadableMask) == 0)
        return false;
    // Guard-страницы пропускаем
    if (protect & PAGE_GUARD)
        return false;
    return true;
}
```

**Почему `PAGE_GUARD` исключен явно.** По документации `PAGE_GUARD` формально "доступ разрешает", но первое обращение к такой странице поднимает guard-исключение, и `ReadProcessMemory` возвращает `ERROR_GUARD_PAGE_VIOLATION`. ТЗ требует, чтобы в выводе были только регионы, читаемые через `ReadProcessMemory`, - значит, `PAGE_GUARD` **обязан** быть отфильтрован. Это ключевая деталь, которую легко упустить, если полагаться только на открытые источники.

### Что проверено экспериментально

- На тестовом процессе все флаги из таблицы выше подтверждены через `ReadProcessMemory` - поведение совпало с ожидаемым.
- На реальных процессах `PAGE_NOACCESS` и `PAGE_GUARD` в выводе **отсутствуют**, а `PAGE_WRITECOPY` и `PAGE_EXECUTE_READ` - присутствуют, как и должны.
- Независимая проверка `ReadProcessMemory` по каждому выведенному адресу (см. раздел [«Как проверялось»](#как-проверялось)) дает **ноль ошибок чтения**.

---

## Архитектура

Класс разбит на слои по принципам SOLID:

| Компонент | Ответственность |
|---|---|
| `IProcessLocator` / `WindowsProcessLocator` | Поиск PID по строке (PID или имя) |
| `IRegionScanner` / `WindowsRegionScanner` | Обход адресного пространства через `VirtualQueryEx` |
| `IReporter` / `ConsoleReporter` | Форматирование и вывод |
| `Application` | Оркестрация сценария |
| `UniqueProcessHandle` | RAII-обертка над Win32 `HANDLE` |
| `protection.*` | Классификация и описание флагов защиты |

### Зачем такая декомпозиция

- **`Application` не знает** ни о `tlhelp32.h`, ни о `VirtualQueryEx`, ни о `printf`. Зависимости приходят через `std::shared_ptr<I…>` в конструкторе - **Dependency Inversion**.
- Легко добавить новый вывод (`JsonReporter`, `FileReporter`) - правка только в `reporter.h/.cpp` и одна строка в `main.cpp` - **Open/Closed**.
- Легко добавить новый способ поиска (по имени окна, по командной строке) - правка только в `process_locator.h/.cpp`.
- Все три интерфейса маленькие (1–3 метода) - **Interface Segregation**.

### RAII и Rule of 5

`UniqueProcessHandle` - move-only RAII-обертка над Win32 `HANDLE`. Деструктор вызывает `CloseHandle`, если хендл валидный (`!= nullptr` и `!= INVALID_HANDLE_VALUE`). Копирование запрещено - два владельца одного `HANDLE` привели бы к double-close.

Для всех полиморфных классов явно объявлены все пять специальных членов (**Rule of 5**): виртуальный деструктор `= default` в интерфейсе + `= delete` на copy/move (защита от slicing), в `final`-реализациях - `= delete` на copy/move, `~… override = default`.

---

## Как проверялось

### 1. Дымовая проверка

```bash
MemRegions.exe cmd
MemRegions.exe <PID>
```

Проверено: вывод по имени и по PID **побайтово идентичен**.

### 2. Инварианты фильтра

```bash
MemRegions.exe cmd | findstr "PAGE_NOACCESS"       # пусто
MemRegions.exe cmd | findstr "PAGE_GUARD"          # пусто
MemRegions.exe cmd | findstr "PAGE_EXECUTE"        # пусто (без READ)
MemRegions.exe cmd | findstr "UNKNOWN"             # пусто
```

Проверено на `cmd.exe`: **все четыре команды пусты**, что подтверждает корректность маски `kReadableMask`.

### 3. Инвариант `Size == End - Base` и отсутствие пересечений

Проверено скриптом PowerShell:

- Для каждой строки `End - Base == Size` - **OK=True**;
- `Base` каждой строки `>= End` предыдущей - **Overlaps: 0**.

### 4. Граничные случаи (коды возврата)

| Сценарий | Команда | Ожидаемо |
|---|---|---|
| Несуществующее имя | `MemRegions.exe nonexistent` | `Process not found`, код 2 |
| Несуществующий PID | `MemRegions.exe 9999999` | `failed: 87`, код 3 |
| Protected без админа | `MemRegions.exe 4` | `failed: 5` + hint, код 3 |
| Лишние аргументы | `MemRegions.exe cmd extra` | Usage, код 1 |
| Переполнение DWORD | `MemRegions.exe 99999999999999999999` | Не падает |
| Регистр и `.exe` | `CMD` / `Cmd` / `cmd.exe` | Идентичный вывод |

Все сценарии пройдены, поведение совпадает с ожидаемым.

### 5. Независимая проверка через `ReadProcessMemory`

Ключевой тест. Для каждого адреса из вывода утилиты отдельный скрипт делает реальный вызов `ReadProcessMemory(addr, 1 byte)` и проверяет успех. Результат на `cmd.exe` (130 регионов):

```bash
Verified readable regions: 130
Failed reads:              0
RESULT: OK - every reported region is actually readable
```

Это доказывает главное требование ТЗ: **каждый выведенный регион действительно читается через `ReadProcessMemory`**.

---

## Структура проекта

```bash
.
├── bin/
│   └── MemRegions.exe              // собранный Release-бинарник
│
├── include/
│   └── memregions/
│       ├── application.h           // Application
│       ├── common.h                // MemoryRegion, UniqueProcessHandle
│       ├── process_locator.h       // IProcessLocator + WindowsProcessLocator
│       ├── protection.h            // kReadableMask, IsReadableRegion, описание
│       ├── region_scanner.h        // IRegionScanner + WindowsRegionScanner
│       └── reporter.h              // IReporter + ConsoleReporter
│
├── src/
│   ├── application.cpp             // оркестрация
│   ├── main.cpp                    // argv, интерактив, WaitForEnter
│   ├── process_locator.cpp
│   ├── protection.cpp
│   ├── region_scanner.cpp
│   └── reporter.cpp
│
├── CMakeLists.txt
├── CMakePresets.json
├── LICENSE
└── README.md
```

---

## Ограничения

Сознательно **не реализовано** (вне рамок ТЗ):
- **Запись в чужой процесс** - утилита только читает, `PROCESS_VM_WRITE` не запрашивается.
- **Обход защищенных процессов без прав администратора** - `OpenProcess` вернет `ERROR_ACCESS_DENIED`, утилита честно сообщит об этом и предложит запуск от админа.
- **Проверка чтения через `ReadProcessMemory` в самом коде** - по ТЗ возможность чтения определяется **сравнением с константными флагами**, а не пробным чтением.

Возможные улучшения (вне ТЗ):
- Вывод в JSON (`--json`).
- Фильтрация по диапазону адресов (`--from`, `--to`).
- Сохранение в файл (`--output file.txt`).
- Поиск по имени окна / по командной строке.

---

## Лицензия / авторство

Тестовое задание. Автор - Куропатин Владислав Сергеевич, студент 2-го курса РТУ МИРЭА.
