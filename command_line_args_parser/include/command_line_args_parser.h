#ifndef COMMAND_LINE_ARGS_PARSER_H
#define COMMAND_LINE_ARGS_PARSER_H

#include <boost/noncopyable.hpp>
#include <boost/python.hpp>
#include <string>

class CommandLineArgsParser {
public:
    CommandLineArgsParser() = default;
    CommandLineArgsParser(const CommandLineArgsParser& other) = delete;
    CommandLineArgsParser& operator=(const CommandLineArgsParser& other) = delete;
    ~CommandLineArgsParser() = default;
    CommandLineArgsParser(CommandLineArgsParser&& other) = delete;
    CommandLineArgsParser& operator=(CommandLineArgsParser&& other) = delete;

    bool parsePythonArgs(boost::python::list args);
    std::string getConfigFileName() const { return m_configFileName; }

private:
    bool parse(int argc, char* argv[]);

private:
    std::string m_configFileName;
};

BOOST_PYTHON_MODULE(command_line_args_parser) {
    boost::python::class_<
        CommandLineArgsParser, boost::noncopyable
    >(
        "CommandLineArgsParser", boost::python::init<>()
    )
        .def(
            "parse", &CommandLineArgsParser::parsePythonArgs,
            boost::python::arg("args")
        )
        .def(
            "getConfigFileName",
            boost::python::make_function(
                &CommandLineArgsParser::getConfigFileName,
                boost::python::return_value_policy<
                    boost::python::return_by_value
                >()
            )
        )
    ;
}

#endif // COMMAND_LINE_ARGS_PARSER_H
