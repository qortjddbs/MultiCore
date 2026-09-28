#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

constexpr int MAX_THREADS = 8;
constexpr int TASK = 5000'0000;
volatile int sum;
std::mutex ll;

volatile bool flag[MAX_THREADS];
volatile int label[MAX_THREADS];

bool Exists_Higher_Priority(const int THREAD_ID, const int NUM_THREADS)
{
	for (int k = 0; k < NUM_THREADS; ++k)
	{
		if (k == THREAD_ID) continue;
		if (flag[k] == false) continue;
		int lk = label[k];
		int li = label[THREAD_ID];
		if (lk < li || (lk == li && k < THREAD_ID)) return true;
	}
	return false;
}

void Lock(const int THREAD_ID, const int NUM_THREADS)
{
	flag[THREAD_ID] = true;

	int max_label = 0;
	for (int k = 0; k < NUM_THREADS; ++k)
	{
		int l = label[k];
		if (l > max_label) max_label = l;
	}
	label[THREAD_ID] = max_label + 1;

	while (Exists_Higher_Priority(THREAD_ID, NUM_THREADS)) {}
}

void Unlock(const int THREAD_ID)
{
	flag[THREAD_ID] = false;
}

std::atomic<bool> a_flag[MAX_THREADS];
std::atomic<int> a_label[MAX_THREADS];

bool Exists_Higher_Priority_atomic(const int THREAD_ID, const int NUM_THREADS)
{
	for (int k = 0; k < NUM_THREADS; ++k)
	{
		if (k == THREAD_ID) continue;
		if (a_flag[k] == false) continue;
		int lk = a_label[k];
		int li = a_label[THREAD_ID];
		if (lk < li || (lk == li && k < THREAD_ID)) return true;
	}
	return false;
}

void Lock_atomic(const int THREAD_ID, const int NUM_THREADS)
{
	a_flag[THREAD_ID] = true;

	int max_label = 0;
	for (int k = 0; k < NUM_THREADS; ++k)
	{
		int l = a_label[k];
		if (l > max_label) max_label = l;
	}
	a_label[THREAD_ID] = max_label + 1;

	while (Exists_Higher_Priority_atomic(THREAD_ID, NUM_THREADS)) {}
}

void Unlock_atomic(const int THREAD_ID)
{
	a_flag[THREAD_ID] = false;
}

void worker(const int NUM_THREADS)
{
	for (int i = 0; i < TASK / NUM_THREADS; ++i)
	{
		sum = sum + 2;
	}
}

void mutex_worker(const int NUM_THREADS)
{
	for (int i = 0; i < TASK / NUM_THREADS; ++i)
	{
		ll.lock();
		sum = sum + 2;
		ll.unlock();
	}
}

void bakery_worker(const int NUM_THREADS, const int THREAD_ID)
{
	for (int i = 0; i < TASK / NUM_THREADS; ++i)
	{
		Lock(THREAD_ID, NUM_THREADS);
		sum = sum + 2;
		Unlock(THREAD_ID);
	}
}

void bakery_atomic_worker(const int NUM_THREADS, const int THREAD_ID)
{
	for (int i = 0; i < TASK / NUM_THREADS; ++i)
	{
		Lock_atomic(THREAD_ID, NUM_THREADS);
		sum = sum + 2;
		Unlock_atomic(THREAD_ID);
	}
}

int main() {
	using namespace std::chrono;

	std::cout << "---------------- NO LOCK ------------------------\n";
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
			std::cout << num_threads << " Thread No Lock SUM = " << sum;
			std::cout << ",  Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
		}
	}

	std::cout << "---------------- MUTEX ------------------------\n";
	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			sum = 0;
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(mutex_worker, num_threads);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			std::cout << num_threads << " Thread Mutex SUM = " << sum;
			std::cout << ",  Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
		}
	}

	for (int repeat = 0; repeat < 10; ++repeat) {
		std::cout << "---------------- BAKERY VOLATILE ------------------------\n";
		{
			for (int num_threads = 8; num_threads <= MAX_THREADS; num_threads *= 2)
			{
				sum = 0;
				for (int k = 0; k < MAX_THREADS; ++k)
				{
					flag[k] = false;
					label[k] = 0;
				}
				auto start_t = high_resolution_clock::now();
				std::thread threads[MAX_THREADS];
				for (int i = 0; i < num_threads; ++i)
					threads[i] = std::thread(bakery_worker, num_threads, i);
				for (int i = 0; i < num_threads; ++i)
					threads[i].join();
				auto end_t = high_resolution_clock::now();
				auto exec_t = end_t - start_t;
				std::cout << num_threads << " Thread Bakery Volatile SUM = " << sum;
				std::cout << ",  Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
			}
		}
	}
	

	std::cout << "---------------- BAKERY ATOMIC ------------------------\n";
	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			sum = 0;
			for (int k = 0; k < MAX_THREADS; ++k)
			{
				a_flag[k] = false;
				a_label[k] = 0;
			}
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(bakery_atomic_worker, num_threads, i);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			std::cout << num_threads << " Thread Bakery Atomic SUM = " << sum;
			std::cout << ",  Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
		}
	}
}
