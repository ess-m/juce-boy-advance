//
// InputService.h
//

#include <array>
#include <atomic>

#include <nba/core.hpp>

class InputService {
private:
    std::array<std::atomic<bool>, static_cast<size_t>(nba::Key::Count)> keys_{};

public:
    void keyDown(nba::Key key) {
        keys_[static_cast<size_t>(key)].store(true, std::memory_order_relaxed);
    }

    void keyUp(nba::Key key) {
        keys_[static_cast<size_t>(key)].store(false, std::memory_order_relaxed);
    }

    void syncToCore(nba::CoreBase& core) {
        for (uint8_t i = 0; i < static_cast<uint8_t>(nba::Key::Count); ++i) {
            core.SetKeyStatus(static_cast<nba::Key>(i), keys_[i].load(std::memory_order_relaxed));
        }
    }
};