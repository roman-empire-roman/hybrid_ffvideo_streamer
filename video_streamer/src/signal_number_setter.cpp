#include "signal_number_setter.h"

#include <iostream>

SignalNumberSetter::SignalNumberSetter() {
    if (SIG_ERR == std::signal(SIGINT, &SignalNumberSetter::setSignalNumber)) {
        std::cerr << "{SignalNumberSetter::SignalNumberSetter}; "
            "unable to set signal handler 'SignalNumberSetter::setSignalNumber' for "
            "signal 'SIGINT'" << std::endl;
        return;
    }
    std::cout << "{SignalNumberSetter::SignalNumberSetter}; "
        "signal handler 'SignalNumberSetter::setSignalNumber' has been successfully set for "
        "signal 'SIGINT'" << std::endl;
}

SignalNumberSetter::~SignalNumberSetter() {
    if (SIG_ERR == std::signal(SIGINT, SIG_DFL)) {
        std::cerr << "{SignalNumberSetter::~SignalNumberSetter}; "
            "unable to set default signal handler 'SIG_DFL' for "
            "signal 'SIGINT'" << std::endl;
        return;
    }
    std::cout << "{SignalNumberSetter::~SignalNumberSetter}; "
        "default signal handler 'SIG_DFL' has been successfully set for "
        "signal 'SIGINT'" << std::endl;
}

void SignalNumberSetter::setSignalNumber(int signalNumber) {
    auto& self = SignalNumberSetter::getInstance();
    self.m_signalNumber = signalNumber;
}
