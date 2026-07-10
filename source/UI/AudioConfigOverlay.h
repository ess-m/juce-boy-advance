//
// AudioConfigOverlay.h
//

#pragma once

#include <juce_animation/juce_animation.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "services/ThemeColors.h"
#include "Font.h"
#include "BackButton.h"
#include "MenuLabel.h"
#include "PopupLook.h"

class AudioConfigOverlay : public juce::Component {
public:
    using ThemeProvider = std::function<ThemeColors()>;

    AudioConfigOverlay() {
        deviceSelector_.setTextProvider([this] {
            return getCurrentDeviceName().upToFirstOccurrenceOf("(", false, false);
        });

        deviceSelector_.setOnClick([this] { showDeviceMenu(); });
        deviceSelector_.setJustification(juce::Justification::right);
        deviceSelector_.place(280, 45, 200, 18);
        addAndMakeVisible(deviceSelector_);

        backButton_.setButtonCallback([this] { hide(); });
        backButton_.setBounds(0, 0, 18, 18);
        addAndMakeVisible(backButton_);

        setAlpha(0.f);

        updater_.addAnimator(fadeAnimation_);
    }

    ~AudioConfigOverlay() override {
        updater_.removeAnimator(fadeAnimation_);
    }

    void show() {
        if (isVisible() && fadingIn_) return;
        fadingIn_ = true;
        setAlpha(0.f);
        setVisible(true);
        toFront(true);
        fadeAnimation_.start();
    }

    void hide() {
        if (!isVisible()) return;
        if (!fadingIn_ && fadeAnimation_.isComplete()) return;
        fadingIn_ = false;
        fadeAnimation_.start();
    }

    void place(int x, int y) {
        setBounds(x - 240, y - 200, 484, 400);
    }

    void setThemeProvider(ThemeProvider cb) {
        themeProvider_ = cb;
        deviceSelector_.setThemeProvider(themeProvider_);
        backButton_.setThemeProvider(themeProvider_);
        popupLook_.setThemeProvider(themeProvider_);
        repaint();
    }

    ThemeColors getColors() const {
        return themeProvider_ ? themeProvider_() : ThemeColors{};
    }

private:
    ThemeProvider themeProvider_;

    MenuLabel deviceSelector_;

    PopupLook popupLook_;
    BackButton backButton_;

    bool fadingIn_ = false;
    juce::VBlankAnimatorUpdater updater_ { this };
    juce::Animator fadeAnimation_ =
        juce::ValueAnimatorBuilder{}
            .withEasing(juce::Easings::createEaseInOut())
            .withDurationMs(150)
            .withValueChangedCallback([this](auto value) {
                const float p = fadingIn_
                    ? static_cast<float>(value)
                    : 1.f - static_cast<float>(value);
                setAlpha(p);
            })
            .withOnCompleteCallback([this] {
                if (!fadingIn_) setVisible(false);
            })
            .build();

    juce::AudioDeviceManager* getDeviceManager() const {
        auto* holder = juce::StandalonePluginHolder::getInstance();
        return holder != nullptr ? &holder->deviceManager : nullptr;
    }

    juce::AudioIODevice* getDevice() const {
        auto* dm = getDeviceManager();
        return dm != nullptr ? dm->getCurrentAudioDevice() : nullptr;
    }

    double getCurrentSampleRate() const {
        auto* d = getDevice();
        return d != nullptr ? d->getCurrentSampleRate() : 0.0;
    }

    int getCurrentBufferSize() const {
        auto* d = getDevice();
        return d != nullptr ? d->getCurrentBufferSizeSamples() : 0;
    }

    juce::String getCurrentDeviceName() const {
        auto* d = getDevice();
        return d != nullptr ? d->getName() : juce::String("None");
    }

    void setOutputDevice(const juce::String& name) {
        auto* dm = getDeviceManager();
        if (dm == nullptr) return;

        juce::AudioDeviceManager::AudioDeviceSetup previous;
        dm->getAudioDeviceSetup(previous);

        auto setup = previous;
        setup.outputDeviceName = name;
        setup.sampleRate = 0;
        setup.bufferSize = 0;

        if (dm->setAudioDeviceSetup(setup, true).isNotEmpty()) {
            dm->setAudioDeviceSetup(previous, true);
        }
        repaint();
    }

    void showDeviceMenu() {
        auto* dm = getDeviceManager();
        if (dm == nullptr) return;

        auto* type = dm->getCurrentDeviceTypeObject();
        if (type == nullptr) return;
        type->scanForDevices();

        const juce::String currentName = getCurrentDeviceName();

        juce::PopupMenu menu;
        menu.setLookAndFeel(&popupLook_);

        for (const auto& name : type->getDeviceNames(false /* output */)) {
            menu.addItem(name, true, false, [this, name] { setOutputDevice(name); });
        }

        menu.showMenuAsync(
            juce::PopupMenu::Options()
                .withParentComponent(this)
                .withMousePosition());
    }

    void paint(juce::Graphics& g) override {
        g.setColour(getColors().lo);

        g.setFont(UIFont::getInstance().getUIFont().withHeight(21.0f));
        g.drawText("Audio config", 24, 0, 200, 18, juce::Justification::left);

        g.fillRect(0, 25, 480, 1);

        g.drawText("Output device", 0, 45, 200, 18, juce::Justification::left);
        g.drawText("Sample rate",   0, 70, 200, 18, juce::Justification::left);
        g.drawText("Buffer size",   0, 95, 200, 18, juce::Justification::left);

        const auto value = [](int v) { return v > 0 ? juce::String(v) : juce::String("—"); };

        g.drawText(value(static_cast<int>(getCurrentSampleRate())), 280, 70, 200, 18, juce::Justification::right);
        g.drawText(value(getCurrentBufferSize()), 280, 95, 200, 18, juce::Justification::right);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioConfigOverlay)
};
