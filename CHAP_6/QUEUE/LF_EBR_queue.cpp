#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <queue>
constexpr int MAX_THREADS = 16;
constexpr int TASK = 1000'0000;

class NODE {
public:
	int value;
	NODE* volatile next;
	int epoch;
	NODE(int v) : value(v), next(nullptr), epoch(0) {}
};

class C_QUEUE {
public:
	NODE* head, * tail;
	std::mutex enq_m, deq_m;
	C_QUEUE()
	{
		std::cout << "Creating Coarse-Grained QUEUE\n";
		head = new NODE(std::numeric_limits<int>::min());
		tail = head;
	}
	void ENQ(int v)
	{
		NODE* node = new NODE(v);
		enq_m.lock();
		tail->next = node;
		tail = node;
		enq_m.unlock();
	}
	int DEQ()
	{
		deq_m.lock();
		if (head->next == nullptr) {
			deq_m.unlock();
			return -1;
		}
		int value = head->next->value;
		NODE* temp = head;
		head = head->next;
		deq_m.unlock();
		delete temp;
		return value;
	}
	void Clear()
	{
		NODE* curr = head->next;
		while (curr != nullptr)
		{
			NODE* temp = curr;
			curr = curr->next;
			delete temp;
		}
		head->next = nullptr;
		tail = head;
	}
	void Print()
	{
		NODE* curr = head->next;
		for (int i = 0; i < 20 && curr != nullptr; i++)
		{
			std::cout << curr->value << ", ";
			curr = curr->next;
		}
		std::cout << std::endl;
	}
};

class LF_QUEUE {
public:
	NODE* volatile head, * volatile tail;
	LF_QUEUE()
	{
		std::cout << "Creating Lock-Free QUEUE\n";
		head = new NODE(std::numeric_limits<int>::min());
		tail = head;
	}
	bool CAS(NODE* volatile& ptr, NODE* old_node, NODE* new_node)
	{
		return std::atomic_compare_exchange_strong(reinterpret_cast<std::atomic<NODE*> volatile*>(&ptr), &old_node, new_node);
	}
	void ENQ(int v)
	{
		NODE* node = new NODE(v);
		while (true) {
			NODE* old_tail = tail;
			NODE* next = old_tail->next;
			if (old_tail != tail) continue;
			if (next == nullptr) {
				if (CAS(old_tail->next, nullptr, node)) {
					CAS(tail, old_tail, node);
					return;
				}
			}
			else {
				CAS(tail, old_tail, next);
			}
		}
	}
	int DEQ()
	{
		while (true) {
			NODE* first = head;
			NODE* last = tail;
			NODE* next = first->next;
			if (first != head) continue;
			if (next == nullptr) return -1;
			if (first == last) {
				CAS(tail, last, next);
				continue;
			}
			int value = next->value;
			if (false == CAS(head, first, next)) continue;
			delete first;
			return value;
		}
	}
	void Clear()
	{
		NODE* curr = head->next;
		while (curr != nullptr)
		{
			NODE* temp = curr;
			curr = curr->next;
			delete temp;
		}
		head->next = nullptr;
		tail = head;
	}
	void Print()
	{
		NODE* curr = head->next;
		for (int i = 0; i < 20 && curr != nullptr; i++)
		{
			std::cout << curr->value << ", ";
			curr = curr->next;
		}
		std::cout << std::endl;
	}
};

thread_local int thread_id = 999;

class EBR {
private:
	std::atomic<int> epoch;
	std::queue<NODE*> memory_pool[MAX_THREADS];
	std::atomic<int> epoch_counter[MAX_THREADS];
public:
	EBR() :epoch(0)
	{
		for (auto& c : epoch_counter) c = std::numeric_limits<int>::max();
	}
	~EBR() {
		for (auto& p : memory_pool) {
			while (!p.empty()) {
				delete p.front();
				p.pop();
			}
		}
	}

