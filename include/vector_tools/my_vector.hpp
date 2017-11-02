#ifndef MY_VECTOR_TEST_IMPLEMENTATION_HEADER
#define MY_VECTOR_TEST_IMPLEMENTATION_HEADER

#include "vector_tools.hpp"
#include <cstddef>
#include <memory>
#include <iterator>
#include <stdexcept>
#include <limits>
#include <initializer_list>

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

		void copy_allocator(const Alloc &new_alloc) { get_alloc() = new_alloc; }
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
			last_ = vector_tools::param_construct_count(first_, count, get_alloc());
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
			last_ = vector_tools::param_construct_count(first_, count, get_alloc(), value);

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
		last_ = vector_tools::copy_ptr_construct_count(
			first_, other.size(), get_alloc(), other.first_);

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
			last_ = vector_tools::safemove_ptr_construct_count(
				first_, other.size(), get_alloc(), other.first);

			end_ = first_ + cap;
		}
	}

	my_vector(std::initializer_list<T> init, const Alloc &alloc = Alloc())
		: my_vector(alloc)
	{
		auto cap = init.size();
		first_ = std::allocator_traits<Alloc>::allocate(get_alloc(), cap);
		last_ = vector_tools::copy_ptr_construct_count(
			first_, init.size(), get_alloc(), init.begin());

		end_ = first_ + cap;
	}

	~my_vector()
	{
		clear_and_destroy();
	}

	my_vector &operator=(const my_vector &other)
	{
		//Destroy everything in our buffer.
		clear();

		//Don't bother to copy if they're the same.
		if (std::allocator_traits<Alloc>::propagate_on_container_copy_assignment::value &&
			get_alloc() != other.get_alloc())
		{
			//Deallocate the buffer and copy their allocator.
			clear_and_destroy();
			copy_allocator(other.get_alloc());
		}

		//Copy the elements into our buffer.
		ensure_space_exact(other.size());

		//We already deleted all our stuff, so copy-construct away.
		vector_tools::copy_ptr_construct_count(first_, get_alloc(), other.begin(), other.end());

		return *this;
	}

	my_vector &operator=(my_vector &&other)
	{
		if (std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value ||
			get_alloc() == other.get_alloc())
		{
			clear_and_destroy();
			copy_allocator(other.get_alloc());
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
			vector_tools::safemove_ptr_construct_range(first_, get_alloc(), other.begin(), other.end());
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

		auto storage = reallocate_storage(new_cap);

		first_ = storage.new_first;
		last_ = storage.new_last;
		end_ = first_ + new_cap;
	}

	void shrink_to_fit()
	{
		if (last_ == end_)
			return;

		auto storage = reallocate_storage(size());

		first_ = storage.new_first;
		last_ = storage.new_last;
		end_ = last_;
	}

	void resize(size_type new_size)
	{
		ensure_space_exact(new_size);

		if (new_size > size())
			last_ = vector_tools::param_construct_count(last_, new_size - size(), get_alloc());
		else
			remove_from_end(size() - new_size);
	}

	void resize(size_type new_size, const value_type& value)
	{
		ensure_space_exact(new_size);

		if (new_size > size())
			last_ = vector_tools::param_construct_count(last_, new_size - size(), get_alloc(), value);
		else
			remove_from_end(size() - new_size);
	}

	iterator erase(const_iterator pos)
	{
		auto r_pos = const_cast<T*>(pos);
		auto next = r_pos + 1;
		auto new_last = vector_tools::safemove_shift_left(r_pos, next, last_);
		vector_tools::destroy_range(new_last, last_, get_alloc());
		last_ = new_last;

		return const_cast<T*>(pos); //Launder this?
	}

	iterator erase(const_iterator first, const_iterator last)
	{
		auto next = const_cast<T*>(last);
		auto new_last = vector_tools::safemove_shift_left(const_cast<T*>(first), next, last_);
		vector_tools::destroy_range(new_last, last_, get_alloc());
		last_ = new_last;

		return const_cast<T*>(first); //Launder this?
	}

private:
	struct realloc_data { T *new_first; T *new_last; };

	//Allocates new storage for capacity,
	//safe-moves the elements
	//destroys the old elements,
	//deallocates the old memory.
	//Note: will fail if `new_cap` < `size()`.
	realloc_data reallocate_storage(size_type new_cap)
	{
		auto old_cap = capacity();
		auto new_first = std::allocator_traits<Alloc>::allocate(get_alloc(), new_cap);
		auto new_last = vector_tools::safemove_ptr_construct_range(
			new_first, get_alloc(), first_, last_);

		vector_tools::destroy_range(first_, last_, get_alloc());
		std::allocator_traits<Alloc>::deallocate(get_alloc(), first_, old_cap);

		return { new_first, new_last };
	}

	//Performs reallocation if there isn't enough space to hold that many elements.
	//Allocates *exactly* that many elements.
	void ensure_space_exact(size_type new_cap)
	{
		auto curr_cap = capacity();
		if (new_cap > curr_cap)
		{
			//Must reallocate.
			auto storage = reallocate_storage(new_cap);

			first_ = storage.new_first;
			last_ = storage.new_last;
			end_ = first_ + new_cap;
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
