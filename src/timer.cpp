#include "timer.h"
namespace Hev {

Timeout::Timeout(const int timeout_in_ms) : m_time(timeout_in_ms) {
  m_finished.store(false);
}

const int Timeout::Start() {
  m_finished.store(false);
  return timer(m_time, true, [this]() { this->m_finished.store(true); });
}

const bool Timeout::IsFinished() const { return m_finished.load(); }
} // namespace Hev
