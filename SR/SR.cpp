#include <thread>
#include <iostream>
#include <mutex>

volatile int g_data = 0;
volatile bool g_ready = false;
std::mutex ll;

void receiver()
{
	while (false == g_ready);
	std::cout << "Received data : " << g_data << std::endl;
}

void sender()
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
	g_data = 42;
	g_ready = true;
}

int main()
{
	//while (false == g_ready);
	//std::cout << "BUG\n";
	std::thread t1(receiver);
	std::thread t2(sender);
	t1.join();
	t2.join();
}