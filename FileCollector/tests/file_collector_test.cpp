#include "file_collector.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <numeric>
#include <random>
#include <thread>
#include <utility>
#include <vector>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <chrono>

namespace
{

    using safe_board::FileCollector;

    // Удобный хелпер: строит вектор байт из initializer_list.
    std::vector<uint8_t> V(std::initializer_list<uint8_t> l) {
        return std::vector<uint8_t>(l); }

}

// Пустой коллектор: реестр пуст, GetFile для неизвестного id -> nullopt.
TEST(FileCollectorTest, EmptyCollector)
{
    FileCollector c{};
    EXPECT_EQ(c.FileCount(), 0u);
    EXPECT_FALSE(c.GetFile(1).has_value());
}

// Чанки приходят по порядку: файл собирается корректно.
TEST(FileCollectorTest, InOrderChunks)
{
    FileCollector c{};
    c.CollectFile(1, 6);
    c.OnNewChunk(1, 0, V({ 0, 1, 2 }));
    c.OnNewChunk(1, 3, V({ 3, 4, 5 }));
    auto snap{ c.GetFile(1) };
    ASSERT_TRUE(snap.has_value());
    EXPECT_TRUE(snap->IsComplete());
    auto bytes{ snap->Materialize() };
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, V({ 0, 1, 2, 3, 4, 5 }));
}

// Чанки приходят в обратном порядке: файл все равно собирается.
TEST(FileCollectorTest, OutOfOrderChunks)
{
    FileCollector c{};
    c.CollectFile(1, 6);
    c.OnNewChunk(1, 3, V({ 3, 4, 5 }));
    c.OnNewChunk(1, 0, V({ 0, 1, 2 }));
    auto bytes{ c.GetFile(1)->Materialize() };
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, V({ 0, 1, 2, 3, 4, 5 }));
}

// Пример из ТЗ: пересекающиеся чанки корректно сливаются.
TEST(FileCollectorTest, OverlappingChunksTaskExample)
{
    FileCollector c{};
    c.CollectFile(1, 10);
    c.OnNewChunk(1, 0, V({ 0, 1, 2, 3 }));
    c.OnNewChunk(1, 2, V({ 2, 3, 4, 5 }));
    c.OnNewChunk(1, 6, V({ 6, 7, 8, 9 }));
    auto bytes{ c.GetFile(1)->Materialize() };
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, V({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }));
}

// Дубликаты и вложенные чанки не увеличивают счетчик собранных байт.
TEST(FileCollectorTest, DuplicateChunksDoNotInflateProgress)
{
    FileCollector c{};
    c.CollectFile(1, 4);
    c.OnNewChunk(1, 0, V({ 0, 1, 2, 3 }));
    c.OnNewChunk(1, 0, V({ 0, 1, 2, 3 }));
    c.OnNewChunk(1, 1, V({ 1, 2 }));
    auto snap{ c.GetFile(1) };
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->CollectedBytes(), 4u);
    EXPECT_TRUE(snap->IsComplete());
    EXPECT_EQ(*snap->Materialize(), V({ 0, 1, 2, 3 }));
}

// Чанк, выходящий за fileSize, обрезается по границе файла.
TEST(FileCollectorTest, ChunkPastEndIsTruncated)
{
    FileCollector c{};
    c.CollectFile(1, 4);
    c.OnNewChunk(1, 2, V({ 2, 3, 4, 5, 6 }));
    c.OnNewChunk(1, 0, V({ 0, 1 }));
    auto bytes{ c.GetFile(1)->Materialize() };
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, V({ 0, 1, 2, 3 }));
}

// Чанк, начинающийся за fileSize, полностью игнорируется.
TEST(FileCollectorTest, ChunkStartingPastEndIsIgnored)
{
    FileCollector c{};
    c.CollectFile(1, 4);
    c.OnNewChunk(1, 4, V({ 9, 9 }));
    c.OnNewChunk(1, 100, V({ 9 }));
    auto snap{ c.GetFile(1) };
    EXPECT_EQ(snap->CollectedBytes(), 0u);
    EXPECT_FALSE(snap->Materialize().has_value());
}

// Файл размера 0 считается полным сразу после CollectFile.
TEST(FileCollectorTest, ZeroSizedFileIsImmediatelyComplete)
{
    FileCollector c{};
    c.CollectFile(1, 0);
    auto snap{ c.GetFile(1) };
    ASSERT_TRUE(snap.has_value());
    EXPECT_TRUE(snap->IsComplete());
    auto bytes{ snap->Materialize() };
    ASSERT_TRUE(bytes.has_value());
    EXPECT_TRUE(bytes->empty());
}

