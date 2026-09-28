#include <thread>
#include <iostream>
#include <mutex>

std::mutex ll;

constexpr int LOOP = 2500'0000;
volatile int* bound;
volatile bool g_done = false;
int num_error = 0;

void thread1()
{
	for (int i = 0; i < LOOP; ++i)
	{
		*bound = -(1 + *bound);
	}
	g_done = true;
}

void thread2()
{
	while (false == g_done)
	{
		int v = *bound;				// *bound로 값 비교 안하는 이유 : and 연산을 하기 전과 후가 값이 다를 수 있기 때문
		if ((v !=0) && (v != -1))
		{
			printf("%X ", v);
			num_error++;
		}
	}
}

int main() {
	// int x;
	// std::cin >> x;

	int buffer[64] = { 0 };
	long long addr = reinterpret_cast<long long>(buffer + 32);
	addr = addr / 64 * 64;
	addr = addr - 1;
	bound = reinterpret_cast<int*>(addr);

	std::thread t1(thread1);
	std::thread t2(thread2);
	t1.join();
	t2.join();

	std::cout << "memory error: " << num_error << std::endl;
}