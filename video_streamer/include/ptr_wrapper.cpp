#include "ptr_wrapper.h"

using namespace PtrWrapperSpace;

template <class T>
PtrWrapper<T>::PtrWrapper(const Creator<T>& creator, const Deleter<T>& deleter) :
    m_deleter{ deleter }, m_ptr{ nullptr }
{
    if (creator && deleter) {
        m_ptr = creator();
    }
}

template <class T>
PtrWrapper<T>::~PtrWrapper() {
    if (m_deleter && m_ptr) {
        m_deleter(&m_ptr);
        m_ptr = nullptr;
    }
    m_deleter = nullptr;
}
