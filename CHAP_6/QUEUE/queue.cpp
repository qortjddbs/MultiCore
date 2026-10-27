#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
constexpr int MAX_THREADS = 16;
constexpr int TASK = 1000'0000;

class NODE {
public:
	int value;
	NODE*  next;
	NODE(int v) : value(v), next(nullptr) {}
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
		std::lock_guard<std::mutex> lg(enq_m);
		NODE* node = new NODE(v);
		tail->next = node;
		tail = node;
	}
	int DEQ()
	{
		std::lock_guard<std::mutex> lg(enq_m);
		if (head->next == nullptr) return -1;
		int value = head->next->value;
		NODE* temp = head;
		head = head->next;           
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

thread_local int thread_id = 999;

C_QUEUE mt_queue;

void benchmark(const int NUM_THREADS, int th_id)
{
	int key = 0;
	thread_id = th_id;

	for (int i = 0; i < TASK / NUM_THREADS; i++) {
		if (i < 32 || (0 == rand() % 2)) mt_queue.ENQ(key++);
		else mt_queue.DEQ();
	}

}

int main() {
	using namespace std::chrono;


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