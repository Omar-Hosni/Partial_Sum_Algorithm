
#include <iostream>
#include <vector>
#include <iomanip>
#include <thread>
#include <future>
#include <atomic>
#include <list>
#include <string>
#include <execution>
#include <functional>
#include <numeric>
#include <cmath>
#include <ctime>
#include <ratio>

#include "Header1.h"


using namespace std;


template<typename Iterator>
void parallel_partial_sum(Iterator first, Iterator last) {

	typedef typename Iterator::value_type value_type;

	struct process_chunk
	{
		void operator()(Iterator begin, Iterator last,
			future<value_type>* previous_end_value,
			promise<value_type>* end_value)
		{
			try {
				Iterator end = last;
				++end;
				partial_sum(begin, end, begin);
				if (previous_end_value)
				{
					auto addend = previous_end_value->get();
					*last += addend;
					if (end_value)
					{
						//not last block
						end_value->set_value(*last);
					}
					for_each(begin, last, [addend](value_type& item) { item += addend; });
				}
				else if (end_value)
				{
					//this is the first thread
					end_value->set_value(*last);
				}
			}
			catch (exception e) {
				if (end_value) {
					end_value->set_exception(current_exception());

				}
				else
				{
					//final block - main threads is the one process the final block
					throw;
				}
			}
		}
	};

	unsigned long const length = distance(first, last);
	if (!length) return;

	unsigned long const min_per_thread = 25;
	unsigned long const max_threads = (length + min_per_thread - 1) / min_per_thread;
	unsigned long const hardware_threads = thread::hardware_concurrency();
	unsigned long const num_threads = min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
	unsigned long block_size = length / num_threads;

	vector<thread> threads(num_threads - 1);
	vector<promise<value_type>> end_values(num_threads - 1);
	vector<future<value_type>> previous_end_values;
	previous_end_values.reserve(num_threads - 1); //reserve function is a built-in function that requests a change of capacity

	join_threads joiner(threads);

	Iterator block_start = first;

	for (unsigned long i = 0; i < (num_threads - 1); ++i) 
	{
		Iterator block_last = block_start;
		advance(block_last, block_size - 1);

		threads[i] = thread(process_chunk(), block_start, block_last, (i != 0) ? &previous_end_values[i - 1] : 0, &end_values[i]);
		
		block_start = block_last;
		++block_start;
		previous_end_values.push_back(end_values[i].get_future());
	}

	Iterator final_element = block_start;
	advance(final_element, distance(block_start, last) - 1);
	process_chunk()(block_start, final_element, (num_threads > 1) ? &previous_end_values.back() : 0, 0);

}

const size_t testSize = 10000000;

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::milli;



int main() {

	vector<int> ints(testSize);
	vector<int> outs(testSize);

	for (auto& i : ints) {
		i = 1;
	}


	//sequential implementation 
	high_resolution_clock::time_point t1 = high_resolution_clock::now();
	inclusive_scan(ints.cbegin(), ints.cend(), outs.begin());
	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	duration<double> result= duration_cast<duration<double>>(t2 - t1);
	cout << "sequential time: " << result.count() << "ms" << endl;

	//C++17 parallel implementation Time
	high_resolution_clock::time_point p1 = high_resolution_clock::now();
	inclusive_scan(execution::par, ints.cbegin(), ints.cend(), outs.begin());
	high_resolution_clock::time_point p2 = high_resolution_clock::now();
	duration<double> result2 = duration_cast<duration<double>>(p2 - p1);
	cout << "C++17 parallel time: " << result2.count() << "ms" << endl;

	//my parallel algorithm
	high_resolution_clock::time_point m1 = high_resolution_clock::now();
	parallel_partial_sum(ints.begin(), ints.end());
	high_resolution_clock::time_point m2 = high_resolution_clock::now();
	duration<double> result3 = duration_cast<duration<double>>(m2 - m1);
	cout << "my algorithm's parallel time: " << result3.count() << "ms" << endl;


}
