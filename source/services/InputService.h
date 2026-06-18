//
// InputService.h
//

#pragma once

#include <array>
#include <atomic>

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "UserSettings.h"

#include <SDL.h>

#include <nba/core.hpp>

class InputService : private juce::Timer {
private:
    static constexpr size_t kKeyCount = static_cast<size_t>(nba::Key::Count);

    std::array<int, kKeyCount> keyboardMap_{};
    std::array<int, kKeyCount> gamepadMap_{};

    std::array<std::atomic<bool>, kKeyCount> keyboard_{};
    std::array<std::atomic<bool>, kKeyCount> gamepad_{};

    SDL_GameController* controller_ = nullptr;
    bool sdlInitialized_ = false;
    bool autoSelectController_ = true;

    juce::String persistedControllerName_;
    std::function<void(int sdlButton)> gamepadCaptureCallback_;

    juce::SharedResourcePointer<UserSettings> settings_;

    void saveSettings() {
        auto& f = settings_->file();
        for (uint8_t i = 0; i < kKeyCount; ++i) {
            f.setValue("kb_" + juce::String(static_cast<int>(i)), keyboardMap_[i]);
            f.setValue("gp_" + juce::String(static_cast<int>(i)), gamepadMap_[i]);
        }
        f.setValue("auto_controller", autoSelectController_);
        f.setValue("controller_name", getCurrentControllerName());
        f.saveIfNeeded();
    }

    void loadSettings() {
        auto& f = settings_->file();
        for (uint8_t i = 0; i < kKeyCount; ++i) {
            const auto kKey = "kb_" + juce::String(static_cast<int>(i));
            const auto gKey = "gp_" + juce::String(static_cast<int>(i));
            if (f.containsKey(kKey)) keyboardMap_[i] = f.getIntValue(kKey, keyboardMap_[i]);
            if (f.containsKey(gKey)) gamepadMap_[i]  = f.getIntValue(gKey, gamepadMap_[i]);
        }
        autoSelectController_ = f.getBoolValue("auto_controller", true);
        persistedControllerName_ = f.getValue("controller_name", "");
    }

    bool openControllerByName(const juce::String& wanted) {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (!SDL_IsGameController(i)) continue;
            const char* n = SDL_GameControllerNameForIndex(i);
            if (n != nullptr && juce::String(n) == wanted) {
                controller_ = SDL_GameControllerOpen(i);
                if (controller_) return true;
            }
        }
        return false;
    }

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
            if (autoSelectController_) openFirstController();
            if (!controller_) {
                clearGamepadState();
                return;
            }
        }

        if (gamepadCaptureCallback_) {
            for (int btn = 0; btn < SDL_CONTROLLER_BUTTON_MAX; ++btn) {
                const auto sdlBtn = static_cast<SDL_GameControllerButton>(btn);
                if (SDL_GameControllerGetButton(controller_, sdlBtn) != 0) {
                    auto cb = std::move(gamepadCaptureCallback_);
                    gamepadCaptureCallback_ = nullptr;
                    clearGamepadState();
                    cb(btn);
                    return;
                }
            }
            return;
        }

        for (size_t i = 0; i < kKeyCount; ++i) {
            const int btn = gamepadMap_[i];
            const bool down = btn >= 0
                && SDL_GameControllerGetButton(
                        controller_, static_cast<SDL_GameControllerButton>(btn)) != 0;
            gamepad_[i].store(down, std::memory_order_relaxed);
        }
    }

