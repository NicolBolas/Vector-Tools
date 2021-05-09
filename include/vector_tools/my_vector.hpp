#ifndef MY_VECTOR_TEST_IMPLEMENTATION_HEADER
#define MY_VECTOR_TEST_IMPLEMENTATION_HEADER

#include "vector_tools.hpp"
#include <cstddef>
#include <memory>
#include <iterator>
#include <stdexcept>
#include <limits>
#include <initializer_list>
#include <algorithm>

namespace detail
{
	template<typename Alloc>
	struct allocator_data : public Alloc
	{
		allocator_data() : Alloc() {}

		allocator_data(const Alloc &alloc) : Alloc(alloc) {}
		allocator_data(Alloc &&alloc) : Alloc(std::move(alloc)) {}

		Alloc &get_alloc() { return static_cast<Alloc&>(*this); }
		const Alloc &get_alloc() const { return static_cast<Alloc&>(*this); }
	};
}

template<typename T, typename Alloc = std::allocator<T>>
class my_vector : private detail::allocator_data<Alloc>
{
private:
	T *first_;
	T *last_;
	T *end_;

	using alloc_data = detail::allocator_data<Alloc>;

public:
	using value_type = T;
	using allocator_type = Alloc;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = typename std::allocator_traits<Alloc>::pointer;
	using const_pointer = typename std::allocator_traits<Alloc>::const_pointer;
	using iterator = T*;
	using const_iterator = const T*;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	my_vector() noexcept(noexcept(Alloc())) : my_vector(Alloc()) {}
	explicit my_vector(const Alloc& alloc) noexcept
		: alloc_data(alloc), first_(nullptr), last_(nullptr), end_(nullptr) {}

	explicit my_vector(size_type count, const Alloc& alloc = Alloc())
		: my_vector(alloc)
	{
		if (count != 0)
		{
			auto cap = count; //Perhaps increase capacity.
			first_ = std::allocator_traits<Alloc>::allocate(get_alloc(), cap);
			last_ = vector_tools::emplace_construct_count(first_, count, get_alloc());
			end_ = first_ + cap;
		}
	}

	explicit my_vector(size_type count, const T &value, const Alloc& alloc = Alloc())
		: my_vector(alloc)
	{
		if (count != 0)
		{
			auto cap = count; //Perhaps increase capacity.
			first_ = std::allocator_traits<Alloc>::allocate(get_alloc(), cap);
			last_ = vector_tools::emplace_construct_count(first_, count, get_alloc(), value);

			end_ = first_ + cap;
		}
	}

	my_vector(const my_vector& other)
		: my_vector(other, std::allocator_traits<Alloc>::select_on_container_copy_construction(other.my_allocator))
	{}

	my_vector(const my_vector& other, const Alloc& alloc)
		: my_vector(alloc)
	{
		auto cap = other.size(); //Perhaps increase capacity.
		first_ = std::allocator_traits<Alloc>::allocate(get_alloc(), cap);
		last_ = vector_tools::copy_insert_range(
			first_, get_alloc(), other.first_, other.last_);

		end_ = first_ + cap;
	}

	my_vector(my_vector &&other) noexcept
		: alloc_data(std::move(other.get_alloc()))
		, first_(other.first_)
		, last_(other.last_)
		, end_(other.end_)
	{
		other.nullify();
	}

	my_vector(my_vector&& other, const Alloc& alloc)
		: my_vector(alloc)
	{
		if (get_alloc() == other.get_alloc())
		{
			//Do actual move by swapping.
			//Our pointers should be NULL.
			this->swap(other);
		}
		else
		{
			//Do element-wise move.
			auto cap = other.size(); //Perhaps increase capacity.
			first_ = std::allocator_traits<Alloc>::allocate(get_alloc(), cap);
			last_ = vector_tools::safemove_insert_range(
				first_, get_alloc(), other.first_, other.last_);

			end_ = first_ + cap;
		}
	}

	my_vector(std::initializer_list<T> init, const Alloc &alloc = Alloc())
		: my_vector(alloc)
	{
		auto cap = init.size();
		first_ = std::allocator_traits<Alloc>::allocate(get_alloc(), cap);
		last_ = vector_tools::copy_insert_range(
			first_, get_alloc(), init.begin(), init.end());

		end_ = first_ + cap;
	}

	~my_vector()
	{
		clear_and_destroy();
	}

