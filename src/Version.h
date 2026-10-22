#pragma once

inline constexpr const char* APP_VERSION = "1.0.0";
// Where "Check for updates" looks. Leave empty until the GitHub page exists, then use
// "https://api.github.com/repos/YOUR-GITHUB-NAME/YOUR-REPOSITORY/releases/latest".
inline constexpr const char* APP_UPDATE_API =
    "https://api.github.com/repos/chrisck5555-dev/WeatherTracker/releases/latest";
inline constexpr const char* APP_AUTHOR = "Christopher Karjala";   // shown in Settings > About