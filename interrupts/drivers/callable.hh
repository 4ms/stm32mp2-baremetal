#pragma once
#pragma once
#include <cstdint>
#include <new> //for placement new
#include <type_traits>
#include <utility>

// Callback is a callable object that takes no parameters,
// and has a maximum of 8 Bytes of captures (on ARM 32-bit system)
// Note: [this] takes 4 Bytes (on ARM 32-bit system), so it's quite useful
// Improvements, possibly: remove m_destroy and make m_data sizeof(void*)
// this would reduce the size (on ARM 32-bit) from 16 Bytes to 8 Bytes
// But it would leak memory if Callable was on the heap
// (which shouldn't happen, since the point of this is to avoid the heap,
// but it could...)

// Function<T> is a callback that takes parameters and returns something
template<unsigned buffer_size, typename Signature>
struct FunctionSized {};

template<unsigned buffer_size, typename Ret, typename... Args>
class FunctionSized<buffer_size, Ret(Args...)> {
public:
	FunctionSized() = default;

	template<typename Callable>
		requires(sizeof(Callable) <= buffer_size && std::is_invocable_v<Callable, Args...>)
	FunctionSized(Callable callable)
		: m_callback{invoke<Callable>}
		, m_destroy{destroy<Callable>}
	{
		new (&m_data[0]) Callable(callable);
	}

	~FunctionSized()
	{
		if (m_destroy)
			m_destroy(&m_data[0]);
	}

	Ret call(Args... args)
	{
		return m_callback(&m_data[0], std::forward<Args>(args)...);
	}

	Ret operator()(Args... args)
	{
		return call(std::forward<Args>(args)...);
	}

	operator bool() const
	{
		return m_callback;
	}

private:
	template<typename Callable>
	static Ret invoke(void *object, Args... args)
	{
		Callable &callable = *reinterpret_cast<Callable *>(object);
		if constexpr (std::is_same_v<void, Ret>)
			callable(std::forward<Args>(args)...);
		else
			return callable(std::forward<Args>(args)...);
	}

	template<typename Callable>
	static void destroy(void *object)
	{
		Callable &callable = *reinterpret_cast<Callable *>(object);
		callable.~Callable();
	}

private:
	using Callback = Ret (*)(void *, Args...);
	Callback m_callback{};

	using Deleter = void (*)(void *);
	Deleter m_destroy{};

	alignas(uint64_t) uint8_t m_data[buffer_size];
};

template<typename Signature>
using Function = FunctionSized<sizeof(void *) * 2, Signature>;

template<unsigned size = sizeof(void *) * 2>
struct CallbackSized : FunctionSized<size, void(void)> {
	CallbackSized() = default;

	template<typename Callable>
	CallbackSized(Callable callable)
		: FunctionSized<size, void(void)>{callable}
	{}
};
// Note: we could just define CallbackSized like the following, but
// that breaks the API for functions using CallbackSized in their signature
// template<unsigned size = sizeof(void *) * 2>
// using CallbackSized = FunctionSized<size, void(void)>;

using Callback = CallbackSized<2 * sizeof(void *)>;
