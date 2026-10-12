#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <array>
#include <queue>

constexpr int MAX_THREADS = 64;
constexpr int TASK = 400'0000;
constexpr int RANGE = 1000;

class NODE {
public:
	int value;
	NODE* volatile next;
	volatile bool removed;
	std::mutex mtx;
	NODE(int v) : value(v), next(nullptr), removed(false) {}
	void Lock() { mtx.lock(); }
	void Unlock() { mtx.unlock(); }
};

class SH_NODE {
public:
	int value;
	std::atomic<std::shared_ptr<SH_NODE>> next;
	volatile bool removed;
	std::mutex mtx;
	SH_NODE(int v) : value(v), next(nullptr), removed(false) {}
	void Lock() { mtx.lock(); }
	void Unlock() { mtx.unlock(); }
};

class C_SET {
public:
	NODE* head, * tail;
	std::mutex ll;
	C_SET()
	{
		std::cout << "Creating Coarse-Grained SET\n";
		head = new NODE(std::numeric_limits<int>::min());
		tail = new NODE(std::numeric_limits<int>::max());
		head->next = tail;
	}
	bool Add(int v)
	{
		NODE* pred = head;
		ll.lock();
		NODE* curr = head->next;
		while (curr->value < v)
		{
			pred = curr;
			curr = curr->next;
		}
		if (curr->value == v) {
			ll.unlock();
			return false;
		}
		else
		{
			NODE* node = new NODE(v);
			node->next = curr;
			pred->next = node;
			ll.unlock();
			return true;
		}
	}
	bool Remove(int v)
	{
		NODE* pred = head;
		ll.lock();
		NODE* curr = head->next;
		while (curr->value < v)
		{
			pred = curr;
			curr = curr->next;
		}
		if (curr->value == v)
		{
			pred->next = curr->next;
			ll.unlock();
			delete curr;
			return true;
		}
		else {
			ll.unlock();
			return false;
		}
	}
	bool Contains(int v)
	{
		ll.lock();
		NODE* curr = head->next;
		while (curr->value < v)
			curr = curr->next;
		bool result = (curr->value == v);
		ll.unlock();
		return result;
	}
	void Clear()
	{
		NODE* curr = head->next;
		while (curr != tail)
		{
			NODE* temp = curr;
			curr = curr->next;
			delete temp;
		}
		head->next = tail;
	}
	void Print()
	{
		NODE* curr = head->next;
		for (int i = 0; i < 20 && curr != tail; i++)
		{
			std::cout << curr->value << ", ";
			curr = curr->next;
		}
		std::cout << std::endl;
	}
};

class F_SET {
public:
	NODE* head, * tail;
	F_SET()
	{
		std::cout << "Creating Fine-Grained SET\n";
		head = new NODE(std::numeric_limits<int>::min());
		tail = new NODE(std::numeric_limits<int>::max());
		head->next = tail;
	}
	bool Add(int v)
	{
		NODE* pred = head;
		pred->Lock();
		NODE* curr = head->next;
		curr->Lock();
		while (curr->value < v)
		{
			pred->Unlock();
			pred = curr;
			curr = curr->next;
			curr->Lock();
		}
		if (curr->value == v) {
			pred->Unlock(); curr->Unlock();
			return false;
		}
		else
		{
			NODE* node = new NODE(v);
			node->next = curr;
			pred->next = node;
			curr->Unlock(); pred->Unlock();
			return true;
		}
	}
	bool Remove(int v)
	{
		NODE* pred = head;
		pred->Lock();
		NODE* curr = head->next;
		curr->Lock();
		while (curr->value < v)
		{
			pred->Unlock();
			pred = curr;
			curr = curr->next;
			curr->Lock();
		}
		if (curr->value == v)
		{
			pred->next = curr->next;
			curr->Unlock(); pred->Unlock();
			delete curr;
			return true;
		}
		else {
			curr->Unlock(); pred->Unlock();
			return false;
		}
	}
	bool Contains(int v)
	{
		head->Lock();
		NODE* curr = head->next;
		curr->Lock();
		head->Unlock();
		while (curr->value < v) {
			auto temp = curr;
			curr = curr->next;
			curr->Lock();
			temp->Unlock();
		}
		bool result = (curr->value == v);
		curr->Unlock();
		return result;
	}
	void Clear()
	{
		NODE* curr = head->next;
		while (curr != tail)
		{
			NODE* temp = curr;
			curr = curr->next;
			delete temp;
		}
		head->next = tail;
	}
	void Print()
	{
		NODE* curr = head->next;
		for (int i = 0; i < 20 && curr != tail; i++)
		{
			std::cout << curr->value << ", ";
			curr = curr->next;
		}
		std::cout << std::endl;
	}
};

