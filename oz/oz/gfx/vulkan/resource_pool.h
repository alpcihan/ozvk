#pragma once

#include <cassert>
#include <cstdint>
#include <queue>
#include <vector>

namespace oz::gfx::vk {

template <typename T> class ResourcePool {
  public:
    uint32_t add(T&& obj) {
        uint32_t id;
        if (!m_freeSlots.empty()) {
            id = m_freeSlots.front();
            m_freeSlots.pop();
            m_objects[id] = std::move(obj);
            m_alive[id]   = true;
        } else {
            id = static_cast<uint32_t>(m_objects.size());
            m_objects.push_back(std::move(obj));
            m_alive.push_back(true);
        }
        return id;
    }

    T& get(uint32_t id) {
        assert(id < m_objects.size() && m_alive[id]);
        return m_objects[id];
    }

    const T& get(uint32_t id) const {
        assert(id < m_objects.size() && m_alive[id]);
        return m_objects[id];
    }

    void remove(uint32_t id) {
        assert(id < m_objects.size() && m_alive[id]);
        m_alive[id] = false;
        m_freeSlots.push(id);
    }

    bool isValid(uint32_t id) const { return id < m_objects.size() && m_alive[id]; }

    template <typename Fn> void forEachAlive(Fn&& fn) {
        for (uint32_t i = 0; i < m_objects.size(); i++) {
            if (m_alive[i]) {
                fn(m_objects[i]);
                m_alive[i] = false;
            }
        }
    }

  private:
    std::vector<T>       m_objects;
    std::vector<bool>    m_alive;
    std::queue<uint32_t> m_freeSlots;
};

} // namespace oz::gfx::vk