	my_vector &operator=(const my_vector &other)
	{
		if (this == &other)
			return *this;

		//Destroy everything in our buffer.
		clear();

		//Don't bother to copy if they're the same.
		if (std::allocator_traits<Alloc>::propagate_on_container_copy_assignment::value &&
			get_alloc() != other.get_alloc())
		{
			//Deallocate the buffer and copy their allocator.
			clear_and_destroy();
			get_alloc() = other.get_alloc();
		}

		//Copy the elements into our buffer.
		ensure_space_exact(other.size());

		//We already deleted all our stuff, so copy-construct away.
		vector_tools::copy_insert_range(first_, get_alloc(), other.begin(), other.end());

		return *this;
	}

	my_vector &operator=(my_vector &&other)
	{
		if (this == &other)
			return *this;

		if (std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value ||
			get_alloc() == other.get_alloc())
		{
			clear_and_destroy();
			get_alloc() = std::move(other.get_alloc());
			first_ = other.first_;
			last_ = other.last_;
			end_ = other.end_;
			other.nullify();
		}
		else
		{
			//Must move individual elements.
			clear();
			ensure_space_exact(other.size());
			vector_tools::safemove_insert_range(first_, get_alloc(), other.begin(), other.end());
		}

		return *this;
	}

	reference at(size_type ix)
	{
		if (ix < size())
			return first_[ix];
		throw std::out_of_range("Out of range");
	}

	const_reference at(size_type ix) const
	{
		if (ix < size())
			return first_[ix];
		throw std::out_of_range("Out of range");
	}

	reference operator[](size_type ix) { return first_[ix]; }
	const_reference operator[](size_type ix) const { return first_[ix]; }

	reference first() { return first_[0]; }
	const_reference first() const { return first_[0]; }

	reference back() { return first_[size() - 1]; }
	const_reference back() const { return first_[size() - 1]; }

	T *data() { return first_; }
	const T *data() const { return first_; }

	bool empty() const noexcept { return first_ == last_; }

	size_type size() const noexcept { return size_type(last_ - first_); }
	size_type max_size() const noexcept { return std::numeric_limits<difference_type>::max(); }

	size_type capacity() const { return size_type(end_ - first_); }

	void swap(my_vector &other) 
		noexcept(noexcept(
		std::allocator_traits<Alloc>::propagate_on_container_swap::value
		/*|| std::allocator_traits<Alloc>::is_always_equal::value*/))
	{
		using std::swap;
		swap(first_, other.first_);
		swap(last_, other.last_);
		swap(end_, other.end_);

		if (std::allocator_traits<Alloc>::propagate_on_container_swap::value)
		{
			swap(get_alloc(), other.get_alloc());
		}
	}

	void clear() noexcept
	{
		vector_tools::destroy_range(first_, last_, get_alloc());
		last_ = first_;
	}

	iterator begin() { return first_; }
	iterator end() { return last_; }
	const_iterator begin() const { return first_; }
	const_iterator end() const { return last_; }
	auto cbegin() const { return begin(); }
	auto cend() const { return end(); }

	reverse_iterator rbegin() { return reverse_iterator(last_); }
	reverse_iterator rend() { return reverse_iterator(first_); }
	const_reverse_iterator rbegin() const { return const_reverse_iterator(last_); }
	const_reverse_iterator rend() const { return const_reverse_iterator(first_); }
	auto crbegin() const { return rbegin(); }
	auto crend() const { return rend(); }


	void reserve(size_type new_cap)
	{
		auto cap = capacity();
		if (cap >= new_cap)
			return;

		reallocate_storage(new_cap);
	}

	void shrink_to_fit()
	{
		if (last_ == end_)
			return;

		reallocate_storage(size());
	}

	void resize(size_type new_size)
	{
		ensure_space_exact(new_size);

		if (new_size > size())
			last_ = vector_tools::emplace_construct_count(last_, new_size - size(), get_alloc());
		else
			remove_from_end(size() - new_size);
	}

	void resize(size_type new_size, const value_type& value)
	{
		ensure_space_exact(new_size);

		if (new_size > size())
			last_ = vector_tools::emplace_construct_count(last_, new_size - size(), get_alloc(), value);
		else
			remove_from_end(size() - new_size);
	}

	iterator erase(const_iterator pos)
	{
		auto r_pos = const_cast<iterator>(pos);
		auto next = r_pos + 1;
		auto new_last = vector_tools::safemove_assign_shift_left(r_pos, next, last_);
		vector_tools::destroy_range(new_last, last_, get_alloc());
		last_ = new_last;

		return const_cast<iterator>(pos); //Launder this?
	}

	iterator erase(const_iterator beg, const_iterator last)
	{
		auto next = const_cast<iterator>(last);
		auto new_last = vector_tools::safemove_assign_shift_left(const_cast<T*>(beg), next, last_);
		vector_tools::destroy_range(new_last, last_, get_alloc());
		last_ = new_last;

		return const_cast<iterator>(beg); //Launder this?
	}

