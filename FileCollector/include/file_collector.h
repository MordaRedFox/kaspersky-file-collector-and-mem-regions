#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <unordered_map>
#include <vector>

namespace safe_board
{

    class FileCollector;

    /**
     * @brief Неизменяемый снимок собранных данных одного файла.
     * Представляет состояние конкретного файла (объявленный размер, объем
     * уже полученных байт и сами интервалы данных) на момент вызова
     * FileCollector::GetFile(). Снимок не изменяется после создания и может
     * безопасно использоваться из любого потока.
     */
    class FileSnapshot
    {
    public:
        using ChunkMap =
            std::map<size_t, std::shared_ptr<const std::vector<uint8_t>>>;

        FileSnapshot() = delete;
        FileSnapshot(const FileSnapshot&) = default;
        FileSnapshot& operator=(const FileSnapshot&) = default;
        FileSnapshot(FileSnapshot&&) noexcept = default;
        FileSnapshot& operator=(FileSnapshot&&) noexcept = default;
        ~FileSnapshot() = default;

        /**
         * @brief Объявленный размер файла (тот, что был передан в CollectFile).
         * @return Размер файла в байтах.
         */
        size_t FileSize() const noexcept { return fileSize_; }

        /**
         * @brief Сколько байт файла уже доступно в снимке.
         * Значение всегда <= FileSize(). Может быть меньше, если часть чанков
         * еще не пришла или пришли с разрывами.
         * @return Количество полученных байт.
         */
        size_t CollectedBytes() const noexcept { return collectedBytes_; }

        /**
         * @brief Проверяет, полностью ли собран файл.
         * @return true, если получены все байты от 0 до FileSize().
         */
        bool IsComplete() const noexcept { return collectedBytes_ >= fileSize_; }

        /**
         * @brief Обходит все доступные непрерывные диапазоны по возрастанию позиции.
         * Позволяет обработать уже полученные данные без материализации всего
         * файла и без копирования payload. Колбэк получает позицию начала
         * диапазона и std::span на его байты.
         * @param fn Колбэк, вызываемый для каждого интервала.
         */
        void ForEachChunk(const std::function<void(
            size_t pos, std::span<const uint8_t> data)>& fn) const;

        /**
         * @brief Собирает файл целиком в один буфер.
         * Копирует payload ровно один раз (этого требует возвращаемый тип).
         * Если файл еще не полон, возвращается std::nullopt - частичная
         * материализация через этот метод не поддерживается; для частичных
         * данных используйте ForEachChunk().
         * @return Вектор байт файла, если IsComplete() == true; иначе nullopt.
         */
        std::optional<std::vector<uint8_t>> Materialize() const;

    private:
        friend class FileCollector;

        /**
         * @brief Конструктор, доступный только FileCollector.
         * @param fileSize Объявленный размер файла.
         * @param collectedBytes Сколько байт уже собрано.
         * @param chunks Разделяемый набор интервалов данных.
         */
        FileSnapshot(size_t fileSize, size_t collectedBytes,
            std::shared_ptr<const ChunkMap> chunks);

        size_t fileSize_{};
        size_t collectedBytes_{};
        std::shared_ptr<const ChunkMap> chunks_;
    };

    /**
     * @brief Потокобезопасный сборщик файлов, приходящих чанками.
     * Класс принимает чанки файла в произвольном порядке (в том числе
     * пересекающиеся и дублирующиеся) и восстанавливает непрерывное
     * содержимое файла. Один файл может собираться несколькими потоками
     * одновременно; разные файлы синхронизируются независимо и не блокируют
     * друг друга.
     */
    class FileCollector
    {
    public:
        FileCollector() = default;
        FileCollector(const FileCollector&) = delete;
        FileCollector& operator=(const FileCollector&) = delete;
        FileCollector(FileCollector&&) = delete;
        FileCollector& operator=(FileCollector&&) = delete;
        ~FileCollector() = default;

        /**
         * @brief Регистрирует файл заданного размера.
         * Создает внутреннюю структуру для хранения чанков. Если файл с таким
         * идентификатором уже зарегистрирован, его ранее собранные данные
         * @param fileId   Идентификатор файла.
         * @param fileSize Ожидаемый размер файла в байтах. 0 - корректное
         * значение: такой файл считается полным сразу.
         */
        void CollectFile(uint32_t fileId, size_t fileSize);

        /**
         * @brief Добавляет очередной чанк файла.
         * Чанки могут приходить в любом порядке, пересекаться и дублироваться.
         * Вставка сливает новый диапазон с пересекающимися/примыкающими
         * соседями, поддерживая инвариант «непересекающиеся максимальные
         * интервалы». Полезная нагрузка перемещается (rvalue-ссылка), лишних
         * копирований нет.
         * @param fileId Идентификатор файла.
         * @param pos Позиция начала чанка в файле.
         * @param chunk Данные чанка (перемещаются внутрь коллектора).
         */
        void OnNewChunk(uint32_t fileId, size_t pos, std::vector<uint8_t>&& chunk);

        /**
         * @brief Возвращает снимок текущего состояния файла.
         * Стоимость - O(n) по числу интервалов, без копирования payload
         * (данные разделяются через shared_ptr). Возвращенный снимок
         * неизменяем и потокобезопасен.
         * @param fileId Идентификатор файла.
         * @return Снимок при успехе; std::nullopt, если файл не зарегистрирован.
         */
        std::optional<FileSnapshot> GetFile(uint32_t fileId) const;

        /**
         * @brief Ожидает полной сборки файла или истечения таймаута.
         * Блокирует вызывающий поток до тех пор, пока файл не будет собран
         * полностью, либо пока не истечет timeout. Удобно для сценариев,
         * в которых продюсер чанков работает в отдельном потоке.
         * @param fileId  Идентификатор файла.
         * @param timeout Максимальное время ожидания.
         * @return true, если файл полон; false при таймауте или если файл
         * не зарегистрирован.
         */
        bool WaitForCompletion(uint32_t fileId,
            std::chrono::milliseconds timeout) const;

        /**
         * @brief Удаляет файл из коллектора.
         * После вызова последующие OnNewChunk/GetFile для данного fileId
         * будут игнорироваться / возвращать nullopt. Ранее выданные снимки
         * остаются валидными, так как они владеют разделяемыми данными.
         * @param fileId Идентификатор файла.
         */
        void RemoveFile(uint32_t fileId);

        /**
         * @brief Возвращает число зарегистрированных (не удаленных) файлов.
         * @return Текущее количество файлов в реестре.
         */
        size_t FileCount() const;

    private:
        /**
         * @brief Внутреннее состояние одного файла.
         * Защищено собственным mtx; cv используется WaitForCompletion.
         * Отдельный мьютекс на файл позволяет параллельно обновлять
         * разные файлы без общей блокировки.
         */
        struct FileData
        {
            size_t fileSize{};
            size_t collectedBytes{};
            FileSnapshot::ChunkMap chunks;
            mutable std::mutex mtx;
            mutable std::condition_variable cv;
        };

        /**
         * @brief Находит запись файла в реестре под shared_lock.
         * @param fileId Идентификатор файла.
         * @return shared_ptr на состояние или nullptr, если файл
         * не зарегистрирован.
         */
        std::shared_ptr<FileData> FindFile(uint32_t fileId) const;

        mutable std::shared_mutex registryMtx_;
        std::unordered_map<uint32_t, std::shared_ptr<FileData>> files_;
    };

} 
