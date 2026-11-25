#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace DocStudio {

// Thread-safe byte-budgeted LRU cache for rendered pages, thumbnails, and decoded tiles.
template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LruByteCache {
public:
    explicit LruByteCache(std::size_t maxBytes = 256ull * 1024ull * 1024ull)
        : maxBytes_(maxBytes) {}

    void SetMaxBytes(std::size_t maxBytes) {
        std::lock_guard<std::mutex> lk(mtx_);
        maxBytes_ = maxBytes;
        EvictToBudgetLocked();
    }

    std::size_t MaxBytes() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return maxBytes_;
    }

    std::size_t BytesUsed() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return bytesUsed_;
    }

    std::size_t EntryCount() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return map_.size();
    }

    std::uint64_t EvictionCount() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return evictions_;
    }

    bool TryGet(const Key& key, Value& outVal) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        order_.splice(order_.begin(), order_, it->second);
        outVal = it->second->value;
        return true;
    }

    bool Put(const Key& key, Value val, std::size_t byteSize) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            SubtractBytesLocked(it->second->byteSize);
            order_.erase(it->second);
            map_.erase(it);
        }
        if (maxBytes_ > 0 && byteSize > maxBytes_) {
            EvictToBudgetLocked();
            return false;
        }
        order_.push_front({ key, std::move(val), byteSize });
        map_[key] = order_.begin();
        bytesUsed_ += byteSize;
        EvictToBudgetLocked();
        return true;
    }

    template <typename Pred>
    void InvalidateIf(Pred pred) {
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto it = order_.begin(); it != order_.end();) {
            if (pred(it->key)) {
                SubtractBytesLocked(it->byteSize);
                map_.erase(it->key);
                it = order_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void Clear() {
        std::lock_guard<std::mutex> lk(mtx_);
        order_.clear();
        map_.clear();
        bytesUsed_ = 0;
    }

private:
    struct Entry {
        Key key;
        Value value;
        std::size_t byteSize = 0;
    };

    void SubtractBytesLocked(std::size_t sz) noexcept {
        bytesUsed_ = (bytesUsed_ >= sz) ? (bytesUsed_ - sz) : 0;
    }

    void EvictToBudgetLocked() {
        if (maxBytes_ == 0) return;
        while (!order_.empty() && bytesUsed_ > maxBytes_) {
            const auto& back = order_.back();
            SubtractBytesLocked(back.byteSize);
            map_.erase(back.key);
            order_.pop_back();
            ++evictions_;
        }
    }

    mutable std::mutex mtx_;
    std::size_t maxBytes_ = 256ull * 1024ull * 1024ull;
    std::size_t bytesUsed_ = 0;
    std::uint64_t evictions_ = 0;
    std::list<Entry> order_;
    std::unordered_map<Key, typename std::list<Entry>::iterator, Hash> map_;
};

} // namespace DocStudio
