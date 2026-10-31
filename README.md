# FileCollector && MemRegions

Этот репозиторий содержит решение двух технических задач для отбора на стажировку на позицию C++ Developer в Лабораторию Касперского.

---

## Структура проекта

```bash
.
├── FileCollector/
│   ├── include/
│   │   └── file_collector.h         // публичный API (FileCollector, FileSnapshot)
│   │
│   ├── src/
│   │   └── file_collector.cpp       // реализация
│   │
│   ├── tests/
│   │   └── file_collector_test.cpp  // тесты на GoogleTest
│   │
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   ├── LICENSE
│   └── README.md
│
├── MemRegions/
│   ├── bin/
│   │   └── MemRegions.exe           // собранный Release-бинарник
│   │
│   ├── include/
│   │   └── memregions/
│   │       ├── application.h        // Application
│   │       ├── common.h             // MemoryRegion, UniqueProcessHandle
│   │       ├── process_locator.h    // IProcessLocator + WindowsProcessLocator
│   │       ├── protection.h         // kReadableMask, IsReadableRegion, описание
│   │       ├── region_scanner.h     // IRegionScanner + WindowsRegionScanner
│   │       └── reporter.h           // IReporter + ConsoleReporter
│   │
│   ├── src/
│   │   ├── application.cpp          // оркестрация
│   │   ├── main.cpp
│   │   ├── process_locator.cpp
│   │   ├── protection.cpp
│   │   ├── region_scanner.cpp
│   │   └── reporter.cpp
│   │
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   ├── LICENSE
│   └── README.md
│
├── .gitignore
├── LICENSE
└── README.md
```

---

## FileCollector

Потокобезопасный класс `FileCollector`, собирающий файл из чанков, приходящих по "сети" в произвольном порядке. Чанки могут пересекаться, дублироваться и приходить с разрывами. Один файл может собираться несколькими потоками одновременно; разные файлы синхронизируются независимо и не мешают друг другу.

---

## MemRegions

Консольная утилита, которая по **PID** или **имени процесса** выводит в консоль только те регионы виртуальной памяти целевого процесса, которые можно прочитать через `ReadProcessMemory` без ошибки доступа. Для каждого региона печатаются адрес начала, адрес конца, размер и параметры защиты.

---

## Лицензия / авторство

Тестовое задание. Автор - Куропатин Владислав Сергеевич, студент 2-го курса РТУ МИРЭА.
