//
// InputService.h
//

#pragma once

#include <array>
#include <atomic>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <SDL.h>

#include <nba/core.hpp>

class InputService : private juce::Timer {
private:
    struct KeyboardMapping {
        int juceKeyCode;
        nba::Key gbaKey;
    };

    static inline const KeyboardMapping kKeyboardMappings[] = {
        { 's', nba::Key::A },
        { 'a', nba::Key::B },
        { 'u', nba::Key::Start },
        { 'o', nba::Key::Select },
        { 'i', nba::Key::Up },
        { 'k', nba::Key::Down },
        { 'j', nba::Key::Left },
        { 'l', nba::Key::Right },
        { 'q', nba::Key::L },
        { 'e', nba::Key::R },
    };

    std::array<std::atomic<bool>, static_cast<size_t>(nba::Key::Count)> keyboard_{};
    std::array<std::atomic<bool>, static_cast<size_t>(nba::Key::Count)> gamepad_{};

    SDL_GameController* controller_ = nullptr;
    bool sdlInitialized_ = false;

    void openFirstController() {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (!SDL_IsGameController(i)) continue;

            controller_ = SDL_GameControllerOpen(i);
            if (controller_) {
                DBG("Gamepad connected: " << SDL_GameControllerName(controller_));
                return;
            }
        }
    }

    void clearGamepadState() {
        for (auto& g : gamepad_) g.store(false, std::memory_order_relaxed);
    }

    void timerCallback() override {
        if (!sdlInitialized_) return;

        SDL_GameControllerUpdate();

        if (controller_ && !SDL_GameControllerGetAttached(controller_)) {
            SDL_GameControllerClose(controller_);
            controller_ = nullptr;
        }

        if (!controller_) {
            openFirstController();
            if (!controller_) {
                clearGamepadState();
                return;
            }
        }

        const auto setKey = [this](nba::Key key, SDL_GameControllerButton btn) {
            const bool down = SDL_GameControllerGetButton(controller_, btn) != 0;
            gamepad_[static_cast<size_t>(key)].store(down, std::memory_order_relaxed);
        };

        setKey(nba::Key::A, SDL_CONTROLLER_BUTTON_B);
        setKey(nba::Key::B, SDL_CONTROLLER_BUTTON_A);
        setKey(nba::Key::Start, SDL_CONTROLLER_BUTTON_START);
        setKey(nba::Key::Select, SDL_CONTROLLER_BUTTON_BACK);
        setKey(nba::Key::Up, SDL_CONTROLLER_BUTTON_DPAD_UP);
        setKey(nba::Key::Down, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        setKey(nba::Key::Left, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        setKey(nba::Key::Right, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        setKey(nba::Key::L, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        setKey(nba::Key::R, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    }

public:
    InputService() {
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0) {
            sdlInitialized_ = true;
            openFirstController();
            startTimerHz(60);
        } else {
            DBG("SDL_InitSubSystem(GAMECONTROLLER) failed: " << SDL_GetError());
        }
    }

    ~InputService() override {
        stopTimer();
        if (controller_) SDL_GameControllerClose(controller_);
        if (sdlInitialized_) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    }

    void pollKeyboard() {
        for (const auto& [keyCode, gbaKey] : kKeyboardMappings) {
            const bool down = juce::KeyPress::isKeyCurrentlyDown(keyCode);
            keyboard_[static_cast<size_t>(gbaKey)].store(down, std::memory_order_relaxed);
        }
    }

    void syncToCore(nba::CoreBase& core) {
        for (uint8_t i = 0; i < static_cast<uint8_t>(nba::Key::Count); ++i) {
            const bool down = keyboard_[i].load(std::memory_order_relaxed)
                           || gamepad_[i].load(std::memory_order_relaxed);
            core.SetKeyStatus(static_cast<nba::Key>(i), down);
        }
    }

    bool isGamepadConnected() const { return controller_ != nullptr; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputService)
};
