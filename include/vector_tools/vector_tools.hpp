#ifndef VECTOR_TOOLS_HEADER
#define VECTOR_TOOLS_HEADER

#include <cstddef>
#include <memory>
#include <type_traits>

namespace vector_tools
{
	namespace detail
	{
		template< class... >
		using void_t = void;

		template<typename Alloc, typename = void_t<> >
		struct uses_default_destroy : std::false_type
		{};

		template<typename Alloc >
		struct uses_default_destroy<Alloc,
			void_t<decltype(std::declval<Alloc>()->destroy(std::declval<typename Alloc::value_type*>()))>> : std::true_type
		{};
	}

	///Destroys all elements in the given range, in reverse order, by calling the destructor.
	///Empty ranges are fine, as are NULL ranges.
	///We could add a SFINAE overload for trivially destructible types that does nothing.
	///But I trust the compiler to do its job.
	template<typename T>
	void destructor_destroy_range(T *begin, T *end) noexcept
	{
		while (end != begin)
		{
			--end;
			end->~T();
		}
	}

	///Destroys all elements in the given range, in reverse order, by calling the destructor.
	///Only allowed if `Alloc::destroy` doesn't exist.
	///Empty ranges are fine, as are NULL ranges.
	template<typename T, typename Alloc>
	std::enable_if_t<detail::uses_default_destroy<Alloc>::value> destroy_range(T *begin, T *end, Alloc &) noexcept
	{
		destructor_destroy_range(begin, end);
	}

	///Destroys all elements in the given range, in reverse order, using the allocator's destroy call.
	///Empty ranges are fine, as are NULL ranges.
	template<typename T, typename Alloc>
	std::enable_if_t<!detail::uses_default_destroy<Alloc>::value> destroy_range(T *begin, T *end, Alloc &alloc) noexcept
	{
		while (end != begin)
		{
			--end;
			std::allocator_traits<Alloc>::destroy(alloc, end);
		}
	}

	///Initializes `count` elements in an array, starting at `first`.
	///Performs initialization via placement new, forwarding the same `args` to each object.
	///Returns a pointer to the one-past-the-end element of the array.
	///If any element fails to be constructed, it will destroy all previously constructed elements.
	///Destruction will happen via a direct call to the destructor.
	template<typename T, typename ...Args>
	T *placement_emplace_construct_count(T *first, std::size_t count, Args &&...args)
	{
		auto curr = first;
		try
		{
			for (; curr - first < count; ++curr)
				new(curr) T(std::forward<Args>(args)...);
		}
		catch (...)
		{
			//curr was never successfully constructed.
			destructor_destroy_range(first, curr);
			throw;
		}
		return curr;
	}

	///Initializes `count` elements in an array, starting at `first`.
	///Performs value initialization using `allocator_traits<Alloc>::construct`,
	///fowarding the same `args` to each call.
	///Returns a pointer to the one-past-the-end element of the array.
	///If any element fails to be constructed, it will destroy all previously constructed elements.
	///Destruction happens initialization using `destroy_range`.
	template<typename T, typename Alloc, typename ...Args>
	T *emplace_construct_count(T *first, std::size_t count, Alloc &alloc, Args &&...args)
	{
		auto curr = first;
		try
		{
			for (; std::size_t(curr - first) < count; ++curr)
				std::allocator_traits<Alloc>::construct(alloc, curr, std::forward<Args>(args)...);
		}
		catch (...)
		{
			//curr was never successfully constructed.
			destroy_range(first, curr, alloc);
			throw;
		}
		return curr;
	}

	///Initializes the elements in `output`,
	///by copy-constructing from the `input/end` range.
	///The copy will be performed by using `allocator_traits<Alloc>::construct`.
	///Returns a pointer to the one-past-the-end element of the new array.
	///If a copy throws, it will destroy all previously constructed elements.
	///Destruction happens initialization using `destroy_range`.
	template<typename T, typename Alloc>
	T *copy_insert_range(T *output, Alloc &alloc, const T *input, const T *end)
	{
		auto curr = output;
		try
		{
			for (; input != end; ++curr, ++input)
				std::allocator_traits<Alloc>::construct(alloc, curr, *input);
		}
		catch (...)
		{
			//curr itself was never successfully constructed.
			destroy_range(output, curr, alloc);
			throw;
		}
		return curr;
	}

