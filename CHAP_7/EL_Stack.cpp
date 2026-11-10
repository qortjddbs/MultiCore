#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <immintrin.h>
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
class BACKOFF_v2 {
	int min_delay, max_delay;
	int limit;
public:
	BACKOFF_v2(int min_delay, int max_delay) : min_delay(min_delay), max_delay(max_delay)
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

class BACKOFF {
	int min_delay, max_delay;
	int limit;
public:
	BACKOFF(int min_delay, int max_delay) : min_delay(1), max_delay(max_delay)
	{
		limit = min_delay;
	}
	void delay()
	{
		_mm_pause();
	}
};

class LFBO_STACK {
	bool CAS(NODE* volatile* addr, NODE* oldv, NODE* newv)
	{
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<std::atomic<NODE*> volatile*>(addr), &oldv, newv);
	}
public:
	NODE* volatile top;
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

int num_threads = 0;
std::atomic<int> num_el = 0;

constexpr int EMPTY = 0;
constexpr int WAITING = 1;
constexpr int BUSY = 2;
constexpr int MAX_WAIT = 1000;

class LF_EX {
	volatile int slot;	// 뒤의 2비트는 상태를 나타내고, 나머지 비트는 값(value)을 나타낸다.
public:
	bool CAS(volatile int * addr, int oldv, int newv, int old_state, int new_state)
	{
		int old_slot = (oldv << 2) | old_state;
		int new_slot = (newv << 2) | new_state;
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<std::atomic<int> volatile*>(addr), &old_slot, new_slot);
	}
	LF_EX() : slot(0) {}
	int exchange(int v, bool* is_busy)
	{
		while (true) {
			int old_slot = slot;
			int state = old_slot & 0x3;
			int value = old_slot >> 2;
			switch (state) {
			case EMPTY:
				if (true == CAS(&slot, value, v, EMPTY, WAITING)) {
					// wait for a partner

					for (int i = 0; i < MAX_WAIT; i++) {
						int curr_slot = slot;
						int curr_state = curr_slot & 0x3;
						if (curr_state == BUSY) {
							int v = curr_slot >> 2;
							slot = 0; // reset the slot to EMPTY
							return v; // return the value from the partner
						}
					}
					if (true == CAS(&slot, v, 0, WAITING, EMPTY)) { // reset the slot to EMPTY
						return -2; // TIME OUT
					}
					else {
						int curr_slot = slot;
						int curr_state = curr_slot & 0x3;
						int v = curr_slot >> 2;
						slot = 0; // reset the slot to EMPTY
						return v; // return the value from the partner
					}
				} 
				*is_busy = true;	
				break;
			case WAITING:
					if (true == CAS(&slot, value, v, WAITING, BUSY)) {
						return value; // return the value from the partner
					}
					*is_busy = true;
					break;
			case BUSY:
				*is_busy = true;
				break;
			}
		}
	}
};

class EL_ARRAY {
	int range;
	LF_EX exchanger[MAX_THREADS / 2 - 1];
	bool CAS(int* addr, int oldv, int newv)
	{
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<std::atomic<int>*>(addr), &oldv, newv);
	}
public:
	EL_ARRAY() { range = 1; }
	~EL_ARRAY() {}
	int Visit(int value, bool* time_out) {
		int slot = rand() % range;
		bool busy = false;
		int ret = exchanger[slot].exchange(value, &busy);
		int old_range = range;
		if ((ret == -2) && (old_range > 1))  // TIME OUT
			CAS(&range, old_range, old_range - 1);
		if ((true == busy) && (old_range < num_threads / 2 - 1))
			CAS(&range, old_range, old_range + 1); // MAX RANGE is # of thread / 2
		return ret;
	}
};


class LFEL_STACK {
	bool CAS(NODE* volatile* addr, NODE* oldv, NODE* newv)
	{
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<std::atomic<NODE*> volatile*>(addr), &oldv, newv);
	}
public:
	NODE* volatile top;
	EL_ARRAY el;
	LFEL_STACK()
	{
		std::cout << "Creating Lock-Free Elimination STACK\n";
		top = nullptr;
	}
	void PUSH(int v)
	{
		NODE* node = new NODE(v);
		while (true) {
			node->next = top;
			if (true == CAS(&top, node->next, node)) break;
			bool time_out = false;
			int ret = el.Visit(v, &time_out);
			if (false == time_out)
				if (ret == -1) {
					num_el++;
					return;
				}
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
			if (false == CAS(&top, last, next)) {
				bool time_out = false;
				int ret = el.Visit(-1, &time_out);
				if (ret != -1 && ret != -2) return ret;
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

C_STACK mt_stack;

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
		num_threads = n;
		num_el = 0;
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
		std::cout << "Number of elimination : " << num_el << std::endl;
		check_history(history);
	}


	std::cout << "\nStarting Benchmark\n";
	{
		for (num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			mt_stack.Clear();
			num_el = 0;
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(benchmark, num_threads, i);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			mt_stack.Print();
			std::cout << "Number of elimination : " << num_el << std::endl;
			std::cout << num_threads << " Threads, STACK Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
		}
	}
}