class MEMORY_POOL {
private:
	std::queue<NODE*> get_pool;
	std::queue<NODE*> free_pool;
public:
	MEMORY_POOL()
	{
		//for (int i = 0; i < TASK / 3; ++i)
		//	get_pool.push(new NODE(0));
	}
	~MEMORY_POOL() {
		while (!get_pool.empty()) {
			delete get_pool.front();
			get_pool.pop();
		}
		while (!free_pool.empty()) {
			delete free_pool.front();
			free_pool.pop();
		}
	}

	NODE* get_node(int value) {
		if (get_pool.empty())
			return new NODE(value);
		else {
			NODE* node = get_pool.front();
			get_pool.pop();
			node->value = value;
			node->next = nullptr;
			node->removed = false;
			return node;
		}
	}
	void free_node(NODE* node) {
		free_pool.push(node);
	}
	void recycle_nodes() {
		get_pool = std::move(free_pool);
	}
};

MEMORY_POOL memory_pool[MAX_THREADS];
thread_local int thread_id = 999;


class O_SET {
public:
	NODE* head, * tail;
	O_SET()
	{
		std::cout << "Creating Optimistic SET\n";
		head = new NODE(std::numeric_limits<int>::min());
		tail = new NODE(std::numeric_limits<int>::max());
		head->next = tail;
	}

	bool Validate(NODE* pred, NODE* curr)
	{
		NODE* node = head;
		while (node->value <= pred->value)
		{
			if (node == pred)
				return pred->next == curr;
			node = node->next;
		}
		return false;
	}

	bool Add(int v)
	{
		while (true) {
			NODE* pred = head;
			NODE* curr = head->next;
			while (curr->value < v)
			{
				pred = curr;
				curr = curr->next;
			}

			pred->Lock(); curr->Lock();
			if (true == Validate(pred, curr)) {
				if (curr->value == v) {
					pred->Unlock(); curr->Unlock();
					return false;
				}
				else
				{
					NODE* node = memory_pool[thread_id].get_node(v);
					node->next = curr;
					pred->next = node;
					curr->Unlock(); pred->Unlock();
					return true;
				}
			}
			pred->Unlock(); curr->Unlock();
		}
	}
	bool Remove(int v)
	{
		while (true) {
			NODE* pred = head;
			NODE* curr = head->next;
			while (curr->value < v)
			{
				pred = curr;
				curr = curr->next;
			}
			pred->Lock(); curr->Lock();
			if (true == Validate(pred, curr))
				if (curr->value == v) {
					pred->next = curr->next;
					curr->Unlock(); pred->Unlock();
					memory_pool[thread_id].free_node(curr);
					return true;
				}
				else {
					curr->Unlock(); pred->Unlock();
					return false;
				}
			pred->Unlock(); curr->Unlock();
		}
	}
	bool Contains(int v)
	{
		while (true) {
			NODE* pred = head;
			NODE* curr = head->next;
			while (curr->value < v)
			{
				pred = curr;
				curr = curr->next;
			}
			pred->Lock(); curr->Lock();
			if (true == Validate(pred, curr))
				if (curr->value == v) {
					curr->Unlock(); pred->Unlock();
					// delete curr;
					return true;
				}
				else {
					curr->Unlock(); pred->Unlock();
					return false;
				}
			pred->Unlock(); curr->Unlock();
		}
	}
	void Clear()
	{
		NODE* curr = head->next;
		while (curr != tail)
		{
			NODE* temp = curr;
			curr = curr->next;
			delete temp;
		}
		head->next = tail;
	}
	void Print()
	{
		NODE* curr = head->next;
		for (int i = 0; i < 20 && curr != tail; i++)
		{
			std::cout << curr->value << ", ";
			curr = curr->next;
		}
		std::cout << std::endl;
	}
};

class Z_SET {
public:
	NODE* head, * tail;
	Z_SET()
	{
		std::cout << "Creating Lazy SET\n";
		head = new NODE(std::numeric_limits<int>::min());
		tail = new NODE(std::numeric_limits<int>::max());
		head->next = tail;
	}