	///Initializes the elements in `output`,
	///by safe-moving values from the `input/end` range.
	///The move will be performed by using `allocator_traits<Alloc>::construct`
	///by using `std::move_if_noexcept`.
	///Returns a pointer to the one-past-the-end element of the new array.
	///If a copy/move throws, it will destroy all previously constructed elements.
	///Destruction happens initialization using `destroy_range`.
	///But if it was a move that caused this... good luck on getting your data back ;)
	template<typename T, typename Alloc>
	T *safemove_insert_range(T *output, Alloc &alloc, T *input, T *end)
	{
		auto curr = output;
		try
		{
			for (; input != end; ++curr, ++input)
				std::allocator_traits<Alloc>::construct(alloc, curr, std::move_if_noexcept(*input));
		}
		catch (...)
		{
			//curr itself was never successfully constructed.
			destroy_range(output, curr, alloc);
			throw;
		}
		return curr;
	}

	///Takes the (possibly empty) range `input/end` and assigns all of them to the range
	///beginning at `target` and ending at `target + (end - input)`.
	///`target` must be before `input` in the array, but the target range may overlap.
	///The `target` range must consist of objects of type `T`.
	///The shift is done via move_if_noexcept assignment.
	///If an exception is thrown, no attempt is made to try to recover,
	///as we may have overwritten data.
	///Returns `target + (end - input)`.
	template<typename T>
	T *safemove_assign_shift_left(T *target, T *input, T *end)
	{
		if (target == input)
			return target;

		for (; input != end; ++target, ++input)
			*target = std::move_if_noexcept(*input);

		return target;
	}

	///A partition represents a set of ranges of elements. The first range has been
	///moved from, and the second range are unconstructed memory.
	template<typename T>
	struct partition
	{
		//The start of the moved-from range.
		T *first;
		//The end of the moved-from range, and the start of the unconstructed range.
		T *last;
		//The end of the unconstructed range. May equal `last` if none of the elements are unconstructed.
		T *end;
	};

	///Takes a range of `pos/last`. It will perform safe-move insertion/assignment
	///to the range from `last` to `back`.
	///The range `pos/last` consists of constructed `T`s. Any movement into them will
	///use safe-move assignment.
	///The range `last/back` are unconstructed storage. Any movement into them will
	///use safe-move insertion through `alloc`.
	///The values are always moved in reverse order.
	///On exceptions, only previously unconstructed elements are deleted.
	///Returns the range of the partitioned elements.
	template<typename T, typename Alloc>
	partition<T> safemove_partition_right(T *pos, T *last, Alloc &alloc, T *back)
	{
		auto src = last - 1;
		auto new_dst = back - 1;
		try
		{
			//Move-insert in reverse order, until either we run out of elements to move
			//Or we're about to start copying over previously moved-from elements.
			while (src != pos && new_dst != last)
			{
				--src; --new_dst;
				std::allocator_traits<Alloc>::construct(alloc, new_dst, std::move_if_noexcept(*src));
			}

			//Move-assign in reverse order until we run out of elements to move.
			auto overwrite_dst = new_dst;
			while (src != pos)
			{
				--src; --overwrite_dst;
				*overwrite_dst = std::move_if_noexcept(*src);
			}

			return { pos,
				last < overwrite_dst ? last : overwrite_dst,
				overwrite_dst };
		}
		catch (...)
		{
			//new_dst itself was never successfully constructed.
			++new_dst;
			destroy_range(new_dst, back, alloc);
			throw;
		}
	}
}

#endif //VECTOR_TOOLS_HEADER
