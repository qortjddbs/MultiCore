#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>

constexpr int MAX_THREADS = 2;
constexpr int TASK = 5000'0000;
volatile int sum;
std::mutex ll;

volatile int victim = 0;
std::atomic<unsigned char> flag[2] = { 0, 0 };

void p_lock(int my_id)
{
	int other_id = 1 - my_id;
	flag[my_id] = true;
	victim = my_id;
	//_asm mfence;
	// std::atomic_thread_fence(std::memory_order_seq_cst);
	while ((flag[other_id].fetch_add(0) == 1) && victim == my_id);
}

void p_unlock(int my_id)
{
	flag[my_id] = 0;
}

void p_worker(const int NUM_THREADS, const int THREAD_ID)
{
	for (int i = 0; i < TASK / NUM_THREADS; ++i)
	{
		p_lock(THREAD_ID);
		sum += 2;
		p_unlock(THREAD_ID);
	}
}

volatile int cas_lock_flag = 0;

bool CAS(volatile int* addr, int expected, int update)
{
	return std::atomic_compare_exchange_strong(reinterpret_cast<volatile std::atomic<int>*>(addr), &expected, update);
}

void cas_lock()
{
	while (!CAS(&cas_lock_flag, 0, 1))
		_mm_pause();
}

void cas_unlock()
{
	CAS(&cas_lock_flag, 1, 0);
}

void cas_worker(const int NUM_THREADS)
{
	for (int i = 0; i < TASK / NUM_THREADS; ++i)
	{
		cas_lock();
		sum = sum + 2;
		cas_unlock();
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
	std::cout << "--------------Peterson--------------------------\n";
	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			sum = 0;
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(p_worker, num_threads, i);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			std::cout << num_threads << " ThreadSUM = " << sum;
			std::cout << ",  Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
		}
	}
	std::cout << "----------------mutex------------------------\n";
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
	std::cout << "-------------Optimal---------------------------\n";
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

	std::cout << "-------------CAS---------------------------\n";
	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			sum = 0;
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(cas_worker, num_threads);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			std::cout << num_threads << " Thread CAS SUM = " << sum;
			std::cout << ",  Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
		}
	}
}