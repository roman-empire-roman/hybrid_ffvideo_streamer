#ifndef COMMON_FUNCTIONS_H
#define COMMON_FUNCTIONS_H

#include <cstdint>
#include <optional>
#include <string>

namespace CommonFunctions {
    bool fileExists(const std::string& fileName);
    bool isRegularFile(const std::string& fileName);
    bool isCharacterFile(const std::string& fileName);
    bool getFileContents(const std::string& fileName, std::string& fileContents);
    std::int64_t getCurTimeSinceEpoch();
    std::optional<std::int64_t> getDiffTime(std::int64_t beginTime, std::int64_t endTime);

    std::optional<std::string> extractHostNameFromRtmpUrl(const std::string& rtmpUrl);
    bool isHostNameValid(const std::string& hostName);

    bool getPngSize(const std::string& fileName, unsigned int& width, unsigned int& height);
}

#endif /* COMMON_FUNCTIONS_H */
