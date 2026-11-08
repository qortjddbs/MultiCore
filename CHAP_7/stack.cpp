#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
constexpr int MAX_THREADS = 32;
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
		std::lock_guard<std::mutex> lg(mtx);
		node->next = top;
		top = node;
	}
	int POP()
	{
		std::lock_guard<std::mutex> lg(mtx);
		if (top == nullptr) return -1;
		NODE* temp = top;
		int value = temp->value;
		top = top->next;
		delete temp;
		return value;
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