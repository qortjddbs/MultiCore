#include <thread>
#include <iostream>
#include <mutex>

constexpr int LOOP = 5000'0000;
int x, y;
int trace_x[LOOP], trace_y[LOOP];

void worker_x()
{
	for (int i = 0; i < LOOP; ++i)
	{
		x = i;
		std::atomic_thread_fence(std::memory_order_seq_cst);
		trace_y[i] = y;
	}
}

void worker_y()
{
	for (int i = 0; i < LOOP; ++i)
	{
		y = i;
		std::atomic_thread_fence(std::memory_order_seq_cst);
		trace_x[i] = x;
	}
}

int main() {
	//int x;
	//std::cin >> x;
	std::thread t1(worker_x);
	std::thread t2(worker_y);
	t1.join();
	t2.join();

	int inconsistency_count = 0;

	for (int i = 0; i < LOOP - 1; ++i)
	{
		if (trace_x[i] == trace_x[i + 1]) {
			int temp_x = trace_x[i];
			if (trace_y[temp_x] == trace_y[temp_x + 1])
				if (trace_y[temp_x] == i) {
					// std::cout << "Inconsistency found at x = " << temp_x << ", y = " << trace_y[temp_x] << std::endl;
					inconsistency_count++;
				}
		}
	}

	std::cout << "Inconsistency count: " << inconsistency_count << std::endl;
	return 0;
}