	NODE* get_node(int value) {
		if (memory_pool[thread_id].empty())
			return new NODE(value);
		else {
			// 안전한가 아닌가 검사한 다음에 꺼내야 한다.
			NODE* node = memory_pool[thread_id].front();		// 맨 앞 노드를 '보기만' 하는 놈
			for (int i = 0; i < MAX_THREADS; ++i) {
				if (node->epoch >= epoch_counter[i])
					return new NODE(value);
			}
			memory_pool[thread_id].pop();						// 실제로 꺼내는 놈
			node->value = value;
			node->next = nullptr;
			return node;
		}
	}
	void free_node(NODE* node) {
		node->epoch = epoch;
		memory_pool[thread_id].push(node);
	}

	void enter() {
		epoch_counter[thread_id] = ++epoch;
	}
	void leave() {
		epoch_counter[thread_id] = std::numeric_limits<int>::max();
	}
};

EBR ebr;

class EBR_LF_QUEUE {
public:
	NODE* volatile head, * volatile tail;
	EBR_LF_QUEUE()
	{
		std::cout << "Creating EBR Lock-Free QUEUE\n";
		head = new NODE(std::numeric_limits<int>::min());
		tail = head;
	}
	bool CAS(NODE* volatile& ptr, NODE* old_node, NODE* new_node)
	{
		return std::atomic_compare_exchange_strong(reinterpret_cast<std::atomic<NODE*> volatile*>(&ptr), &old_node, new_node);
	}
	void ENQ(int v)
	{
		ebr.enter();
		NODE* node = ebr.get_node(v);
		while (true) {
			NODE* old_tail = tail;
			NODE* next = old_tail->next;
			if (old_tail != tail) continue;
			if (next == nullptr) {
				if (CAS(old_tail->next, nullptr, node)) {
					CAS(tail, old_tail, node);
					ebr.leave();
					return;
				}
			}
			else {
				CAS(tail, old_tail, next);
			}
		}
	}
	int DEQ()
	{
		ebr.enter();
		while (true) {
			NODE* first = head;
			NODE* last = tail;
			NODE* next = first->next;
			if (first != head) continue;
			if (next == nullptr) {
				ebr.leave();
				return -1;
			}
			if (first == last) {
				CAS(tail, last, next);
				continue;
			}
			int value = next->value;
			if (false == CAS(head, first, next)) continue;
			ebr.free_node(first);
			ebr.leave();
			return value;
		}
	}
	void Clear()
	{
		NODE* curr = head->next;
		while (curr != nullptr)
		{
			NODE* temp = curr;
			curr = curr->next;
			delete temp;
		}
		head->next = nullptr;
		tail = head;
	}
	void Print()
	{
		NODE* curr = head->next;
		for (int i = 0; i < 20 && curr != nullptr; i++)
		{
			std::cout << curr->value << ", ";
			curr = curr->next;
		}
		std::cout << std::endl;
	}
};

EBR_LF_QUEUE mt_queue;

#include <unordered_set>
#include <queue>
#include <array>

class HISTORY {
public:
	int op;
	int i_value;
	int o_value;
	HISTORY(int o, int i, int re) : op(o), i_value(i), o_value(re) {}
};

std::array<std::vector<HISTORY>, MAX_THREADS> history;

int num_threads = 0;