// Частичный снимок: доступны только реально полученные интервалы.
TEST(FileCollectorTest, PartialSnapshot)
{
    FileCollector c{};
    c.CollectFile(1, 10);
    c.OnNewChunk(1, 0, V({ 0, 1, 2 }));
    c.OnNewChunk(1, 7, V({ 7, 8 }));
    auto snap{ c.GetFile(1) };
    ASSERT_TRUE(snap.has_value());
    EXPECT_FALSE(snap->IsComplete());
    EXPECT_EQ(snap->CollectedBytes(), 5u);
    EXPECT_FALSE(snap->Materialize().has_value());
    std::vector<std::pair<size_t, std::vector<uint8_t>>> ranges{};
    snap->ForEachChunk([&](size_t pos, std::span<const uint8_t> d) {
        ranges.emplace_back(pos, std::vector<uint8_t>(d.begin(), d.end())); });
    ASSERT_EQ(ranges.size(), 2u);
    EXPECT_EQ(ranges[0].first, 0u);
    EXPECT_EQ(ranges[0].second, V({ 0, 1, 2 }));
    EXPECT_EQ(ranges[1].first, 7u);
    EXPECT_EQ(ranges[1].second, V({ 7, 8 }));
}

// Снимок фиксирует состояние: последующие вставки его не меняют.
TEST(FileCollectorTest, SnapshotIsStable)
{
    FileCollector c{};
    c.CollectFile(1, 4);
    c.OnNewChunk(1, 0, V({ 0, 1 }));
    auto early{ c.GetFile(1) };
    ASSERT_TRUE(early.has_value());
    c.OnNewChunk(1, 2, V({ 2, 3 }));
    auto late{ c.GetFile(1) };
    ASSERT_TRUE(late.has_value());
    EXPECT_FALSE(early->IsComplete());
    EXPECT_EQ(early->CollectedBytes(), 2u);
    EXPECT_TRUE(late->IsComplete());
    EXPECT_EQ(*late->Materialize(), V({ 0, 1, 2, 3 }));
}

// Чанк для незарегистрированного fileId игнорируется.
TEST(FileCollectorTest, UnknownFileIsIgnored)
{
    FileCollector c{};
    c.OnNewChunk(42, 0, V({ 1, 2, 3 }));
    EXPECT_EQ(c.FileCount(), 0u);
    EXPECT_FALSE(c.GetFile(42).has_value());
}

// Повторный CollectFile сбрасывает ранее собранные данные.
TEST(FileCollectorTest, RecollectResetsData)
{
    FileCollector c{};
    c.CollectFile(1, 4);
    c.OnNewChunk(1, 0, V({ 0, 1, 2, 3 }));
    EXPECT_TRUE(c.GetFile(1)->IsComplete());
    c.CollectFile(1, 4);
    auto snap{ c.GetFile(1) };
    EXPECT_EQ(snap->CollectedBytes(), 0u);
    EXPECT_FALSE(snap->IsComplete());
}

// RemoveFile удаляет запись из реестра.
TEST(FileCollectorTest, RemoveFileDropsData)
{
    FileCollector c{};
    c.CollectFile(1, 4);
    c.RemoveFile(1);
    EXPECT_EQ(c.FileCount(), 0u);
    EXPECT_FALSE(c.GetFile(1).has_value());
}

// WaitForCompletion просыпается по сигналу о завершении сборки.
TEST(FileCollectorTest, WaitForCompletionSuccess)
{
    FileCollector c{};
    c.CollectFile(1, 100);
    std::thread producer{ [&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        c.OnNewChunk(1, 0, std::vector<uint8_t>(100, 7));
    } };
    EXPECT_TRUE(c.WaitForCompletion(1, std::chrono::seconds(5)));
    producer.join();
    EXPECT_TRUE(c.GetFile(1)->IsComplete());
}

// WaitForCompletion корректно возвращает false по таймауту
// и для незарегистрированного fileId.
TEST(FileCollectorTest, WaitForCompletionTimeout)
{
    FileCollector c{};
    c.CollectFile(1, 100);
    EXPECT_FALSE(c.WaitForCompletion(1, std::chrono::milliseconds(20)));
    EXPECT_FALSE(c.WaitForCompletion(999, std::chrono::milliseconds(20)));
}