	bool Validate(NODE* pred, NODE* curr)
	{
		return (pred->removed == false) && 
			(curr->removed == false) && 
			pred->next == curr;
	}

	bool Add(int v)
	{
		while (true) {
			NODE* pred = head;
			NODE* curr = head->next;
			while (curr->value < v)
			{
				pred = curr;
				curr = curr->next;
			}

			pred->Lock(); curr->Lock();
			if (true == Validate(pred, curr)) {
				if (curr->value == v) {
					pred->Unlock(); curr->Unlock();
					return false;
				}
				else
				{
					NODE* node = memory_pool[thread_id].get_node(v);
					node->next = curr;
					pred->next = node;
					curr->Unlock(); pred->Unlock();
					return true;
				}
			}
			pred->Unlock(); curr->Unlock();
		}
	}
	bool Remove(int v)
	{
		while (true) {
			NODE* pred = head;
			NODE* curr = head->next;
			while (curr->value < v)
			{
				pred = curr;
				curr = curr->next;
			}
			pred->Lock(); curr->Lock();
			if (true == Validate(pred, curr))
				if (curr->value == v) {
					curr->removed = true;
					pred->next = curr->next;
					curr->Unlock(); pred->Unlock();
					memory_pool[thread_id].free_node(curr);
					return true;
				}
				else {
					curr->Unlock(); pred->Unlock();
					return false;
				}
			pred->Unlock(); curr->Unlock();
		}
	}
	bool Contains(int v)
	{
		while (true) {
			NODE* pred = head;
			NODE* curr = head->next;
			while (curr->value < v)
			{
				pred = curr;
				curr = curr->next;
			}
			pred->Lock(); curr->Lock();
			if (true == Validate(pred, curr))
				if (curr->value == v) {
					curr->Unlock(); pred->Unlock();
					return true;
				}
				else {
					curr->Unlock(); pred->Unlock();
					return false;
				}
			pred->Unlock(); curr->Unlock();
		}
	}
	void Clear()
	{
		NODE* curr = head->next;
		while (curr != tail)
		{
			NODE* temp = curr;
			curr = curr->next;
			delete temp;
		}
		head->next = tail;
	}
	void Print()
	{
		NODE* curr = head->next;
		for (int i = 0; i < 20 && curr != tail; i++)
		{
			std::cout << curr->value << ", ";
			curr = curr->next;
		}
		std::cout << std::endl;
	}
};

class SHZ_SET {
public:
	std::shared_ptr<SH_NODE> head, tail;
	SHZ_SET()
	{
		std::cout << "Creating C++20 Atomic Shared Pointer Lazy SET\n";
		head = std::make_shared<SH_NODE>(std::numeric_limits<int>::min());
		tail = std::make_shared<SH_NODE>(std::numeric_limits<int>::max());
		head->next = tail;
	}

	bool Validate(std::shared_ptr<SH_NODE> pred, std::shared_ptr<SH_NODE> curr)
	{
		return (pred->removed == false) &&
			(curr->removed == false) &&
			pred->next.load() == curr;
	}

	bool Add(int v)
	{
		while (true) {
			std::shared_ptr<SH_NODE> pred = head;
			std::shared_ptr<SH_NODE> curr = head->next;
			while (curr->value < v)
			{
				pred = curr;
				curr = curr->next;
			}

			pred->Lock(); curr->Lock();
			if (true == Validate(pred, curr)) {
				if (curr->value == v) {
					pred->Unlock(); curr->Unlock();
					return false;
				}
				else
				{
					std::shared_ptr<SH_NODE> node = std::make_shared<SH_NODE>(v);
					node->next = curr;
					pred->next = node;
					curr->Unlock(); pred->Unlock();
					return true;
				}
			}
			pred->Unlock(); curr->Unlock();
		}
	}
	bool Remove(int v)
	{
		while (true) {
			std::shared_ptr<SH_NODE> pred = head;
			std::shared_ptr<SH_NODE> curr = head->next;
			while (curr->value < v)
			{
				pred = curr;
				curr = curr->next;
			}
			pred->Lock(); curr->Lock();
			if (true == Validate(pred, curr))
				if (curr->value == v) {
					curr->removed = true;
					std::atomic_exchange(&pred->next, std::atomic_load(&curr->next));
					curr->Unlock(); pred->Unlock();
					return true;
				}
				else {
					curr->Unlock(); pred->Unlock();
					return false;
				}
			pred->Unlock(); curr->Unlock();
		}
	}
	bool Contains(int v)
	{
		while (true) {
			std::shared_ptr<SH_NODE> pred = head;
			std::shared_ptr<SH_NODE> curr = head->next;
			while (curr->value < v)
			{
				pred = curr;
				curr = curr->next;
			}
			pred->Lock(); curr->Lock();
			if (true == Validate(pred, curr))
				if (curr->value == v) {
					curr->Unlock(); pred->Unlock();
					return true;
				}
				else {
					curr->Unlock(); pred->Unlock();
					return false;
				}
			pred->Unlock(); curr->Unlock();
		}
	}

