// timer.h
// A timer which will start on call and call a
// call back function when time ends.

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <type_traits>
namespace Hev {

template <class callable, class... arguments>
int timer(std::chrono::milliseconds &after, bool async, callable &&f,
          arguments &&...args) {
  std::function<typename std::result_of<callable(arguments...)>::type()> task(
      std::bind(std::forward<callable>(f), std::forward(args)...));

  if (async) {
    std::thread([after, task]() {
      std::this_thread::sleep_for(after);
      task();
    }).detach();
  } else {
    std::this_thread::sleep_for(after);
    task();
  }
  return 0;
}

class Timeout {
public:
  Timeout(const int timeout_in_ms);
  ~Timeout() = default;

  const int Start();
  const bool IsFinished() const;

private:
  std::chrono::milliseconds m_time;
  std::atomic<bool> m_finished;
};

} // namespace Hev
