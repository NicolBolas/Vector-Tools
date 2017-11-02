
#include "vector_tools\my_vector.hpp"
#include <iostream>
#include <string>

template<typename T, typename Alloc>
void print_vector(const my_vector<T, Alloc> &vec)
{
	std::cout << "Size: " << vec.size() << " Capacity: " << vec.capacity() << std::endl;
	for (const auto &val : vec)
	{
		std::cout << val << " ";
	}

	std::cout << std::endl;
}

int main(int argc, const char*argv[])
{
	my_vector<int> ints({1, 2, 3, 4, 5, 20, 19, 18, 17, 16});

	print_vector(ints);
	
	ints.erase(ints.begin());
	print_vector(ints);

	ints.erase(ints.begin() + 2, ints.begin() + 5);
	print_vector(ints);

	ints.resize(15, 30);
	print_vector(ints);

	ints.resize(5, 20);
	print_vector(ints);

	ints = my_vector<int>(2, 20);

	ints.resize(7);
	print_vector(ints);


	std::cout << "Press keys.\n";

	std::string str;
	std::cin >> str;

	return 0;
}