	void Clear()
	{
		head->next = tail;
	}

	void Print()
	{
		std::shared_ptr<SH_NODE> curr = head->next;
		for (int i = 0; i < 20 && curr != tail; i++)
		{
			std::cout << curr->value << ", ";
			curr = curr->next;
		}
		std::cout << std::endl;
	}
};

class LF_NODE;

class LFPTR {
public:
	volatile long long	value;
	void set_ptr(LF_NODE* ptr) {
		value = reinterpret_cast<long long>(ptr);
	}
	LF_NODE* get_ptr() {
		return reinterpret_cast<LF_NODE*>(value & ~1LL);
	}
	bool get_mark() {
		return (value & 1LL) != 0;
	}
	bool get_mark(LF_NODE*& ptr) {
		long long temp = value;			// temp를 만드는 이유 : 아래줄과 그 다음 줄에서 value를 두 번 접근하게 되는데 그 사이에 값이 바뀔 수 있기 때문
		ptr = reinterpret_cast<LF_NODE*>(temp & ~1LL);
		return (temp & 1LL) != 0;
	}
	bool CAS(LF_NODE* old_ptr, LF_NODE* new_ptr, bool old_mark, bool new_mark) {
		long long old_value = reinterpret_cast<long long>(old_ptr) | (old_mark ? 1 : 0);
		long long new_value = reinterpret_cast<long long>(new_ptr) | (new_mark ? 1 : 0);
		return std::atomic_compare_exchange_strong(reinterpret_cast<volatile std::atomic<long long>*>(&value), &old_value, new_value);
	}
};

class LF_NODE {
public:
	int value;
	LFPTR next;
	LF_NODE(int v) : value(v), next(0) {}
};

class LF_SET {
public:
	LF_NODE* head, * tail;
	LF_SET()
	{
		std::cout << "Creating Lock Free SET\n";
		head = new LF_NODE(std::numeric_limits<int>::min());
		tail = new LF_NODE(std::numeric_limits<int>::max());
		head->next.set_ptr(tail);
	}

	void Find(int v, LF_NODE*& pred, LF_NODE*& curr)
	{
		while (true) {
			retry:
			pred = head;
			curr = head->next.get_ptr();
			while (true) {
				LF_NODE* succ;
				bool removed = curr->next.get_mark(succ);
				while (true == removed) {
					if (false == pred->next.CAS(curr, succ, false, false)) goto retry;
					curr = succ;
					removed = curr->next.get_mark(succ);
				}
				if (curr->value >= v) return;
				pred = curr;
				curr = succ;
			}
			return;
		}
	}

	bool Add(int v)
	{
		while (true) {
			LF_NODE* pred;
			LF_NODE* curr;
			Find(v, pred, curr);

			if (curr->value == v) {
				return false;
			}
			else
			{
				LF_NODE* node = new LF_NODE(v);
				node->next.set_ptr(curr);			// 아직 비공개 -> set_ptr 가능
				if (true == pred->next.CAS(curr, node, false, false))
					return true;
				delete node;						// 같은 이유로 아직 비공개 -> delete 가능
			}
		}
	}
	bool Remove(int v)
	{
		while (true) {
			LF_NODE* pred; LF_NODE* curr;
			Find(v, pred, curr);
			if (curr->value != v) return false;
			LF_NODE* succ = curr->next.get_ptr();
			if (false == curr->next.CAS(succ, succ, false, true)) continue;
			pred->next.CAS(curr, succ, false, false);		// 실패해도 괜찮음. Find에서 대신 지워줌
			return true;
		}
	}
	bool Contains(int v)		// wait-free
	{
		LF_NODE* curr = head;
		while (curr->value < v)
			curr = curr->next.get_ptr();
		return (curr->value == v) && (false == curr->next.get_mark());
	}
	void Clear()
	{
		LF_NODE* curr = head->next.get_ptr();
		while (curr != tail)
		{
			LF_NODE* temp = curr;
			curr = curr->next.get_ptr();
			delete temp;
		}
		head->next.set_ptr(tail);
	}
	void Print()
	{
		LF_NODE* curr = head->next.get_ptr();
		for (int i = 0; i < 20 && curr != tail; i++)
		{
			std::cout << curr->value << ", ";
			curr = curr->next.get_ptr();
		}
		std::cout << std::endl;
	}
};

