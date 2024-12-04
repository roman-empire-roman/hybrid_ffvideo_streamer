#include "timeout_checker.h"

#include <iostream>

#include "common_functions.h"

namespace {
    constexpr std::int64_t g_timeout = 50000; // timeout in microseconds

    enum OperationState {
        CONTINUE_EXECUTION = 0,
        READY_TO_INTERRUPT,
        ERROR
    };

    using CheckerRawPtr = TimeoutChecker::CheckerRawPtr;
    using CheckerWeakPtr = TimeoutChecker::CheckerWeakPtr;
}

bool TimeoutChecker::setup() {
    m_isTimeoutReached = false;
    m_beginTime = 0;

    CheckerWeakPtr checkerWeakPtr;
    if (TimeoutChecker::getCheckerWeakPtr(this, checkerWeakPtr)) {
		auto checkerSharedPtr = checkerWeakPtr.lock();
		if (checkerSharedPtr) {
			std::cout << "{TimeoutChecker::setup}; weak pointer to timeout checker is already set" << std::endl;
			return true;
		}
	}

	try {
		checkerWeakPtr = this->weak_from_this();
	} catch (const std::bad_weak_ptr& exception) {
        std::cerr << "{TimeoutChecker::setup}; "
            "exception 'std::bad_weak_ptr' was successfully caught while "
            "getting weak pointer to timeout checker; "
            "exception description: '" << exception.what() << "'" << std::endl;
		return false;
	} catch (...) {
        std::cerr << "{TimeoutChecker::setup}; "
            "unknown exception was caught while "
            "getting weak pointer to timeout checker" << std::endl;
		return false;
	}
	{
		auto checkerSharedPtr = checkerWeakPtr.lock();
		if (nullptr == checkerSharedPtr) {
			std::cerr << "{TimeoutChecker::setup}; shared pointer to timeout checker is NULL" << std::endl;
			return false;
		}
	}
    if (!TimeoutChecker::setCheckerWeakPtr(this, checkerWeakPtr)) {
        return false;
    }
    std::cout << "{TimeoutChecker::setup}; weak pointer to timeout checker has been successfully set" << std::endl;
    return true;
}

auto TimeoutChecker::onReadyToCheckTimeout() {
    if (0 == m_beginTime) {
        return OperationState::CONTINUE_EXECUTION;
    }
    if (m_isTimeoutReached) {
        std::cout << "{TimeoutChecker::onReadyToCheckTimeout}; timeout '" << g_timeout << " microseconds' is already reached" << std::endl;
        return OperationState::READY_TO_INTERRUPT;
    }
    auto curTime = CommonFunctions::getCurTimeSinceEpoch();
    auto diffTime = CommonFunctions::getDiffTime(m_beginTime, curTime);
    if (!diffTime.has_value()) {
        return OperationState::ERROR;
    }

    if (diffTime.value() >= g_timeout) {
        m_isTimeoutReached = true;
        std::cout << "{TimeoutChecker::onReadyToCheckTimeout}; timeout '" << g_timeout << " microseconds' reached; "
            "elapsed time: '" << diffTime.value() << " microseconds'" << std::endl;
        return OperationState::READY_TO_INTERRUPT;
    } else {
        std::cout << "{TimeoutChecker::onReadyToCheckTimeout}; timeout '" << g_timeout << " microseconds' is NOT reached; "
            "elapsed time: '" << diffTime.value() << " microseconds'" << std::endl;
    }
    return OperationState::CONTINUE_EXECUTION;
}

int TimeoutChecker::onProxyReadyToCheckTimeout(void* checkerPtr) {
    if (nullptr == checkerPtr) {
        std::cerr << "{TimeoutChecker::onProxyReadyToCheckTimeout}; "
            "void pointer to timeout checker is NULL" << std::endl;
        return static_cast<int>(OperationState::ERROR);
    }
    auto checkerRawPtr = static_cast<CheckerRawPtr>(checkerPtr);
    auto it = s_checkerWeakPtrs.find(checkerRawPtr);
    if (s_checkerWeakPtrs.cend() == it) {
        std::cerr << "{TimeoutChecker::onProxyReadyToCheckTimeout}; "
            "raw pointer to timeout checker was NOT found in map" << std::endl;
        return static_cast<int>(OperationState::ERROR);
    }
    auto& checkerWeakPtr = it->second;
    auto checkerSharedPtr = checkerWeakPtr.lock();
    if (nullptr == checkerSharedPtr) {
        std::cerr << "{TimeoutChecker::onProxyReadyToCheckTimeout}; "
            "shared pointer to timeout checker is NULL" << std::endl;
        return static_cast<int>(OperationState::ERROR);
    }
    return static_cast<int>(
        checkerSharedPtr->onReadyToCheckTimeout()
    );
}

void TimeoutChecker::setBeginTime() {
    m_isTimeoutReached = false;
    m_beginTime = CommonFunctions::getCurTimeSinceEpoch();
}

void TimeoutChecker::resetBeginTime() {
    m_beginTime = 0;
}

bool TimeoutChecker::getCheckerWeakPtr(
    CheckerRawPtr checkerRawPtr, CheckerWeakPtr& checkerWeakPtr
) {
    if (nullptr == checkerRawPtr) {
        return false;
    }
    auto it = s_checkerWeakPtrs.find(checkerRawPtr);
    if (s_checkerWeakPtrs.cend() == it) {
        return false;
    }
    checkerWeakPtr = it->second;
    return true;
}

bool TimeoutChecker::setCheckerWeakPtr(
    CheckerRawPtr checkerRawPtr, const CheckerWeakPtr& checkerWeakPtr
) {
    if (nullptr == checkerRawPtr) {
        std::cerr << "{TimeoutChecker::setCheckerWeakPtr}; "
            "raw pointer to timeout checker is NULL" << std::endl;
        return false;
    }
    auto it = s_checkerWeakPtrs.find(checkerRawPtr);
    if (s_checkerWeakPtrs.cend() != it) {
        std::cerr << "{TimeoutChecker::setCheckerWeakPtr}; "
            "weak pointer to timeout checker was already inserted to map" << std::endl;
        return false;
    }
    s_checkerWeakPtrs.insert({checkerRawPtr, checkerWeakPtr});
    return true;
}
