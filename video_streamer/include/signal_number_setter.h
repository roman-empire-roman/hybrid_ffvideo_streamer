#ifndef SIGNAL_NUMBER_SETTER_H
#define SIGNAL_NUMBER_SETTER_H

#include <csignal>

class SignalNumberSetter {
public:
    static SignalNumberSetter& getInstance() {
        static SignalNumberSetter setter;
        return setter;
    }

    bool isSet() const { return (SIGINT == m_signalNumber); }

private:
    SignalNumberSetter();
    SignalNumberSetter(const SignalNumberSetter& other) = delete;
    SignalNumberSetter& operator=(const SignalNumberSetter& other) = delete;
    ~SignalNumberSetter();
    SignalNumberSetter(SignalNumberSetter&& other) = delete;
    SignalNumberSetter& operator=(SignalNumberSetter&& other) = delete;

    static void setSignalNumber(int signalNumber);

private:
    volatile std::sig_atomic_t m_signalNumber = 0;
};

#endif /* SIGNAL_NUMBER_SETTER_H */
