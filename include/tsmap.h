// tsmap.h
// A thread safe hash map class to read and write
// from a map in a safe manner
#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace Hev {
template <typename K, typename V, size_t NumBuckets = 64> class TSMap {
  struct Bucket {
    std::map<K, V> data;
    mutable std::shared_mutex mutex;
  };

  std::vector<Bucket> buckets_;

  Bucket &get_bucket(const K &key) {
    return buckets_[std::hash<K>{}(key) % NumBuckets];
  }

public:
  TSMap() : buckets_(NumBuckets) {}

  void insert(const K &key, const V &value) {
    auto &bucket = get_bucket(key);
    std::unique_lock lock(bucket.mutex);
    bucket.data[key] = value;
  }

  std::optional<V> get(const K &key) const {
    const auto &bucket = get_bucket(key);
    std::shared_lock lock(bucket.mutex);
    auto it = bucket.data.find(key);
    if (it != bucket.data.end()) {
      return it->second;
    }
    return std::nullopt;
  }
};
} // namespace Hev
