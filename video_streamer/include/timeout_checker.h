#ifndef TIMEOUT_CHECKER_H
#define TIMEOUT_CHECKER_H

#include <cstdint>
#include <memory>
#include <unordered_map>

class TimeoutChecker : public std::enable_shared_from_this<TimeoutChecker> {
public:
    TimeoutChecker() = default;
    TimeoutChecker(const TimeoutChecker& other) = delete;
    TimeoutChecker& operator=(const TimeoutChecker& other) = delete;
    ~TimeoutChecker() = default;
    TimeoutChecker(TimeoutChecker&& other) = delete;
    TimeoutChecker& operator=(TimeoutChecker&& other) = delete;

    using CheckerRawPtr = TimeoutChecker*;
    using CheckerWeakPtr = std::weak_ptr<TimeoutChecker>;

    bool setup();

    static int onProxyReadyToCheckTimeout(void* checkerPtr);

    void setBeginTime();
    void resetBeginTime();

    bool isTimeoutReached() const { return m_isTimeoutReached; }

private:
    auto onReadyToCheckTimeout();
    static bool getCheckerWeakPtr(
        CheckerRawPtr checkerRawPtr, CheckerWeakPtr& checkerWeakPtr
    );
    static bool setCheckerWeakPtr(
        CheckerRawPtr checkerRawPtr, const CheckerWeakPtr& checkerWeakPtr
    );

private:
    static inline std::unordered_map<CheckerRawPtr, CheckerWeakPtr> s_checkerWeakPtrs;

    std::int64_t m_beginTime = 0;
    bool m_isTimeoutReached = false;
};

#endif /* TIMEOUT_CHECKER_H */