	void push_back(const T &value)
	{
		if (last_ == end_)
			ensure_space_exact(calc_expanded_capacity());

		last_ = vector_tools::emplace_construct_count(last_, 1, get_alloc(), value);
	}

	void push_back(T &&value)
	{
		if (last_ == end_)
			ensure_space_exact(calc_expanded_capacity());

		last_ = vector_tools::emplace_construct_count(last_, 1, get_alloc(), std::move(value));
	}

	template<typename ...Args>
	reference emplace_back(Args&&... args)
	{
		if (last_ == end_)
			ensure_space_exact(calc_expanded_capacity());

		last_ = vector_tools::emplace_construct_count(last_, 1, get_alloc(), std::forward<Args>(args)...);
		return *(last_ - 1);
	}

	void pop_back()
	{
		vector_tools::destroy_range(last_ - 1, last_, get_alloc());
		--last_;
	}

	iterator insert(const_iterator pos, const T &value)
	{
		if (pos == last_)
		{
			push_back(value);
			return (last_ - 1);
		}

		iterator pos_it = const_cast<iterator>(pos);

		if (last_ == end_)
		{
			//Expand storage and copy everything up until `pos`.
			auto new_cap = calc_expanded_capacity();
			auto realloc = alloc_and_partial_copy(new_cap, pos_it);

			//Save this location.
			auto new_pos = realloc.new_last;

			//Copy everything from `pos_it` onwards, but copy it
			//one element past the previous last element.
			realloc.new_last = vector_tools::safemove_insert_range(
				realloc.new_last + 1, get_alloc(), pos_it, last_);

			replace_storage(realloc);

			//Move the value to the saved location, between the two ranges.
			realloc.new_last = vector_tools::copy_insert_range(
				new_pos, get_alloc(), &value, &value + 1);

			return new_pos; //Launder this?
		}

		//There is enough space; shift elements down one and move.
		auto part = vector_tools::safemove_partition_right(pos_it, last_, get_alloc(), end_);
		*part.first = value;
		return pos;
	}

	iterator insert(const_iterator pos, T &&value)
	{
		if (pos == last_)
		{
			push_back(std::move(value));
			return (last_ - 1);
		}

		iterator pos_it = const_cast<iterator>(pos);

		if (last_ == end_)
		{
			//Expand storage and copy everything up until `pos`.
			auto new_cap = calc_expanded_capacity();
			auto realloc = alloc_and_partial_copy(new_cap, pos_it);

			//Save this location.
			auto new_pos = realloc.new_last;

			//Copy everything from `pos_it` onwards, but copy it
			//one element past the previous last element.
			realloc.new_last = vector_tools::safemove_insert_range(
				realloc.new_last + 1, get_alloc(), pos_it, last_);

			replace_storage(realloc);

			//Move the value to the saved location, between the two ranges.
			realloc.new_last = vector_tools::safemove_insert_range(
				new_pos, get_alloc(), &value, &value + 1);

			return new_pos; //Launder this?
		}

		//There is enough space; shift elements down one and move.
		auto part = vector_tools::safemove_partition_right(pos_it, last_, get_alloc(), end_);
		*part.first = std::move(value);
		return pos;
	}

	iterator insert(const_iterator pos, size_type count, const T& value)
	{
		iterator pos_it = const_cast<iterator>(pos);
		iterator new_pos{};

		if (size_type(capacity() - size()) < count)
		{
			//Allocate storage, safe-moving `first_` up to `pos`.
			//copy-insert `count` elements from `value`.
			//safe-move `post` to `last_`.
			//Swap the new storage and delete the old.
			auto new_cap = calc_expanded_capacity(count);
			auto realloc = alloc_and_partial_copy(new_cap, pos_it);
			new_pos = realloc.new_last;
			realloc.new_last = vector_tools::emplace_construct_count(realloc.new_last, count, get_alloc(), value);
			realloc.new_last = vector_tools::safemove_insert_range(
				realloc.new_last, get_alloc(), pos_it, last_);
		}
		else
		{
			//Partition `count` elements.
			//copy-insert/assign `count` `value`s.
			auto part = vector_tools::safemove_partition_right(
				pos_it, last_, get_alloc(), last_ + count);

			new_pos = pos_it;

			//Assign to the assignable range.
			for (; pos_it != part.last; ++pos_it)
				*pos_it = value;

			//Insert to the insertable range.
			vector_tools::emplace_construct_count(
				pos_it, size_type(part.end - pos_it), get_alloc(), value);
		}

		return new_pos; //Launder this?
	}