void check_history(int num_threads)
{
	std::unordered_multiset <int> survive = {};
	std::cout << "Checking Consistency : ";
	if (history[0].size() == 0) {
		std::cout << "No history.\n";
		return;
	}
	for (int i = 0; i < num_threads; ++i) {
		for (auto& op : history[i]) {
			if (op.op == 0) survive.insert(op.i_value);
		}
	}

	for (int i = 0; i < num_threads; ++i) {
		for (auto& op : history[i]) {
			if (op.op == 1) {
				if (op.o_value == -1) continue;
				auto it = survive.find(op.o_value);
				if (it == survive.end()) {
					std::cout << "ERROR. The value " << op.o_value << " dequeued while it is not in the queue.\n";
					exit(-1);
				}
				survive.erase(it);
			}
		}
	}

	std::queue<int> copy_queues[MAX_THREADS];

	for (int i = 0; i < num_threads; ++i)
		for (auto& op : history[i])
			if (op.op == 1)
				if (op.o_value != -1)
					copy_queues[i].push(op.o_value);

	std::queue<int> backup_queue;
	for (;;) {
		auto x = mt_queue.DEQ();
		if (x == -1) break;
		backup_queue.push(x);
		auto it = survive.find(x);
		if (it == survive.end()) {
			std::cout << "ERROR. The value " << x << " dequeued while it is not in the queue.\n";
			exit(-1);
		}
		survive.erase(it);
	}

	if (survive.size() > 0) {
		std::cout << "ERROR. The value " << *survive.begin() << " should not be in the queue.\n";
		exit(-1);
	}

	//std::unordered_multiset<int> counter;
	//int index[MAX_THREADS] = { 0 };
	//for (int i = 0; i < num_threads; ++i)
	//	counter.insert(0);

	//while (true) {
	//	bool all_empty = true;
	//	bool exists = false;

	//	for (int i = 0; i < num_threads; ++i) {
	//		if (copy_queues[i].empty() == false) {
	//			all_empty = false;
	//			int x = copy_queues[i].front();
	//			auto it = counter.find(x);
	//			if (it != counter.end()) {
	//				copy_queues[i].pop();
	//				counter.erase(it);
	//				counter.insert(x + 1);
	//				exists = true;
	//			}
	//		}
	//	}
	//	if ((all_empty == true) && (backup_queue.empty() == true))
	//		break;

	//	if (false == exists) {
	//		if (backup_queue.empty() == true) {
	//			std::cout << "There is missing number in queue!\n";
	//			exit(-1);
	//		}
	//		int x = backup_queue.front();
	//		backup_queue.pop();
	//		auto it = counter.find(x);
	//		if (it != counter.end()) {
	//			counter.erase(it);
	//			counter.insert(x + 1);
	//			exists = true;
	//		}
	//		else {
	//			std::cout << "There is missing number in queue!\n";
	//			exit(-1);
	//		}
	//	}
	//}
	for (int i = 0; i < num_threads; ++i) history[i].clear();
	std::cout << " OK\n";
}

void benchmark_check(int num_threads, int th_id)
{
	thread_id = th_id;
	int key = 0;
	const int LOOP = TASK / num_threads;
	for (int i = 0; i < LOOP; i++) {
		if ((i < 32) || (rand() % 2 == 0)) {
			history[th_id].emplace_back(0, key, -2);
			mt_queue.ENQ(key++);
		}
		else {
			history[th_id].emplace_back(1, -1, mt_queue.DEQ());
		}
	}
}



void benchmark(const int NUM_THREADS, int th_id)
{
	int key = 0;
	thread_id = th_id;

	for (int i = 0; i < TASK / NUM_THREADS; i++) {
		if (i < 2 || (0 == rand() % 2)) mt_queue.ENQ(key++);
		else mt_queue.DEQ();
	}

}

int main() {
	using namespace std::chrono;
	thread_id = 0;

	//std::cout << "Strting Error Check.\n";
	//for (num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
	//	std::vector<std::thread> threads;
	//	for (auto& h : history) h.clear();
	//	auto start_time = high_resolution_clock::now();
	//	for (int j = 0; j < num_threads; ++j) {
	//		threads.emplace_back(benchmark_check, num_threads, j);
	//	}
	//	for (auto& thread : threads) {
	//		thread.join();
	//	}
	//	auto end_time = high_resolution_clock::now();
	//	auto elapsed = end_time - start_time;
	//	auto exec_ms = duration_cast<milliseconds>(elapsed).count();
	//	std::cout << "Threads: " << num_threads << ", Time: " << exec_ms << " seconds\n";
	//	check_history(num_threads);
	//	mt_queue.Clear();
	//}

	std::cout << "\nStarting Benchmark\n";
	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			mt_queue.Clear();
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(benchmark, num_threads, i);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			mt_queue.Print();
			std::cout << num_threads << " Threads, QUEUE Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
		}
	}
}