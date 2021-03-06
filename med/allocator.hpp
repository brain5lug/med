/**
@file
simplest allocator in fixed buffer

@copyright Denis Priyomov 2016-2017
Distributed under the MIT License
(See accompanying file LICENSE or visit https://github.com/cppden/med)
*/

#pragma once

#include <memory>

#include "name.hpp"
#include "exception.hpp"

namespace med {

/**
 * Aligns pointer for the type T incrementing initial address if needed
 * @param p initial possibly unaligned pointer
 */
template <class T>
constexpr void* forward_aligned(void* p) noexcept
{
	const auto uptr = reinterpret_cast<uintptr_t>(p);
	return reinterpret_cast<void*>((uptr + alignof(T) - 1) & -alignof(T));
}

/**
 * Returns pointer to place aligned type T before the given pointer
 * @param p pointer to point after or to the end of T
 */
template <class T>
constexpr void* backward_aligned(void* p) noexcept
{
	const auto uptr = reinterpret_cast<uintptr_t>(p) - sizeof(T);
	return reinterpret_cast<void*>(uptr & -alignof(T));
}

namespace internal {

class base_allocator
{
public:
	/**
	 * Resets to the current allocation buffer
	 */
	void reset() noexcept                  { m_begin = m_start; m_end = m_finish; }

	/**
	 * Resets to new allocation buffer
	 * @param data start of allocation buffer
	 * @param size size of allocation buffer
	 */
	void reset(void* data, std::size_t size) noexcept
	{
		m_start = static_cast<uint8_t*>(data);
		m_finish = m_start + (m_start ? size : 0);
		reset();
	}

	template <typename T, std::size_t SIZE>
	void reset(T (&data)[SIZE]) noexcept   { reset(data, SIZE * sizeof(T)); }

	std::size_t size() const noexcept      { return m_end > m_begin ? std::size_t(m_end - m_begin) : 0; }

	uint8_t* begin()                       { return m_begin; }
	uint8_t* end()                         { return m_end; }

protected:
	void begin(void* p)                    { m_begin = static_cast<uint8_t*>(p); }
	void end(void* p)                      { m_end = static_cast<uint8_t*>(p); }

private:
	uint8_t*    m_begin {nullptr}; //current starting allocation space
	uint8_t*    m_end {nullptr}; //the end of current allocation space
	uint8_t*    m_start {nullptr}; //start of assigned allocation buffer
	uint8_t*    m_finish {nullptr}; //end of assigned allocation buffer
};

} //end: namespace

template <bool FORWARD, class BUFFER>
class allocator;

template <class BUFFER>
class allocator<true, BUFFER> : public internal::base_allocator
{
public:
	/**
	 * Allocates T from the beginning of current free buffer space
	 * @return pointer to instance of T or nullptr/throw when out of space
	 */
	template <class T>
	T* allocate()
	{
		void* p = forward_aligned<T>(this->begin());
		if (this->end() - static_cast<uint8_t*>(p) >= int(sizeof(T)))
		{
			T* result = new (p) T{};
			this->begin(result + 1);
			return result;
		}
		MED_THROW_EXCEPTION(out_of_memory, name<T>(), sizeof(T))
	}
};

template <class BUFFER>
class allocator<false, BUFFER> : public internal::base_allocator
{
public:
	//TODO: anything better than depending on buffer? pointer to end doesn't look better though...
	explicit allocator(BUFFER& buf) noexcept
		: m_buffer{ buf }
	{}

	/**
	 * Allocates T from the back of current free buffer space
	 * @return pointer to instance of T or nullptr/throw when out of space
	 */
	template <class T>
	T* allocate()
	{
		void* p = backward_aligned<T>(this->end());
		if (this->begin() <= p)
		{
			T* result = new (p) T{};
			this->end(p);
			//also need to adjust the space left to the buffer
			m_buffer.end(static_cast<typename BUFFER::pointer>(p));
			return result;
		}
		MED_THROW_EXCEPTION(out_of_memory, name<T>(), sizeof(T))
	}

private:
	BUFFER&     m_buffer;
};


template <class, class Enable = void >
struct is_allocator : std::false_type { };
template <class T>
struct is_allocator<T,
	std::enable_if_t<
		std::is_same_v<int*, decltype(std::declval<T>().template allocate<int>())>
	>
> : std::true_type { };
template <class T>
constexpr bool is_allocator_v = is_allocator<T>::value;

template <class T>
inline auto get_allocator_ptr(T& t) -> std::enable_if_t<is_allocator_v<T>, T*>
{
	return &t;
}
template <class T>
inline auto get_allocator_ptr(T* pt) -> std::enable_if_t<is_allocator_v<T>, T*>
{
	return pt;
}
template <class T>
inline auto get_allocator_ptr(T& t) -> std::add_pointer_t<decltype(t.get_allocator())>
{
	auto& allocator = t.get_allocator();
	static_assert(is_allocator_v<decltype(allocator)>, "IS NOT ALLOCATOR!");
	return &allocator;
}
template <class T>
inline auto get_allocator_ptr(T* pt) -> std::add_pointer_t<decltype(pt->get_allocator())>
{
	auto& allocator = pt->get_allocator();
	static_assert(is_allocator_v<decltype(allocator)>, "IS NOT ALLOCATOR!");
	return &allocator;
}


} //end: namespace med