public:
    InputService() {
        keyboardMap_.fill(-1);
        keyboardMap_[static_cast<size_t>(nba::Key::A)]      = 's';
        keyboardMap_[static_cast<size_t>(nba::Key::B)]      = 'a';
        keyboardMap_[static_cast<size_t>(nba::Key::Start)]  = 'u';
        keyboardMap_[static_cast<size_t>(nba::Key::Select)] = 'o';
        keyboardMap_[static_cast<size_t>(nba::Key::Up)]     = 'i';
        keyboardMap_[static_cast<size_t>(nba::Key::Down)]   = 'k';
        keyboardMap_[static_cast<size_t>(nba::Key::Left)]   = 'j';
        keyboardMap_[static_cast<size_t>(nba::Key::Right)]  = 'l';
        keyboardMap_[static_cast<size_t>(nba::Key::L)]      = 'q';
        keyboardMap_[static_cast<size_t>(nba::Key::R)]      = 'e';

        gamepadMap_.fill(-1);
        gamepadMap_[static_cast<size_t>(nba::Key::A)]      = SDL_CONTROLLER_BUTTON_B;
        gamepadMap_[static_cast<size_t>(nba::Key::B)]      = SDL_CONTROLLER_BUTTON_A;
        gamepadMap_[static_cast<size_t>(nba::Key::Start)]  = SDL_CONTROLLER_BUTTON_START;
        gamepadMap_[static_cast<size_t>(nba::Key::Select)] = SDL_CONTROLLER_BUTTON_BACK;
        gamepadMap_[static_cast<size_t>(nba::Key::Up)]     = SDL_CONTROLLER_BUTTON_DPAD_UP;
        gamepadMap_[static_cast<size_t>(nba::Key::Down)]   = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
        gamepadMap_[static_cast<size_t>(nba::Key::Left)]   = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
        gamepadMap_[static_cast<size_t>(nba::Key::Right)]  = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
        gamepadMap_[static_cast<size_t>(nba::Key::L)]      = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
        gamepadMap_[static_cast<size_t>(nba::Key::R)]      = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;

        // defaults
        loadSettings();

        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0) {
            sdlInitialized_ = true;

            // load saved settings
            if (!autoSelectController_) {
                if (persistedControllerName_.isNotEmpty()
                    && persistedControllerName_ != "None") {
                    openControllerByName(persistedControllerName_);
                }
            } else {
                openFirstController();
            }

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
        for (size_t i = 0; i < kKeyCount; ++i) {
            const int code = keyboardMap_[i];
            const bool down = code >= 0 && juce::KeyPress::isKeyCurrentlyDown(code);
            keyboard_[i].store(down, std::memory_order_relaxed);
        }
    }

    void setKeyboardMapping(nba::Key gbaKey, int juceKeyCode) {
        keyboardMap_[static_cast<size_t>(gbaKey)] = juceKeyCode;
        saveSettings();
    }

    int getKeyboardMapping(nba::Key gbaKey) const {
        return keyboardMap_[static_cast<size_t>(gbaKey)];
    }

    void setGamepadMapping(nba::Key gbaKey, int sdlButton) {
        gamepadMap_[static_cast<size_t>(gbaKey)] = sdlButton;
        saveSettings();
    }

    int getGamepadMapping(nba::Key gbaKey) const {
        return gamepadMap_[static_cast<size_t>(gbaKey)];
    }

    void beginGamepadCapture(std::function<void(int sdlButton)> onCapture) {
        gamepadCaptureCallback_ = std::move(onCapture);
    }

    void cancelGamepadCapture() {
        gamepadCaptureCallback_ = nullptr;
    }

    std::vector<std::pair<int, juce::String>> enumerateControllers() {
        std::vector<std::pair<int, juce::String>> result;
        if (!sdlInitialized_) return result;

        SDL_GameControllerUpdate();
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (!SDL_IsGameController(i)) continue;
            const char* name = SDL_GameControllerNameForIndex(i);
            result.emplace_back(i, name != nullptr ? juce::String(name) : juce::String("Unknown"));
        }
        return result;
    }

    void selectController(int joystickIndex) {
        autoSelectController_ = false;

        if (controller_) {
            SDL_GameControllerClose(controller_);
            controller_ = nullptr;
            clearGamepadState();
        }

        if (joystickIndex >= 0 && SDL_IsGameController(joystickIndex)) {
            controller_ = SDL_GameControllerOpen(joystickIndex);
        }

        saveSettings();
    }

    juce::String getCurrentControllerName() const {
        if (controller_ == nullptr) return "None";
        const char* name = SDL_GameControllerName(controller_);
        return name != nullptr ? juce::String(name) : juce::String("Unknown");
    }

    int getCurrentControllerJoystickId() const {
        if (controller_ == nullptr) return -1;
        auto* j = SDL_GameControllerGetJoystick(controller_);
        return j != nullptr ? SDL_JoystickInstanceID(j) : -1;
    }

    static juce::String gamepadButtonName(int sdlButton) {
        switch (sdlButton) {
            case SDL_CONTROLLER_BUTTON_A: return "A";
            case SDL_CONTROLLER_BUTTON_B: return "B";
            case SDL_CONTROLLER_BUTTON_X: return "X";
            case SDL_CONTROLLER_BUTTON_Y: return "Y";
            case SDL_CONTROLLER_BUTTON_BACK: return "Back";
            case SDL_CONTROLLER_BUTTON_GUIDE: return "Guide";
            case SDL_CONTROLLER_BUTTON_START: return "Start";
            case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "L Stick";
            case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "R Stick";
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "LB";
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "RB";
            case SDL_CONTROLLER_BUTTON_DPAD_UP: return "Up";
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "Down";
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "Left";
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "Right";
            default: return "?";
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
