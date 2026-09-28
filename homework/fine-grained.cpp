#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>

constexpr int MAX_THREADS = 16;

class NODE {
public:
	int value;
	NODE* next;
	std::mutex mtx;
	NODE(int v) : value(v), next(nullptr) {}
	void Lock() { mtx.lock(); }
	void Unlock() { mtx.unlock(); }
};

class C_SET {
public:
	NODE* head, * tail;
	C_SET()
	{
		std::cout << "Creating Fine-Grained SET\n\n";
		head = new NODE(std::numeric_limits<int>::min());
		tail = new NODE(std::numeric_limits<int>::max());
		head->next = tail;
	}
	bool Add(int v)
	{
		head->Lock();
		NODE* pred = head;
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
			curr->Unlock();
			pred->Unlock();
			return false;
		}
		else
		{
			NODE* node = new NODE(v);
			node->next = curr;
			pred->next = node;
			curr->Unlock();
			pred->Unlock();
			return true;
		}
	}
	bool Remove(int v)
	{
		head->Lock();
		NODE* pred = head;
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
			curr->Unlock();
			delete curr;
			pred->Unlock();
			return true;
		}
		else {
			curr->Unlock();
			pred->Unlock();
			return false;
		}
	}
	bool Contains(int v)
	{
		head->Lock();
		NODE* pred = head;
		NODE* curr = head->next;
		curr->Lock();
		while (curr->value < v) {
			pred->Unlock();
			pred = curr;
			curr = curr->next;
			curr->Lock();
		}
			
		bool result = (curr->value == v);

		curr->Unlock();
		pred->Unlock();
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

constexpr int TASK = 400'0000;
constexpr int RANGE = 1000;


C_SET mt_set;

void benchmark(const int NUM_THREADS)
{
	int key;

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
			auto start_t = high_resolution_clock::now();
			std::thread threads[MAX_THREADS];
			for (int i = 0; i < num_threads; ++i)
				threads[i] = std::thread(benchmark, num_threads);
			for (int i = 0; i < num_threads; ++i)
				threads[i].join();
			auto end_t = high_resolution_clock::now();
			auto exec_t = end_t - start_t;
			mt_set.Print();
			std::cout << num_threads << " Threads, SET Execution time: " << duration_cast<milliseconds>(exec_t).count() << " ms\n\n";
		}
	}
}