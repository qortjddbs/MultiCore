#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>

constexpr int MAX_THREADS = 16;
constexpr int TASK = 5000'0000;
volatile int sum;
std::atomic <int> atomic_sum;
std::mutex ll;
constexpr int CACHE_LINE_SIZE = 64;
constexpr int STRIDE = CACHE_LINE_SIZE / sizeof(int);
volatile int array_sum[MAX_THREADS * STRIDE];

void worker(const int NUM_THREADS)
{
	for (int i = 0; i < TASK / NUM_THREADS; ++i)
	{
		//ll.lock();
		sum += 2;
		//ll.unlock();
	}
}

void atomic_worker(const int NUM_THREADS)
{
	for (int i = 0; i < TASK / NUM_THREADS; ++i)
	{
		atomic_sum.fetch_add(2);
		// atomic_sum += 2;
	}
}

void optimal_worker(const int NUM_THREADS)
{
	volatile int local_sum = 0;
	for (int i = 0; i < TASK / NUM_THREADS; ++i)
	{
		local_sum = local_sum + 2;
	}
	ll.lock();
	sum = sum + local_sum;
	ll.unlock();
}

void array_worker(const int NUM_THREADS, const int THREAD_ID)
{
	for (int i = 0; i < TASK / NUM_THREADS; ++i)
	{
		array_sum[THREAD_ID] = array_sum[THREAD_ID] + 2;
	}
}

void stride_array_worker(const int NUM_THREADS, const int THREAD_ID)
{
	for (int i = 0; i < TASK / NUM_THREADS; ++i)
	{
		array_sum[THREAD_ID] = array_sum[THREAD_ID] + 2;
	}
}

int main() {
	using namespace std::chrono;
	//char ch;
	//std::cin >> ch;
	{
		auto start_t = high_resolution_clock::now();
		for (auto i = 0; i < TASK; ++i)
			sum = sum + 2;
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		std::cout << "Single Thread SUM = " << sum;
		std::cout << ",  Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
	}
	std::cout << "----------------------------------------\n";
	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			sum = 0;
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(worker, num_threads);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			std::cout << num_threads << " ThreadSUM = " << sum;
			std::cout << ",  Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
		}
	}
	std::cout << "----------------------------------------\n";
	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			atomic_sum = 0;
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(atomic_worker, num_threads);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			std::cout << num_threads << " Thread Atomic SUM = " << atomic_sum;
			std::cout << ",  Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
		}
	}
	std::cout << "----------------------------------------\n";
	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			sum = 0;
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(optimal_worker, num_threads);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			std::cout << num_threads << " Thread Optimal SUM = " << sum;
			std::cout << ",  Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
		}
	}

	std::cout << "---------------- ARRAY ------------------------\n";
	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			sum = 0;
			for (auto& s : array_sum) s = 0;
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(array_worker, num_threads, i);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			for (int i = 0; i < num_threads; ++i)
				sum += array_sum[i];
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			std::cout << num_threads << " Thread Optimal SUM = " << sum;
			std::cout << ",  Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
		}
	}

	std::cout << "---------------- ARRAY STRIDE ------------------------\n";
	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			sum = 0;
			for (auto& s : array_sum) s = 0;
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(stride_array_worker, num_threads, i);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			for (int i = 0; i < num_threads; ++i)
				sum += array_sum[i * STRIDE];
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			std::cout << num_threads << " Thread ARRAY STRIDE SUM = " << sum;
			std::cout << ",  Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
		}
	}
}