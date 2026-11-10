#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
constexpr int MAX_THREADS = 64;
constexpr int TASK = 1000'0000;

class NODE {
public:
	int value;
	NODE* volatile next;
	NODE(int v) : value(v), next(nullptr) {}
};

class C_STACK {
public:
	NODE* top;
	std::mutex mtx;
	C_STACK()
	{
		std::cout << "Creating Coarse-Grained STACK\n";
		top = nullptr;
	}
	void PUSH(int v)
	{
		NODE* node = new NODE(v);
		mtx.lock();
		node->next = top;
		top = node;
		mtx.unlock();
	}
	int POP()
	{
		mtx.lock();
		if (nullptr == top) {
			mtx.unlock();
			return -2;
		}
		int v = top->value;
		NODE* temp = top;
		top = top->next;
		mtx.unlock();
		delete temp;
		return v;
	}
	void Clear()
	{
		while (top != nullptr)
		{
			NODE* temp = top;
			top = top->next;
			delete temp;
		}
	}
	void Print()
	{
		NODE* curr = top;
		for (int i = 0; i < 20 && curr != nullptr; i++)
		{
			std::cout << curr->value << ", ";
			curr = curr->next;
		}
		std::cout << std::endl;
	}
};

class LF_STACK {
	bool CAS(NODE* volatile* addr, NODE* oldv, NODE* newv)
	{
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<std::atomic<NODE*> volatile *>(addr), &oldv, newv);
	}
public:
	NODE* top;
	LF_STACK()
	{
		std::cout << "Creating Lock-Free STACK\n";
		top = nullptr;
	}
	void PUSH(int v)
	{
		NODE* node = new NODE(v);
		while (true) {
			node->next = top;
			if (true == CAS(&top, node->next, node)) break;
		}
	}
	int POP()
	{
		while (true) {
			NODE* last = top;
			if (nullptr == last) return -2;
			NODE* next = last->next;
			int v = last->value;
			if (last != top) continue;
			if (false == CAS(&top, last, next)) continue;
			// delete temp;
			return v;
		}
	}
	void Clear()
	{
		while (top != nullptr)
		{
			NODE* temp = top;
			top = top->next;
			delete temp;
		}
	}
	void Print()
	{
		NODE* curr = top;
		for (int i = 0; i < 20 && curr != nullptr; i++)
		{
			std::cout << curr->value << ", ";
			curr = curr->next;
		}
		std::cout << std::endl;
	}
};

class BACKOFF_v1 {
	int min_delay, max_delay;
	int limit;
public:
	BACKOFF_v1(int min_delay, int max_delay) : min_delay(min_delay), max_delay(max_delay)
	{
		limit = min_delay;
	}
	void delay()
	{
		int delay = rand() % limit;
		if (limit < max_delay) limit *= 2;
		std::this_thread::sleep_for(std::chrono::microseconds(delay));
	}
};

#include <intrin.h>
class BACKOFF {
	int min_delay, max_delay;
	int limit;
public:
	BACKOFF(int min_delay, int max_delay) : min_delay(min_delay), max_delay(max_delay)
	{
		limit = min_delay;
	}
	void delay()
	{
		int delay = rand() % limit;
		if (limit < max_delay) limit *= 2;
		delay = delay * 500;
		auto start = __rdtsc();
		while (__rdtsc() - start < delay);
	}
};

class LFBO_STACK {
	bool CAS(NODE* volatile* addr, NODE* oldv, NODE* newv)
	{
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<std::atomic<NODE*> volatile*>(addr), &oldv, newv);
	}