LF_SET mt_set;

class HISTORY {
public:
	int op;
	int i_value;
	bool o_value;
	HISTORY(int o, int i, bool re) : op(o), i_value(i), o_value(re) {}
};

std::array<std::vector<HISTORY>, MAX_THREADS> history;

void check_history(int num_threads)
{
	std::array <int, RANGE> survive = {};
	std::cout << "Checking Consistency : ";
	if (history[0].size() == 0) {
		std::cout << "No history.\n";
		return;
	}

	for (int i = 0; i < num_threads; ++i) {
		for (auto& op : history[i]) {
			if (false == op.o_value) continue;
			if (op.op == 3) continue;
			if (op.op == 0) survive[op.i_value]++;
			if (op.op == 1) survive[op.i_value]--;
		}
	}

	for (int i = 0; i < RANGE; ++i) {
		int val = survive[i];
		if (val < 0) {
			std::cout << "ERROR. The value " << i << " removed while it is not in the set.\n";
			exit(-1);
		}
		else if (val > 1) {
			std::cout << "ERROR. The value " << i << " is added while the set already have it.\n";
			exit(-1);
		}
		else if (val == 0) {
			if (mt_set.Contains(i)) {
				std::cout << "ERROR. The value " << i << " should not exists.\n";
				exit(-1);
			}
		}
		else if (val == 1) {
			if (false == mt_set.Contains(i)) {
				std::cout << "ERROR. The value " << i << " shoud exists.\n";
				exit(-1);
			}
		}
	}
	std::cout << num_threads << " Threads, History Check: OK\n";
}

void clear_history(int num_threads)
{
	for (int i = 0; i < num_threads; ++i) {
		history[i].clear();
	}
}

void benchmark_check(int num_threads, int th_id)
{
	thread_id = th_id;
	for (int i = 0; i < TASK / num_threads; ++i) {
		int op = rand() % 3;
		switch (op) {
		case 0: {
			int v = rand() % RANGE;
			history[th_id].emplace_back(0, v, mt_set.Add(v));
			break;
		}
		case 1: {
			int v = rand() % RANGE;
			history[th_id].emplace_back(1, v, mt_set.Remove(v));
			break;
		}
		case 2: {
			int v = rand() % RANGE;
			history[th_id].emplace_back(2, v, mt_set.Contains(v));
			break;
		}
		}
	}
}

void benchmark(const int NUM_THREADS, int th_id)
{
	int key;
	thread_id = th_id;

	for (int i = 0; i < TASK / NUM_THREADS; i++) {
		switch (rand() % 3) {
		case 0: key = rand() % RANGE;
			mt_set.Add(key);
			break;
		case 1: key = rand() % RANGE;
			mt_set.Remove(key);
			break;
		case 2: key = rand() % RANGE;
			mt_set.Contains(key);
			break;
		default: std::cout << "Error\n";
			exit(-1);
		}
	}
}

int main() {
	using namespace std::chrono;

	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			mt_set.Clear();
			clear_history(num_threads);
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(benchmark_check, num_threads, i);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			check_history(num_threads);
			for (int i = 0; i < num_threads; ++i)
				memory_pool[i].recycle_nodes();
		}
	}

	std::cout << "\nStarting Benchmark\n";

	{
		for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2)
		{
			mt_set.Clear();
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(benchmark, num_threads, i);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			mt_set.Print();
			std::cout << num_threads << " Threads, SET Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
			for (int i = 0; i < num_threads; ++i)
				memory_pool[i].recycle_nodes();
		}
	}
}