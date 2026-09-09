#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>

constexpr int MAX_THREADS = 16;
constexpr int TASK = 5000'0000;
volatile int sum;		// volatile -> 컴파일러가 맘대로 최적화하지 못하게 함
std::atomic<int> atomic_sum;	// atomic -> 원자적 연산을 보장
std::mutex ll;
volatile int array_sum[MAX_THREADS];

void worker(const int NUM_THREADS)
{
	for (int i = 0; i < TASK / NUM_THREADS; ++i)
	{
		ll.lock();
		sum = sum + 2;
		ll.unlock();
	}
}

void atomic_worker(const int NUM_THREADS)
{
	for (int i = 0; i < TASK / NUM_THREADS; ++i)
	{
		atomic_sum.fetch_add(2);		// == atomic_sum += 2;
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
	
}

int main()
{
	{
		auto start_t = std::chrono::high_resolution_clock::now();
		for (auto i = 0; i < TASK; ++i)
		{
			sum = sum + 2;
		}
		auto end_t = std::chrono::high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		std::cout << "Single Thread SUM : " << sum;
		std::cout << ", Execution time: " << duration_cast<std::chrono::milliseconds>(exec_t).count() << " ms\n";
	}
	{
		std::cout << "------------------------------------------------------------------------\n";
		sum = 0;
		for (int i = 1; i <= MAX_THREADS; i *= 2)
		{
			sum = 0;
			auto start_t = std::chrono::high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int j = 0; j < i; ++j)
			{
				threads[j] = std::thread(worker, i);
			}
			for (int j = 0; j < i; ++j)
			{
				threads[j].join();
			}
			auto end_t = std::chrono::high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			std::cout << i << " Thread Lock SUM : " << sum;
			std::cout << ", Execution time: " << duration_cast<std::chrono::milliseconds>(exec_t).count() << " ms\n";
		}
	}
	{
		std::cout << "------------------------------------------------------------------------\n";
		for (int i = 1; i <= MAX_THREADS; i *= 2)
		{
			atomic_sum = 0;
			auto start_t = std::chrono::high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int j = 0; j < i; ++j)
			{
				threads[j] = std::thread(atomic_worker, i);
			}
			for (int j = 0; j < i; ++j)
			{
				threads[j].join();
			}
			auto end_t = std::chrono::high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			std::cout << i << " Thread Atomic SUM : " << atomic_sum;
			std::cout << ", Execution time: " << duration_cast<std::chrono::milliseconds>(exec_t).count() << " ms\n";
		}
	}
	{
		std::cout << "------------------------------------------------------------------------\n";
		for (int i = 1; i <= MAX_THREADS; i *= 2)
		{
			sum = 0;
			auto start_t = std::chrono::high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int j = 0; j < i; ++j)
			{
				threads[j] = std::thread(optimal_worker, i);
			}
			for (int j = 0; j < i; ++j)
			{
				threads[j].join();
			}
			auto end_t = std::chrono::high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			std::cout << i << " Thread Optimal SUM : " << sum;
			std::cout << ", Execution time: " << duration_cast<std::chrono::milliseconds>(exec_t).count() << " ms\n";
		}
	}
	{
		std::cout << "------------------------------------------------------------------------\n";
		for (int i = 1; i <= MAX_THREADS; i *= 2)
		{
			sum = 0;
			for (auto& s : array_sum) s = 0;
			auto start_t = std::chrono::high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int j = 0; j < i; ++j)
			{
				threads[j] = std::thread(array_worker, i, j);
			}
			for (int j = 0; j < i; ++j)
			{
				threads[j].join();
			}
			auto end_t = std::chrono::high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			std::cout << i << " Thread Array SUM : " << sum;
			std::cout << ", Execution time: " << duration_cast<std::chrono::milliseconds>(exec_t).count() << " ms\n";
		}
	}
}