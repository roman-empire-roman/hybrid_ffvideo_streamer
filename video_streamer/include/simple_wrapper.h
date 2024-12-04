#ifndef SIMPLE_WRAPPER_H
#define SIMPLE_WRAPPER_H

#include <functional>

namespace SimpleWrapperSpace {
    using Constructor = std::function< void(void) >;
    using Destructor = std::function< void(void) >;

    class SimpleWrapper {
    public:
        explicit SimpleWrapper(const Constructor& constructor, const Destructor& destructor);
        ~SimpleWrapper();

        SimpleWrapper(const SimpleWrapper& other) = delete;
        SimpleWrapper& operator=(const SimpleWrapper& other) = delete;
        SimpleWrapper(SimpleWrapper&& other) = delete;
        SimpleWrapper& operator=(SimpleWrapper&& other) = delete;

    private:
        Destructor m_destructor = nullptr;
    };
}

#endif /* SIMPLE_WRAPPER_H */
