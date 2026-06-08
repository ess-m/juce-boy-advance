//
// UserSettings.h
//

#pragma once

#include <juce_data_structures/juce_data_structures.h>

class UserSettings {
public:
    static juce::PropertiesFile& file() {
        static juce::PropertiesFile f { [] {
            juce::PropertiesFile::Options o;
            o.applicationName = "FMS";
            o.filenameSuffix = ".settings";
            o.folderName = "FMS";
            o.osxLibrarySubFolder = "Application Support";
            o.commonToAllUsers = false;
            return o;
        }() };
        return f;
    }
};