	iterator insert(const_iterator pos, std::initializer_list<T> ilist)
	{
		iterator pos_it = const_cast<iterator>(pos);
		iterator new_pos{};

		if (size_type(capacity() - size()) < ilist.size())
		{
			//Allocate storage, safe-moving `first_` up to `pos`.
			//copy-insert `ilist`.
			//safe-move `pos` to `last_`.
			//Swap the new storage and delete the old.
			auto new_cap = calc_expanded_capacity(ilist.size());
			auto realloc = alloc_and_partial_copy(new_cap, pos_it);
			new_pos = realloc.new_last;
			realloc.new_last = vector_tools::copy_insert_range(
				realloc.new_last, get_alloc(), ilist.begin(), ilist.end());
			realloc.new_last = vector_tools::safemove_insert_range(
				realloc.new_last, get_alloc(), pos_it, last_);
		}
		else
		{
			//Partition `count` elements.
			//copy-insert/assign `count` `value`s.
			auto part = vector_tools::safemove_partition_right(
				pos_it, last_, get_alloc(), last_ + ilist.size());

			new_pos = pos_it;

			//Assign to the assignable range.
			auto input = ilist.begin();
			for (; pos_it != part.last; ++pos_it, ++input)
				*pos_it = *input;

			//Insert to the insertable range.
			vector_tools::copy_insert_range(
				pos_it, get_alloc(), input, ilist.end());
		}

		return new_pos; //Launder this?
	}

private:
	//Given a number of additional elements to add to the `vector`, calculate the new capacity
	//expanded capacity required.
	//Takes into account the possibility of a zero capacity.
	size_type calc_expanded_capacity(size_type num_additional_elements = 1) const
	{
		auto cap = std::max<size_type>(capacity(), 4);
		if (num_additional_elements > (cap / 2))
			cap = cap + num_additional_elements;

		return cap + (cap / 2);
	}

	struct realloc_data
	{
		T *new_first; T *new_last; T *new_end;

		void flush()
		{
			vector_tools::destroy_range(new_first, new_last, get_alloc());
			std::allocator_traits<Alloc>::deallocate(get_alloc(), new_first, new_end - new_first);
		}
	};

	//Allocates `new_cap` of storage, and does a safe-move
	//of elements from `first_` to `pos`.
	//Does not destroy anything, and the old member pointers remain.
	realloc_data alloc_and_partial_copy(size_type new_cap, iterator pos)
	{
		auto new_first = std::allocator_traits<Alloc>::allocate(get_alloc(), new_cap);
		auto new_last = vector_tools::safemove_insert_range(
			new_first, get_alloc(), first_, pos);

		return { new_first, new_last, new_first + new_cap };
	}

	//Destroys all of the current elements and replaces them with those in `storage`.
	void replace_storage(realloc_data storage)
	{
		auto old_cap = capacity();
		vector_tools::destroy_range(first_, last_, get_alloc());
		std::allocator_traits<Alloc>::deallocate(get_alloc(), first_, old_cap);

		first_ = storage.new_first;
		last_ = storage.new_last;
		end_ = storage.new_end;
	}

	//Allocates storage for `new_cap`,
	//safe-moves all of the current elements
	//destroys the current elements,
	//deallocates the current memory.
	//swaps out the member pointers to new elements and memory.
	void reallocate_storage(size_type new_cap)
	{
		auto old_cap = capacity();

		//"partial" copy the whole thing.
		auto storage = alloc_and_partial_copy(new_cap, last_);
		replace_storage(storage);
	}

	//After calling this function, the capacity shall be no larger than `new_cap`.
	//Performs reallocation if there isn't enough space to hold that many elements.
	//Allocates *exactly* that many elements.
	void ensure_space_exact(size_type new_cap)
	{
		auto curr_cap = capacity();
		if (new_cap > curr_cap)
		{
			//Must reallocate.
			reallocate_storage(new_cap);
		}
	}

	//Destroys `count` elements, starting at the end.
	void remove_from_end(size_type count)
	{
		auto new_last = last_ - count;
		vector_tools::destroy_range(new_last, last_, get_alloc());
		last_ = new_last;
	}

	void clear_and_destroy()
	{
		clear();
		std::allocator_traits<Alloc>::deallocate(get_alloc(), first_, size());
		nullify();
	}

	//Sets all pointers to nullptr.
	void nullify()
	{
		first_ = nullptr;
		last_ = nullptr;
		end_ = nullptr;
	}
};


#endif //MY_VECTOR_TEST_IMPLEMENTATION_HEADER
