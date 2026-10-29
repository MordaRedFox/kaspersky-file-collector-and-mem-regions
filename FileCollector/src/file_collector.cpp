#include "file_collector.h"
#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <functional>
#include <span>
#include <cstdint>
#include <optional>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <chrono>

namespace safe_board
{

    // FileSnapshot

    FileSnapshot::FileSnapshot(
        size_t fileSize, size_t collectedBytes, std::shared_ptr<const ChunkMap> chunks)
        : fileSize_(fileSize), collectedBytes_(collectedBytes), chunks_(std::move(chunks))
    {}

    void FileSnapshot::ForEachChunk(const std::function<void(
        size_t, std::span<const uint8_t>)>& fn) const
    {
        for (const auto& [pos, data] : *chunks_)
            fn(pos, std::span<const uint8_t>(data->data(), data->size()));
    }

    std::optional<std::vector<uint8_t>> FileSnapshot::Materialize() const
    {
        if (!IsComplete())
            return std::nullopt;
        std::vector<uint8_t> out(fileSize_);
        for (const auto& [pos, data] : *chunks_)
            std::copy(data->begin(), data->end(),
                out.begin() + static_cast<std::ptrdiff_t>(pos));
        return out;
    }

    // FileCollector

    void FileCollector::CollectFile(uint32_t fileId, size_t fileSize)
    {
        auto fd{ std::make_shared<FileData>() };
        fd->fileSize = fileSize;
        {
            std::unique_lock<std::shared_mutex> lock{ registryMtx_ };
            files_[fileId] = std::move(fd);
        }
    }

    void FileCollector::OnNewChunk(uint32_t fileId, size_t pos,
        std::vector<uint8_t>&& chunk)
    {
        if (chunk.empty())
            return;
        auto fd{ FindFile(fileId) };
        if (!fd)
            return;
        bool completed{};
        {
            std::lock_guard<std::mutex> lock{ fd->mtx };
            if (pos >= fd->fileSize)
                return;
            const size_t len{ std::min(chunk.size(), fd->fileSize - pos) };
            const size_t chunkEnd{ pos + len };
            auto& chunks{ fd->chunks };
            auto it{ chunks.upper_bound(pos) };
            if (it != chunks.begin())
            {
                auto prev{ std::prev(it) };
                if (prev->first + prev->second->size() >= pos)
                    it = prev;
            }
            auto first{ it }, last{ it };
            size_t newStart{ pos }, newEnd{ chunkEnd }, removedBytes{};
            while (last != chunks.end() && last->first <= newEnd)
            {
                newStart = std::min(newStart, last->first);
                newEnd = std::max(newEnd, last->first + last->second->size());
                removedBytes += last->second->size();
                ++last;
            }
            std::vector<uint8_t> merged(newEnd - newStart);
            for (auto jt{ first }; jt != last; ++jt)
            {
                std::copy(jt->second->begin(), jt->second->end(),
                    merged.begin() + static_cast<std::ptrdiff_t>(jt->first - newStart));
            }
            std::copy_n(chunk.begin(), len,
                merged.begin() + static_cast<std::ptrdiff_t>(pos - newStart));
            if (first != last)
                chunks.erase(first, last);
            chunks.emplace(newStart,
                std::make_shared<const std::vector<uint8_t>>(std::move(merged)));
            fd->collectedBytes = fd->collectedBytes - removedBytes + (newEnd - newStart);
            completed = fd->collectedBytes >= fd->fileSize;
        }
        if (completed)
            fd->cv.notify_all();
    }

    std::optional<FileSnapshot> FileCollector::GetFile(uint32_t fileId) const
    {
        auto fd{ FindFile(fileId) };
        if (!fd)
            return std::nullopt;
        std::lock_guard<std::mutex> lock{ fd->mtx };
        auto chunksCopy{ std::make_shared<FileSnapshot::ChunkMap>(fd->chunks) };
        return FileSnapshot(fd->fileSize, fd->collectedBytes, std::move(chunksCopy));
    }

    bool FileCollector::WaitForCompletion(uint32_t fileId,
        std::chrono::milliseconds timeout) const
    {
        auto fd{ FindFile(fileId) };
        if (!fd)
            return false;
        std::unique_lock<std::mutex> lock{ fd->mtx };
        return fd->cv.wait_for(lock, timeout, [&fd] {
            return fd->collectedBytes >= fd->fileSize; });
    }

    void FileCollector::RemoveFile(uint32_t fileId)
    {
        std::unique_lock<std::shared_mutex> lock{ registryMtx_ };
        files_.erase(fileId);
    }

    size_t FileCollector::FileCount() const
    {
        std::shared_lock<std::shared_mutex> lock{ registryMtx_ };
        return files_.size();
    }

    std::shared_ptr<FileCollector::FileData> FileCollector::FindFile(
        uint32_t fileId) const
    {
        std::shared_lock<std::shared_mutex> lock{ registryMtx_ };
        auto it{ files_.find(fileId) };
        return it == files_.end() ? nullptr : it->second;
    }

}
