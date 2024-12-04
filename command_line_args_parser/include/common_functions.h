#ifndef COMMON_FUNCTIONS_H
#define COMMON_FUNCTIONS_H

#include <string>

namespace CommonFunctions {
    bool fileExists(const std::string& fileName);
    bool isRegularFile(const std::string& fileName);

    void printDiagnosticInfo();
}

#endif /* COMMON_FUNCTIONS_H */
