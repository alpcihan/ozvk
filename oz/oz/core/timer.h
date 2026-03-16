#pragma once

#include <chrono>
#include <cstdint>

namespace oz {

class Timer {
  public:
    Timer();

    void tick();

    float    deltaTime() const;
    float    elapsed() const;
    float    fps() const;
    uint64_t frameCount() const;

  private:
    using Clock     = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    TimePoint m_start;
    TimePoint m_lastTick;
    float     m_deltaTime  = 0.0f;
    uint64_t  m_frameCount = 0;
};

} // namespace oz
