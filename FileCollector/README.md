# FileCollector

Потокобезопасный класс `FileCollector`, собирающий файл из чанков, приходящих по "сети" в произвольном порядке. Чанки могут пересекаться, дублироваться и приходить с разрывами. Один файл может собираться несколькими потоками одновременно; разные файлы синхронизируются независимо и не мешают друг другу.

Сетевое взаимодействие не реализуется - задача про сам коллектор.

---

## Содержание

1. [Быстрый старт](#быстрый-старт)
2. [Структура проекта](#структура-проекта)
3. [Идея решения](#идея-решения)
4. [Публичный API](#публичный-api)
5. [Потокобезопасность](#потокобезопасность)
6. [Производительность и сложность](#производительность-и-сложность)
7. [Граничные случаи](#граничные-случаи)
8. [Тесты](#тесты)
9. [Сборка](#сборка)
10. [Ограничения и возможные улучшения](#ограничения-и-возможные-улучшения)

---

## Быстрый старт

Минимальный пример: собрать файл из трех пересекающихся чанков (пример прямо из ТЗ).

```cpp
#include "file_collector.h"
#include <cstdio>

int main() {
    safe_board::FileCollector c;
    // Файл размером 10 байт.
    c.CollectFile(/*fileId=*/1, /*fileSize=*/10);
    // Чанк №1: [A, B, C, D] начиная с 0.
    c.OnNewChunk(1, 0, {'A', 'B', 'C', 'D'});
    // Чанк №2: [C, D, E, F] начиная с 2 - пересекается с предыдущим.
    c.OnNewChunk(1, 2, {'C', 'D', 'E', 'F'});
    // Чанк №3: [G, H, I, J] начиная с 6 - не пересекается.
    c.OnNewChunk(1, 6, {'G', 'H', 'I', 'J'});
    // Снимок текущего состояния.
    if (auto snap{ c.GetFile(1) }; snap && snap->IsComplete())
    {
        if (auto bytes{ snap->Materialize()})
        {
            std::fwrite(bytes->data(), 1, bytes->size(), stdout);
            std::fputc('\n', stdout);
        }
    }
    // Ожидаемый вывод: ABCDEFGHIJ
    return 0;
}
```

Если файл еще не полон, `Materialize()` вернет `std::nullopt`. Для обработки частично полученных данных используйте `ForEachChunk()` - он отдает `std::span` на уже собранные интервалы без копирования.

---

## Структура проекта

```bash
.
├── include/
│   └── file_collector.h         // публичный API (FileCollector, FileSnapshot)
│
├── src/
│   └── file_collector.cpp       // реализация
│
├── tests/
│   └── file_collector_test.cpp  // тесты на GoogleTest
│
├── CMakeLists.txt
├── CMakePresets.json
├── LICENSE
└── README.md
```

---

## Идея решения

### Хранение чанков

Для каждого файла - `std::map<size_t, std::shared_ptr<const std::vector<uint8_t>>>`:
- **Ключ** - начало непрерывного интервала данных;
- **Значение** - неизменяемый буфер.

Поддерживается инвариант: **интервалы не пересекаются и не касаются**. Любая вставка сливает новый диапазон с 0..N соседями, которые пересекаются или примыкают (проверяется условие `prev_end >= pos` слева и `next_start <= newEnd` справа).

Что это дает:
- **Пересекающиеся чанки** сливаются, лишние байты не задваиваются;
- **Дубликаты** идемпотентны - счетчик `collectedBytes` не растет;
- **Разрывы** хранятся естественно: между интервалами просто "дырка" в позициях, мапа это переживает без выделения памяти под отсутствующие байты;
- **Восстановление** файла - один линейный проход по мапе.

Почему `std::map`, а не, скажем, разреженный массив/битовая карта:
- Размер файла заранее неизвестен (задается в `CollectFile`, но не ограничен);
- Чанков может быть мало и они могут быть большими - мапа не выделяет память под "еще не пришедшие" байты;
- Слияние соседей в мапе - O(log n + k), где k - число затронутых интервалов.

Почему `std::shared_ptr<const std::vector<uint8_t>>`, а не `std::vector`:
- После вставки данные чанка **неизменяемы** - это позволяет снапшоту шарить их **без копирования**;
- `FileSnapshot` хранит разделяемый `map<...>`, а не копию payload - снятие снимка O(n) по числу интервалов, а не по размеру файла.

### Получение файла

Метод `GetFile(fileId)` возвращает `std::optional<FileSnapshot>`, а не `std::vector<uint8_t>`. Прямое возвращение вектора не отвечает на требования ТЗ:

| Требование ТЗ | Прямой `vector` | `FileSnapshot` |
|---|---|---|
| Частичное получение | ✗ (либо все, либо ничего) | ✓ `ForEachChunk` |
| Полное получение | ✓ | ✓ `Materialize` |
| Минимум копирований | ✗ (копия всех данных) | ✓ (снапшот - O(число интервалов), payload шарится) |
| Потокобезопасность | ✗ (изменяемый вектор) | ✓ (иммутабельный снимок) |
| Знание о неполноте | ✗ | ✓ `IsComplete()`, `CollectedBytes()` |

`FileSnapshot` иммутабелен после создания - можно передавать между потоками без блокировок.

---

## Публичный API

### `class FileCollector`

```cpp
class FileCollector {
public:
    FileCollector() = default;
    FileCollector(const FileCollector&) = delete;
    FileCollector& operator=(const FileCollector&) = delete;
    FileCollector(FileCollector&&) = delete;
    FileCollector& operator=(FileCollector&&) = delete;
    ~FileCollector() = default;

    void CollectFile(uint32_t fileId, size_t fileSize);

    void OnNewChunk(uint32_t fileId, size_t pos, std::vector<uint8_t>&& chunk);

    std::optional<FileSnapshot> GetFile(uint32_t fileId) const;

    bool WaitForCompletion(uint32_t fileId, std::chrono::milliseconds timeout) const;

    void RemoveFile(uint32_t fileId);

    size_t FileCount() const;
};
```

- `CollectFile(id, size)` - создает структуру для файла. Повторный вызов сбрасывает ранее собранные данные (но **не** ломает уже выданные снапшоты - они владеют разделяемыми данными).
- `OnNewChunk(id, pos, chunk)` - добавляет чанк. Порядок любой, пересечения и дубликаты допустимы. Чанк за границей `fileSize` обрезается; чанк для незарегистрированного `id` игнорируется; пустой чанк игнорируется.
- `GetFile(id)` - снимок текущего состояния или `nullopt`.
- `WaitForCompletion(id, timeout)` - блокирующее ожидание полной сборки. `true` - файл полон, `false` - таймаут или файл не зарегистрирован.
- `RemoveFile(id)` - удаляет файл из реестра. Ранее выданные снапшоты остаются валидными.
- `FileCount()` - число зарегистрированных файлов.

### `class FileSnapshot`

```cpp
class FileSnapshot {
public:
    using ChunkMap = std::map<size_t, std::shared_ptr<const std::vector<uint8_t>>>;

    size_t FileSize() const noexcept;

    size_t CollectedBytes() const noexcept;

    bool IsComplete() const noexcept;

    void ForEachChunk(const std::function<void(size_t, std::span<const uint8_t>)>& fn) const;

    std::optional<std::vector<uint8_t>> Materialize() const;
};
```

- `FileSize()` - объявленный размер файла.
- `CollectedBytes()` - сколько уже собрано (может быть `< FileSize()`).
- `IsComplete()` - все ли байты получены.
- `ForEachChunk(fn)` - обход доступных непрерывных диапазонов без копирования payload. Колбэк получает позицию и `std::span`. **`span` валиден только на время вызова колбэка.**
- `Materialize()` - собирает файл целиком в `vector` (единственное неизбежное копирование). `nullopt`, если файл еще не полон.

---

## Потокобезопасность

Двухуровневая схема:
1. **Реестр файлов** (`std::unordered_map<id, shared_ptr<FileData>>`) защищен `std::shared_mutex`:
   - Чтение (`GetFile`, `FileCount`, `FindFile`, `WaitForCompletion`) - под `shared_lock`, параллельно;
   - Мутации (`CollectFile`, `RemoveFile`) - под `unique_lock`, эксклюзивно.

2. **Каждый файл** имеет собственный `std::mutex mtx` и `std::condition_variable cv`:
   - `OnNewChunk` сериализуются по одному файлу;
   - `GetFile` берет `mtx` только на время копирования мапы интервалов (не на время обработки payload);
   - `WaitForCompletion` ждет на `cv` до полной сборки или таймаута.

Разные файлы **не блокируют друг друга**: `OnNewChunk` для файла A не задевает `mtx` файла B, а реестр берется под `shared_lock` в `FindFile` на микроскопическое время.

Иммутабельность `FileSnapshot` означает, что после `GetFile` снимок можно читать из любого числа потоков без синхронизации.

`FileCollector` и `FileSnapshot` используют **Rule of 5** (или "Rule of 6" с дефолтным конструктором):

- У `FileCollector` копирование/перемещение запрещены (у него `std::shared_mutex`, который не копируется и не перемещается);
- У `FileSnapshot` дефолтный конструктор `= delete` - иначе можно было бы создать "пустой" снимок с `nullptr` внутри и `IsComplete()` ложно вернул бы `true`, что привело бы к UB в `Materialize()`;
- Копирование/перемещение `FileSnapshot` разрешены (`= default`) - это дешевые операции (несколько указателей).

---

## Производительность и сложность

Пусть `n` - число интервалов файла на момент операции, `k` - число соседей, затронутых вставкой (обычно 0, 1 или 2).

| Операция | Сложность | Комментарий |
|---|---|---|
| `CollectFile` | O(1) + взятие `unique_lock` | Создание пустого `FileData` |
| `OnNewChunk` | O(log n + k) + O(размер нового диапазона) | Слияние соседей; `std::move` payload |
| `GetFile` | O(n) | Копия мапы `shared_ptr`, payload не копируется |
| `Materialize` | O(fileSize) | Одно неизбежное копирование |
| `ForEachChunk` | O(n) | Без копирования |
| `WaitForCompletion` | O(1) + ожидание | На `condition_variable` |
| `FileCount` | O(1) + `shared_lock` | |

Минимизация копирований:
- `OnNewChunk` принимает `std::vector<uint8_t>&&` - данные перемещаются внутрь коллектора;
- При слиянии формируется один новый буфер `merged`, старые интервалы освобождаются;
- `GetFile` шарит payload через `shared_ptr` - снапшот не копирует байты;
- `Materialize` копирует payload ровно один раз (это требование возвращаемого типа `std::vector<uint8_t>`).

Минимизация блокировок:
- `shared_lock` на реестре для всех "читающих" операций;
- критическая секция `OnNewChunk` - только по одному файлу;
- `Materialize` и `ForEachChunk` работают **вне** блокировки, так как снимок уже отвязан от `FileData`.

---

## Граничные случаи

| Ситуация | Поведение |
|---|---|
| Пустой чанк | Игнорируется |
| Чанк для незарегистрированного `fileId` | Игнорируется (защита от гонки "чанк раньше `CollectFile`") |
| `pos >= fileSize` | Чанк полностью игнорируется |
| `pos + chunk.size() > fileSize` | Чанк обрезается до `fileSize - pos` |
| `fileSize == 0` | Файл сразу полон; `Materialize()` вернет пустой вектор |
| Полный дубликат уже собранного | `collectedBytes` не растет; интервал не "задваивается" |
| Вложенный чанк | То же |
| Касание двух соседних интервалов | Каскадное слияние в один |
| Повторный `CollectFile` с тем же `id` | Данные сбрасываются; старые снапшоты остаются валидными |
| `RemoveFile` при живых снапшотах | Снапшоты продолжают работать (владеют разделяемыми данными) |
| `WaitForCompletion` для неизвестного `id` | `false` |
| `WaitForCompletion` после `RemoveFile` | `false` по таймауту (файла больше нет в реестре) |

---

## Тесты

Все тесты в `tests/file_collector_test.cpp`, фреймворк - GoogleTest.

### Функциональные

1. `EmptyCollector` - пустой реестр, `GetFile` для неизвестного id → `nullopt`.
2. `InOrderChunks` - чанки приходят по порядку.
3. `OutOfOrderChunks` - чанки в обратном порядке.
4. `OverlappingChunksTaskExample` - **пример из ТЗ**.
5. `DuplicateChunksDoNotInflateProgress` - дубликаты и вложенные чанки.
6. `ChunkPastEndIsTruncated` - обрезка по `fileSize`.
7. `ChunkStartingPastEndIsIgnored` - `pos >= fileSize` игнорируется.
8. `ZeroSizedFileIsImmediatelyComplete` - файл размера 0.
9. `PartialSnapshot` - частичный снимок, `ForEachChunk` отдает правильные интервалы.
10. `SnapshotIsStable` - снимок не меняется при последующих вставках.
11. `UnknownFileIsIgnored` - чанк для неизвестного id.
12. `RecollectResetsData` - повторный `CollectFile` сбрасывает данные.
13. `RemoveFileDropsData` - `RemoveFile` удаляет запись.
14. `WaitForCompletionSuccess` - ожидание реально просыпается по сигналу.
15. `WaitForCompletionTimeout` - таймаут и неизвестный id.
16. `ChunkBridgesTwoIntervals` - каскадное слияние двух соседей.
17. `SnapshotSurvivesRemoveFile` - снимок остается валидным после `RemoveFile`.

### Многопоточные

18. `ConcurrentChunksSameFile` - 8 потоков, 1 MiB, ~260 чанков по 4 KiB с перекрытием 100 байт, порядок перемешан. Проверяется, что файл собирается ровно в ожидаемое содержимое.
19. `ConcurrentDifferentFiles` - 16 файлов по 64 KiB, каждый собирается своим потоком в обратном порядке. Проверяется, что файлы не мешают друг другу и собираются побайтово верно.

Запустить все:

```bash
ctest --test-dir out/build/vs2022-x64-debug -C Debug --output-on-failure
```

Или через `--preset`:

```bash
ctest --preset vs2022-x64-debug
```

Или в Visual Studio: **Test → Test Explorer → Run All Tests**.

---

## Сборка

### Требования

- **CMake ≥ 3.21** (для presets; без них - ≥ 3.16).
- **Компилятор с C++20** (для `std::span`):
  - MSVC 19.29+ (Visual Studio 2019 16.10+),
  - GCC 10+,
  - Clang 12+.
- **Git** - при первой сборке CMake скачает GoogleTest через `FetchContent`.

### Visual Studio 2022

1. Установить workload **«Desktop development with C++»** и компонент **«C++ CMake tools for Windows»**.
2. **File → Open → Folder…** → выбрать корень проекта (там, где `CMakeLists.txt`).
3. VS автоматически подхватит `CMakePresets.json`; в выпадающем списке выбрать `vs2022-x64-debug` или `vs2022-x64-release`.
4. `Ctrl+Shift+B` - сборка.
5. **Test → Test Explorer → Run All Tests** - прогон 19 тестов.

### Командная строка (любая платформа)

С конфигурацией Debug:

```bash
cmake --preset vs2022-x64-debug
cmake --build --preset vs2022-x64-debug
ctest --preset vs2022-x64-debug --output-on-failure
```

С конфигурацией Release:

```bash
cmake --preset vs2022-x64-release
cmake --build --preset vs2022-x64-release
ctest --preset vs2022-x64-release --output-on-failure
```

### Без presets (классический CMake)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

---

## Ограничения и возможные улучшения

Сознательно **не** реализовано (вне рамок ТЗ):
- **Сетевое взаимодействие** - по условию не требуется.
- **Дисковая запись** - коллектор собирает данные в памяти; логика сохранения на диск - ответственность вызывающего кода.
- **Ограничение памяти** - если чанки приходят с большими разрывами, память тратится только под реально полученные интервалы (а не под всю "дырку"), но общий объем по всем файлам не ограничен.
- **Персистентность** - реестр живет в памяти процесса.

Возможные доработки:
- **`GetFile` с ожиданием** - если очень нужно "получить готовый файл или подождать", комбинация `WaitForCompletion` + `GetFile` закрывает сценарий, но можно было бы добавить `GetFileOrWait(id, timeout)`.
- **LRU-эвикшн** для реестра, если файлов очень много.
- **`std::pmr`-аллокаторы** - для снижения фрагментации при большом числе мелких чанков.
- **`ForEachChunk` под внешним колбэком** - сейчас обход идет по `map`, взятой из иммутабельного снапшота, поэтому безопасен.

---

## Лицензия / авторство

Тестовое задание. Автор - Куропатин Владислав Сергеевич, студент 2-го курса РТУ МИРЭА.
