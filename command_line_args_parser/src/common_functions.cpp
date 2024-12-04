#include "common_functions.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <filesystem>
#include <iostream>

bool CommonFunctions::fileExists(const std::string& fileName) {
    if (fileName.empty()) {
        std::cerr << "{CommonFunctions::fileExists}; file name is empty" << std::endl;
        return false;
    }
    if (!std::filesystem::exists(fileName)) {
        std::cerr << "{CommonFunctions::fileExists}; file '" << fileName << "' does NOT exist" << std::endl;
        return false;
    }
    return true;
}

bool CommonFunctions::isRegularFile(const std::string& fileName) {
    if (fileName.empty()) {
        std::cerr << "{CommonFunctions::isRegularFile}; file name is empty" << std::endl;
        return false;
    }
    if (!std::filesystem::is_regular_file(fileName)) {
        std::cerr << "{CommonFunctions::isRegularFile}; file '" << fileName << "' is NOT a regular file" << std::endl;
        return false;
    }
    return true;
}

void CommonFunctions::printDiagnosticInfo() {
    auto diagnosticInfo = boost::current_exception_diagnostic_information();
    boost::algorithm::replace_all(diagnosticInfo, "\n", " ");
    boost::algorithm::trim(diagnosticInfo);
    if (!diagnosticInfo.empty()) {
        std::cerr << "{CommonFunctions::printDiagnosticInfo}; "
            "'" << diagnosticInfo << "'" << std::endl;
    }
}
