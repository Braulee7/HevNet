// A thread safe queue which just wraps the
// std queue but adds a mutex to safely
// read and write to the container

#include <condition_variable>
#include <mutex>
#include <queue>

#define LOCK(mut) std::unique_lock lock(mut)

namespace Hev {

template <class T, class container = std::deque<T>> class TSQueue {
public:
  TSQueue() = default;
  ~TSQueue() {
    // release lock and notify all to unblock any threads
    // use of the queue beyond this is undefined behavior
    m_mut.unlock();
    m_cond.notify_all();
  }

  T &front() {
    LOCK(m_mut);
    return m_queue.front();
  }

  const T &front() const {
    LOCK(m_mut);
    return m_queue.front();
  }

  T &back() {
    LOCK(m_mut);
    return m_queue.back();
  }

  const T &back() const {
    LOCK(m_mut);
    return m_queue.back();
  }

  bool empty() {
    LOCK(m_mut);
    return m_queue.empty();
  }

  size_t size() const {
    LOCK(m_mut);
    return m_queue.size();
  }

  void push(const T &value) {
    LOCK(m_mut);
    m_queue.push(value);
    m_cond.notify_one();
  }

  void push(T &&value) {
    LOCK(m_mut);
    m_queue.push(std::move(value));
    m_cond.notify_one();
  }
  template <class... Args> void emplace(Args &&...args) {
    LOCK(m_mut);
    m_queue.emplace(std::forward<Args>(args)...);
    m_cond.notify_one();
  }

  void pop() {
    LOCK(m_mut);
    m_queue.pop();
  }

  // waits until an item is available in the queue
  bool pop_wait(T *item) {
    LOCK(m_mut);
    m_cond.wait(lock, [this]() { return !m_queue.empty(); });

    if (!item)
      return false;
    *item = std::move(m_queue.front());
    m_queue.pop();
    return item;
  }

  bool pop_wait_till(std::chrono::milliseconds ms, T *item) {
    LOCK(m_mut);
    if (!m_cond.wait_for(lock, ms, [this]() { return !m_queue.empty(); })) {
      return false;
    }
    if (!item) {
      return false;
    }
    *(item) = std::move(m_queue.front());
    m_queue.pop();
    return item;
  }

private:
  std::queue<T, container> m_queue;
  std::mutex m_mut;
  std::condition_variable m_cond;
};

} // namespace Hev