public:
	NODE* top;
	LFBO_STACK()
	{
		std::cout << "Creating Lock-Free Back Off STACK\n";
		top = nullptr;
	}
	void PUSH(int v)
	{
		BACKOFF bo(1, MAX_THREADS);
		NODE* node = new NODE(v);
		while (true) {
			node->next = top;
			if (true == CAS(&top, node->next, node)) break;
			bo.delay();
		}
	}
	int POP()
	{
		while (true) {
			BACKOFF bo(1, MAX_THREADS);
			NODE* last = top;
			if (nullptr == last) return -2;
			NODE* next = last->next;
			int v = last->value;
			if (last != top) continue;
			if (false == CAS(&top, last, next)) {
				bo.delay();
				continue;
			}
			// delete temp;
			return v;
		}
	}
	void Clear()
	{
		while (top != nullptr)
		{
			NODE* temp = top;
			top = top->next;
			delete temp;
		}
	}
	void Print()
	{
		NODE* curr = top;
		for (int i = 0; i < 20 && curr != nullptr; i++)
		{
			std::cout << curr->value << ", ";
			curr = curr->next;
		}
		std::cout << std::endl;
	}
};

thread_local int thread_id = 999;

//int num_threads = 0;

LFBO_STACK mt_stack;

struct HISTORY {
	std::vector <int> push_values, pop_values;
};
std::atomic_int stack_size;

void benchmark_test(const int th_id, const int num_threads, HISTORY& h)
{
	thread_id = th_id;
	int loop_count = TASK / num_threads;
	for (int i = 0; i < loop_count; i++) {
		if ((rand() % 2) || i < 128 / num_threads) {
			h.push_values.push_back(i);
			stack_size++;
			mt_stack.PUSH(i);
		}
		else {
			volatile int curr_size = stack_size--;
			int res = mt_stack.POP();
			if (res == -2) {
				stack_size++;
				if ((curr_size > num_threads * 2) && (stack_size > num_threads)) {
					std::cout << "ERROR Non_Empty Stack Returned NULL\n";
					exit(-1);
				}
			}
			else h.pop_values.push_back(res);
		}
	}
}

#include <unordered_set>
#include <set>

void check_history(std::vector <HISTORY>& h)
{
	std::unordered_multiset <int> pushed, poped, in_stack;

	for (auto& v : h)
	{
		for (auto num : v.push_values) pushed.insert(num);
		for (auto num : v.pop_values) poped.insert(num);
		while (true) {
			int num = mt_stack.POP();
			if (num == -2) break;
			poped.insert(num);
		}
	}
	for (auto num : pushed) {
		if (poped.count(num) < pushed.count(num)) {
			std::cout << "Pushed Number " << num << " does not exists in the STACK.\n";
			exit(-1);
		}
		if (poped.count(num) > pushed.count(num)) {
			std::cout << "Pushed Number " << num << " is poped more than " << poped.count(num) - pushed.count(num) << " times.\n";
			exit(-1);
		}
	}
	for (auto num : poped)
		if (pushed.count(num) == 0) {
			std::multiset <int> sorted;
			for (auto num : poped)
				sorted.insert(num);
			std::cout << "There were elements in the STACK no one pushed : ";
			int count = 20;
			for (auto num : sorted)
				std::cout << num << ", ";
			std::cout << std::endl;
			exit(-1);

		}
	std::cout << "NO ERROR detectd.\n";
}


void benchmark(const int NUM_THREADS, int th_id)
{
	int key = 0;
	thread_id = th_id;
	const int loop_count = TASK / NUM_THREADS;
	for (auto i = 0; i < loop_count; ++i) {
		if ((rand() % 2 == 0) || (i < 1000))
			mt_stack.PUSH(key++);
		else
			mt_stack.POP();
	}
}

int main() {
	using namespace std::chrono;

	std::cout << "\nStarting Error Check\n";
	for (int n = 1; n <= MAX_THREADS; n = n * 2) {
		//num_threads = n;
		mt_stack.Clear();
		std::vector<std::thread> tv;
		std::vector<HISTORY> history;
		history.resize(n);
		stack_size = 0;
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < n; ++i) {
			tv.emplace_back(benchmark_test, i, n, std::ref(history[i]));
		}
		for (auto& th : tv)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		size_t ms = duration_cast<milliseconds>(exec_t).count();
		std::cout << n << " Threads,  " << ms << "ms. ----";
		mt_stack.Print();
		check_history(history);
	}


	std::cout << "\nStarting Benchmark\n";
	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			mt_stack.Clear();
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(benchmark, num_threads, i);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			mt_stack.Print();
			std::cout << num_threads << " Threads, STACK Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
		}
	}
}