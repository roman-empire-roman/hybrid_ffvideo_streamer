#include "simple_wrapper.h"

using namespace SimpleWrapperSpace;

SimpleWrapper::SimpleWrapper(const Constructor& constructor, const Destructor& destructor) :
    m_destructor{ destructor }
{
    if (constructor) {
        constructor();
    }
}

SimpleWrapper::~SimpleWrapper() {
    if (m_destructor) {
        m_destructor();
    }
}
