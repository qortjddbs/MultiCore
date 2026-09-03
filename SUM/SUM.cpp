#include <iostream>
#include <thread>
#include <mutex>

int sum;
std::mutex ll;

void worker()
{
	for (int i = 0; i < 2500'0000; ++i)
	{
		ll.lock();
		sum = sum + 2;
		ll.unlock();
	}
}

int main()
{
	std::thread t1(worker);
	std::thread t2(worker);
	t1.join();
	t2.join();
	std::cout << "SUM : " << sum << std::endl;
}