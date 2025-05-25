// tsmap.h
// A thread safe hash map class to read and write
// from a map in a safe manner
#include <map>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace Hev {
template <typename K, typename V, size_t NumBuckets = 64> class TSMap {
  struct Bucket {
    std::map<K, V> data;
    mutable std::shared_mutex mutex;
  };

  std::vector<Bucket> m_buckets;

  Bucket &get_bucket(const K &key) {
    return m_buckets[std::hash<K>{}(key) % NumBuckets];
  }

public:
  TSMap() : m_buckets(NumBuckets) {}

  void insert(const K &key, const V &value) {
    auto &bucket = get_bucket(key);
    std::unique_lock lock(bucket.mutex);
    bucket.data[key] = value;
  }

  bool Remove(const K &key) {
    auto &bucket = get_bucket(key);
    std::unique_lock lock(bucket.mutex);
    return bucket.data.erase(key) > 0;
  }

  bool get(const K &key, V &value) const {
    const auto &bucket = get_bucket(key);
    std::shared_lock lock(bucket.mutex);
    auto it = bucket.data.find(key);
    if (it != bucket.data.end()) {
      value = it->second;
      return true;
    }
    return false;
  }

  std::vector<V> GetGreaterThan(const K &key) {
    std::vector<V> results(25);
    for (auto &bucket : m_buckets) {
      std::shared_lock lock(bucket.mutex);
      for (const auto &[seq, buff] : bucket.data) {
        if (seq < key) {
          results.push_back(buff);
        }
      }
    }

    return results;
  }
};
} // namespace Hev
