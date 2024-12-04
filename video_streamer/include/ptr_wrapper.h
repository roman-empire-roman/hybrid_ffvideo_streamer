#ifndef PTR_WRAPPER_H
#define PTR_WRAPPER_H

#include <functional>

namespace PtrWrapperSpace {
	template <class T>
	using Creator = std::function< T*(void) >;

	template <class T>
	using Deleter = std::function< void(T**) >;

	template <class T>
	class PtrWrapper {
	public:
		explicit PtrWrapper(const Creator<T>& creator, const Deleter<T>& deleter);
		~PtrWrapper();
		PtrWrapper(const PtrWrapper& other) = delete;
		PtrWrapper& operator=(const PtrWrapper& other) = delete;
		PtrWrapper(PtrWrapper&& other) = delete;
		PtrWrapper& operator=(PtrWrapper&& other) = delete;

		const T* get() const { return m_ptr; }
        T** getAddress() { return &m_ptr; }
		const T* operator->() const { return m_ptr; }
        T* operator->() { return m_ptr; }

	private:
		Deleter<T> m_deleter = nullptr;
		T* m_ptr = nullptr;
	};
}

#include "ptr_wrapper.cpp"

#endif // PTR_WRAPPER_H
