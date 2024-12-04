#include "command_line_args_parser.h"

#include <boost/program_options.hpp>
#include <iostream>
#include <limits>
#include <memory>
#include <pythonrun.h>
#include <stdexcept>
#include <vector>
#include <utility>

#include "common_functions.h"
#include "simple_wrapper.h"

bool CommandLineArgsParser::parsePythonArgs(boost::python::list pythonArgs) {
    using namespace SimpleWrapperSpace;

    boost::python::ssize_t nPythonArgs = 0;
    try {
        nPythonArgs = boost::python::len(pythonArgs);
    } catch (const boost::python::error_already_set& exception) {
        if (Py_IsInitialized()) {
            PyErr_Print();
        }

        std::cerr << "{CommandLineArgsParser::parsePythonArgs}; "
            "exception 'boost::python::error_already_set' was successfully caught while "
            "getting the length of a Python argument list" << std::endl;
        return false;
    } catch (...) {
        std::cerr << "{CommandLineArgsParser::parsePythonArgs}; "
            "unknown exception was caught while "
            "getting the length of a Python argument list" << std::endl;
        CommonFunctions::printDiagnosticInfo();
        return false;
    }
    if (nPythonArgs < 0) {
        std::cerr << "{CommandLineArgsParser::parsePythonArgs}; "
            "length of a Python argument list is less than zero" << std::endl;
        return false;
    }
    if (0 == nPythonArgs) {
        std::cerr << "{CommandLineArgsParser::parsePythonArgs}; "
            "length of a Python argument list is equal to zero" << std::endl;
        return false;
    }
    constexpr auto maxInt = std::numeric_limits<int>::max();
    if (nPythonArgs > static_cast<boost::python::ssize_t>(maxInt)) {
        std::cerr << "{CommandLineArgsParser::parsePythonArgs}; "
            "length of a Python argument list is too long" << std::endl;
        return false;
    }

    char** argumentsAsRawStrings = nullptr;
    std::unique_ptr<SimpleWrapper> argsMemoryManager{ nullptr };
    try {
        auto argsAllocator = [&argumentsAsRawStrings, &nPythonArgs] () {
            argumentsAsRawStrings = new char*[
                1 + static_cast<std::size_t>(nPythonArgs)
            ];
        };
        auto argsDeallocator = [&argumentsAsRawStrings] () {
            delete[] argumentsAsRawStrings;
        };
        argsMemoryManager = std::make_unique<SimpleWrapper>(
            argsAllocator, argsDeallocator
        );
    } catch (const std::bad_alloc& exception) {
        std::cerr << "{CommandLineArgsParser::parsePythonArgs}; "
            "exception 'std::bad_alloc' was successfully caught while "
            "allocating memory for command line arguments; "
            "exception description: '" << exception.what() << "'" << std::endl;
        return false;
    } catch (...) {
        std::cerr << "{CommandLineArgsParser::parsePythonArgs}; "
            "unknown exception was caught while "
            "allocating memory for command line arguments" << std::endl;
        return false;
    }

    std::vector<std::string> argumentsAsStrings;
    try {
        argumentsAsStrings.resize(
            static_cast<std::size_t>(nPythonArgs)
        );
        for (boost::python::ssize_t i = 0; i < nPythonArgs; ++i) {
            const auto& pythonArg = pythonArgs[i];
            auto extractedPythonArg = boost::python::extract<std::string>(pythonArg);
            if (!extractedPythonArg.check()) {
                std::cerr << "{CommandLineArgsParser::parsePythonArgs}; "
                    "unable to extract string argument from Python object" << std::endl;
                return false;
            }
            std::string argAsString(extractedPythonArg());
            if (argAsString.empty()) {
                std::cerr << "{CommandLineArgsParser::parsePythonArgs}; "
                    "string argument is empty" << std::endl;
                return false;
            }
            // std::cout << "{CommandLineArgsParser::parsePythonArgs}; "
            //     "i: '" << static_cast<std::size_t>(i) << "'; "
            //     "number of arguments: '" << static_cast<std::size_t>(nPythonArgs) << "'; "
            //     "string argument: '" << argAsString << "'" << std::endl;

            auto& argFromVector = argumentsAsStrings[ static_cast<std::size_t>(i) ];
            argFromVector = std::move(argAsString);
            argumentsAsRawStrings[ static_cast<std::size_t>(i) ] = const_cast<char*>(
                argFromVector.c_str()
            );
        }
    } catch (const boost::python::error_already_set& exception) {
        if (Py_IsInitialized()) {
            PyErr_Print();
        }

        std::cerr << "{CommandLineArgsParser::parsePythonArgs}; "
            "exception 'boost::python::error_already_set' was successfully caught while "
            "extracting string arguments from Python objects" << std::endl;
        return false;
    } catch (const std::length_error& exception) {
        std::cerr << "{CommandLineArgsParser::parsePythonArgs}; "
            "exception 'std::length_error' was successfully caught while "
            "extracting string arguments from Python objects; "
            "exception description: '" << exception.what() << "'" << std::endl;
        return false;
    } catch (const std::bad_alloc& exception) {
        std::cerr << "{CommandLineArgsParser::parsePythonArgs}; "
            "exception 'std::bad_alloc' was successfully caught while "
            "extracting string arguments from Python objects; "
            "exception description: '" << exception.what() << "'" << std::endl;
        return false;
    } catch (...) {
        std::cerr << "{CommandLineArgsParser::parsePythonArgs}; "
            "unknown exception was caught while "
            "extracting string arguments from Python objects" << std::endl;
        CommonFunctions::printDiagnosticInfo();
        return false;
    }
    argumentsAsRawStrings[ static_cast<std::size_t>(nPythonArgs) ] = nullptr;

    return parse(
        static_cast<int>(nPythonArgs), argumentsAsRawStrings
    );
}

bool CommandLineArgsParser::parse(int argc, char* argv[]) {
    if (!m_configFileName.empty()) {
        std::cerr << "{CommandLineArgsParser::parse}; configuration file name is already set" << std::endl;
        return false;
    }

    try {
        std::string configFileName;
        boost::program_options::options_description description;
        description.add_options()
            (
                "help,h", "Display help message"
            )
            (
                "config,c",
                boost::program_options::value<std::string>(
                    &configFileName
                )->required(), "Path to configuration file"
            );

        boost::program_options::variables_map variables;
        boost::program_options::store(
            boost::program_options::parse_command_line(
                argc, argv, description
            ), variables
        );

        if ((variables.count("help") > 0) && (variables.count("config") > 0)) {
            std::cerr << "{CommandLineArgsParser::parse}; select only one option: '--help' or '--config'" << std::endl;
            return false;
        }
        if (variables.count("help") > 0) {
            std::cout << description;
            return false;
        }
        boost::program_options::notify(variables);

        if (!CommonFunctions::fileExists(configFileName)) {
            return false;
        }
        if (!CommonFunctions::isRegularFile(configFileName)) {
            return false;
        }
        m_configFileName = std::move(configFileName);
    } catch (const boost::program_options::error& exception) {
        std::cerr << "{CommandLineArgsParser::parse}; "
            "exception 'boost::program_options::error' was successfully caught while "
            "parsing command line arguments; "
            "exception description: '" << exception.what() << "'" << std::endl;
        return false;
    } catch (...) {
        std::cerr << "{CommandLineArgsParser::parse}; "
            "unknown exception was caught while "
            "parsing command line arguments" << std::endl;
        CommonFunctions::printDiagnosticInfo();
        return false;
    }
    return true;
}
