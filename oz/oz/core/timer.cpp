#include "oz/core/timer.h"

namespace oz {

Timer::Timer() : m_start(Clock::now()), m_lastTick(m_start) {}

void Timer::tick() {
    auto now    = Clock::now();
    m_deltaTime = std::chrono::duration<float>(now - m_lastTick).count();
    m_lastTick  = now;
    m_frameCount++;
}

float Timer::deltaTime() const { return m_deltaTime; }

float Timer::elapsed() const {
    return std::chrono::duration<float>(Clock::now() - m_start).count();
}

float Timer::fps() const {
    return m_deltaTime > 0.0f ? 1.0f / m_deltaTime : 0.0f;
}

uint64_t Timer::frameCount() const { return m_frameCount; }

} // namespace oz
