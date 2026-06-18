//
// UserSettings.h
//

#pragma once

#include <juce_data_structures/juce_data_structures.h>

class UserSettings {
public:
    UserSettings() {
        juce::PropertiesFile::Options o;
        o.applicationName = "FMS";
        o.filenameSuffix = ".settings";
        o.folderName = "FMS";
        o.osxLibrarySubFolder = "Application Support";
        o.commonToAllUsers = false;
        properties_.setStorageParameters(o);
    }

    ~UserSettings() {
        properties_.saveIfNeeded();
    }

    juce::PropertiesFile& file() {
        return *properties_.getUserSettings();
    }

private:
    juce::ApplicationProperties properties_;
};