// Многопоточная вставка чанков в один файл в случайном порядке.
TEST(FileCollectorTest, ConcurrentChunksSameFile)
{
    constexpr size_t kSize{ 1 << 20 };
    FileCollector c{};
    c.CollectFile(7, kSize);
    std::vector<uint8_t> expected(kSize);
    std::mt19937 rng{ 42 };
    for (auto& b : expected)
        b = static_cast<uint8_t>(rng());
    std::vector<std::pair<size_t, size_t>> ranges{};
    for (size_t p{ 0 }; p < kSize; p += 4096)
    {
        size_t len{ std::min<size_t>(4096 + 100, kSize - p) };
        ranges.emplace_back(p, len);
    }
    std::shuffle(ranges.begin(), ranges.end(), rng);
    constexpr int kThreads{ 8 };
    std::atomic<size_t> next{ 0 };
    std::vector<std::thread> threads{};
    threads.reserve(kThreads);
    for (int t{}; t < kThreads; ++t)
    {
        threads.emplace_back([&] {
            size_t i{};
            while ((i = next.fetch_add(1)) < ranges.size())
            {
                auto [p, len] = ranges[i];
                std::vector<uint8_t> chunk(expected.begin() + p,
                    expected.begin() + p + len);
                c.OnNewChunk(7, p, std::move(chunk));
            }
            });
    }
    for (auto& th : threads)
        th.join();
    auto snap{ c.GetFile(7) };
    ASSERT_TRUE(snap.has_value());
    ASSERT_TRUE(snap->IsComplete());
    auto bytes{ snap->Materialize() };
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, expected);
}

// Параллельная работа с разными файлами: файлы не мешают друг другу.
TEST(FileCollectorTest, ConcurrentDifferentFiles)
{
    constexpr uint32_t kFiles{ 16 };
    constexpr size_t kSize{ 64 * 1024 };
    std::vector<uint8_t> payload(kSize);
    std::iota(payload.begin(), payload.end(), uint8_t{ 0 });
    FileCollector c{};
    for (uint32_t f{ 0 }; f < kFiles; ++f)
        c.CollectFile(f, kSize);
    std::vector<std::thread> threads{};
    threads.reserve(kFiles);
    for (uint32_t f{ 0 }; f < kFiles; ++f)
    {
        threads.emplace_back([&, f] {
            for (size_t pos{ kSize }; pos > 0; pos -= 1024)
            {
                size_t p{ pos - 1024 };
                c.OnNewChunk(f, p, std::vector<uint8_t>(
                    payload.begin() + p, payload.begin() + p + 1024));
            }
            });
    }
    for (auto& th : threads)
        th.join();
    for (uint32_t f{ 0 }; f < kFiles; ++f)
    {
        auto snap{ c.GetFile(f) };
        ASSERT_TRUE(snap.has_value());
        auto bytes{ snap->Materialize() };
        ASSERT_TRUE(bytes.has_value());
        EXPECT_EQ(*bytes, payload);
    }
}

// Чанк, сшивающий два соседних интервала (проверяет каскадное слияние).
TEST(FileCollectorTest, ChunkBridgesTwoIntervals) {
    FileCollector c{};
    c.CollectFile(1, 5);
    c.OnNewChunk(1, 0, V({ 0 }));
    c.OnNewChunk(1, 4, V({ 4 }));
    c.OnNewChunk(1, 1, V({ 1, 2, 3 }));
    auto snap{ c.GetFile(1) };
    ASSERT_TRUE(snap.has_value());
    EXPECT_TRUE(snap->IsComplete());
    EXPECT_EQ(*snap->Materialize(), V({ 0, 1, 2, 3, 4 }));
    size_t ranges{};
    snap->ForEachChunk([&](size_t, std::span<const uint8_t>) { ++ranges; });
    EXPECT_EQ(ranges, 1u);
}

// RemoveFile не ломает ранее выданный снимок (проверяет shared_ptr-семантику).
TEST(FileCollectorTest, SnapshotSurvivesRemoveFile) {
    FileCollector c{};
    c.CollectFile(1, 4);
    c.OnNewChunk(1, 0, V({ 0, 1, 2, 3 }));
    auto snap{ c.GetFile(1) };
    ASSERT_TRUE(snap.has_value());
    c.RemoveFile(1);
    EXPECT_FALSE(c.GetFile(1).has_value());
    EXPECT_TRUE(snap->IsComplete());
    EXPECT_EQ(*snap->Materialize(), V({ 0, 1, 2, 3 }));
}
