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
        sampleRateSelector_.setTextProvider([this] {
            const int sr = static_cast<int>(getCurrentSampleRate());
            return sr > 0 ? juce::String(sr) : juce::String("—");
        });

        sampleRateSelector_.setOnClick([this] { showSampleRateMenu(); });
        sampleRateSelector_.setJustification(juce::Justification::right);
        sampleRateSelector_.place(280, 80, 200, 18);
        addAndMakeVisible(sampleRateSelector_);

        bufferSizeSelector_.setTextProvider([this] {
            const int bs = getCurrentBufferSize();
            return bs > 0 ? juce::String(bs) : juce::String("—");
        });

        bufferSizeSelector_.setOnClick([this] { showBufferSizeMenu(); });
        bufferSizeSelector_.setJustification(juce::Justification::right);
        bufferSizeSelector_.place(280, 105, 200, 18);
        addAndMakeVisible(bufferSizeSelector_);

        deviceSelector_.setTextProvider([this] {
            return getCurrentDeviceName().upToFirstOccurrenceOf(" ", false, false);
        });

        deviceSelector_.setOnClick([this] { showDeviceMenu(); });
        deviceSelector_.setJustification(juce::Justification::right);
        deviceSelector_.place(280, 130, 200, 18);
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
        sampleRateSelector_.setThemeProvider(themeProvider_);
        bufferSizeSelector_.setThemeProvider(themeProvider_);
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

    MenuLabel sampleRateSelector_;
    MenuLabel bufferSizeSelector_;
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

    template <typename Mutator>
    void applySetup(Mutator&& mutator) {
        auto* dm = getDeviceManager();
        if (dm == nullptr) return;
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        dm->getAudioDeviceSetup(setup);
        mutator(setup);
        dm->setAudioDeviceSetup(setup, true);
    }

    void showSampleRateMenu() {
        auto* d = getDevice();
        if (d == nullptr) return;

        juce::PopupMenu menu;
        menu.setLookAndFeel(&popupLook_);

        for (double sr : d->getAvailableSampleRates()) {
            menu.addItem(juce::String(static_cast<int>(sr)), true, false,
                [this, sr] {
                    applySetup([sr](auto& s) { s.sampleRate = sr; });
                    sampleRateSelector_.repaint();
                }
            );
        }

        menu.showMenuAsync(
            juce::PopupMenu::Options()
                .withParentComponent(this)
                .withMousePosition());
    }

    void showBufferSizeMenu() {
        auto* d = getDevice();
        if (d == nullptr) return;

        const auto available = d->getAvailableBufferSizes();

        juce::PopupMenu menu;
        menu.setLookAndFeel(&popupLook_);

        for (int bs : { 32, 64, 128, 256, 512, 1024 }) {
            if (!available.contains(bs)) continue;
            menu.addItem(juce::String(bs), true, false,
                [this, bs] {
                    applySetup([bs](auto& s) { s.bufferSize = bs; });
                    bufferSizeSelector_.repaint();
                });
        }

        menu.showMenuAsync(
            juce::PopupMenu::Options()
                .withParentComponent(this)
                .withMousePosition());
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
            menu.addItem(name, true, false,
                [this, name] {
                    applySetup([&name](auto& s) { s.outputDeviceName = name; });
                    deviceSelector_.repaint();
                });
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

        g.drawText("Sample rate",   0, 80,  200, 18, juce::Justification::left);
        g.drawText("Buffer size",   0, 105, 200, 18, juce::Justification::left);
        g.drawText("Output device", 0, 130, 200, 18, juce::Justification::left);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioConfigOverlay)
};
