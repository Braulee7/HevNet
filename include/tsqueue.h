// A thread safe queue which just wraps the
// std queue but adds a mutex to safely
// read and write to the container

#include <condition_variable>
#include <mutex>
#include <queue>

#define LOCK(mut) std::unique_lock lock(mut)

namespace Hev {

/* Thread safe Queue
 * Wrapper over a std::queue that implements a mutex
 * to make any read and writes thread safe
 */
template <class T, class container = std::deque<T>> class TSQueue {
public:
  TSQueue() = default;
  TSQueue(TSQueue &&other) {
    if (this == &other)
      return;
    std::unique_lock t_lock(this->m_mut);
    std::unique_lock o_lock(other.m_mut);

    this->m_queue = std::move(other.m_queue);
  }
  ~TSQueue() {
    // release lock and notify all to unblock any threads
    // use of the queue beyond this is undefined behavior
    m_mut.unlock();
    m_cond.notify_all();
  }

  TSQueue &operator=(TSQueue &&other) {
    if (this == &other)
      return *this;
    std::unique_lock t_lock(this->m_mut);
    std::unique_lock o_lock(other.m_mut);

    this->m_queue = std::move(other.m_queue);
    return *this;
  }

  /* front:
   * Retrieves the first element in the queue
   */
  T &front() {
    LOCK(m_mut);
    return m_queue.front();
  }

  /* front:
   * Retrieves a constant reference to the first element
   * in the queue
   */
  const T &front() const {
    LOCK(m_mut);
    return m_queue.front();
  }

  /* front:
   * Retrieves the last element in the queue
   */
  T &back() {
    LOCK(m_mut);
    return m_queue.back();
  }

  /* front:
   * Retrieves a constant reference to the last element
   * in the queue
   */
  const T &back() const {
    LOCK(m_mut);
    return m_queue.back();
  }

  /* empty
   * checks if the queue is empty
   */
  bool empty() {
    LOCK(m_mut);
    return m_queue.empty();
  }

  /* size
   * retrieves the current number of elements in the list
   */
  size_t size() const {
    LOCK(m_mut);
    return m_queue.size();
  }

  /* push
   * adds an element ot the queue and notifies the first thread
   * waiting on the condition variable so it can retrieve it
   */
  void push(const T &value) {
    LOCK(m_mut);
    m_queue.push(value);
    m_cond.notify_one();
  }

  /* push
   * adds an element ot the queue and notifies the first thread
   * waiting on the condition variable so it can retrieve it
   */
  void push(T &&value) {
    LOCK(m_mut);
    m_queue.push(std::move(value));
    m_cond.notify_one();
  }

  /* emplace
   * adds an element ot the queue and notifies the first thread
   * waiting on the condition variable so it can retrieve it.
   * This one just construct the element in place wrather than
   * taking an already constructed element
   */
  template <class... Args> void emplace(Args &&...args) {
    LOCK(m_mut);
    m_queue.emplace(std::forward<Args>(args)...);
    m_cond.notify_one();
  }

  /* pop
   * deletes the last element in the queue
   */
  void pop() {
    LOCK(m_mut);
    m_queue.pop();
  }

  // waits until an item is available in the queue
  /* pop_wait
   * waits until an item is in the queue to pop it
   * and return it to the user
   * param:
   *  item: the item that is retrieved from the queue.
   * returns
   *  boolean indicating the status of the wait. If it is false
   *  a spureous wake may have occurred and item is undefined
   */
  bool pop_wait(T *item) {
    LOCK(m_mut);
    m_cond.wait(lock, [this]() { return !m_queue.empty(); });

    if (!item)
      return false;
    *item = std::move(m_queue.front());
    m_queue.pop();
    return item;
  }

  /* pop_wait_till
   * Waits until an item is in the queue to pop it for some designated
   * amount of time. It either retrieves an item or unblocks and item
   * is undefined
   * param:
   *  ms: amount of time to wait for
   *  item: the item retrieved if any
   * returns:
   *  boolean indicating the status of the wait. True if an item was
   *  retrieved, false otherwise and item is undefined
   */
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
