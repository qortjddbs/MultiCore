#include <iostream>
#include <thread>
#include <chrono>

const int MAX_THREADS = 8;
const int TOTAL = 500'0000;

int n;
volatile bool flag[MAX_THREADS];
volatile int label[MAX_THREADS];
volatile int sum;

// 슬라이드 15번째 줄 : (∃k != i)( flag[k] && (label[k],k) << (label[i],i) )
// => "나보다 우선순위가 높은 스레드가 하나라도 있는가?"
bool Exists_Higher_Priority(int i)
{
	for (int k = 0; k < n; ++k) {
		if (k == i) continue;
		if (flag[k] == false) continue;
		int lk = label[k];		// volatile 읽기는 한 번만
		int li = label[i];
		if (lk < li || (lk == li && k < i)) return true;	// (lk,k) << (li,i) : 사전식 비교
	}
	return false;
}

void Lock(int i)
{
	flag[i] = true;

	int max_label = 0;			// max(label[0], ..., label[n-1])
	for (int k = 0; k < n; ++k) {
		int l = label[k];
		if (l > max_label) max_label = l;
	}
	label[i] = max_label + 1;

	while (Exists_Higher_Priority(i)) {}
}

void Unlock(int i)
{
	flag[i] = false;
}

void thread_func(int thid)
{
	for (auto i = 0; i < TOTAL / n; ++i)
	{
		Lock(thid);
		sum = sum + 2;
		Unlock(thid);
	}
}

int main()
{
	using namespace std::chrono;

	for (n = 1; n <= MAX_THREADS; n *= 2) {
		// 이전 실행의 흔적 제거 (sum, flag, label)
		sum = 0;
		for (int k = 0; k < MAX_THREADS; ++k) {
			flag[k] = false;
			label[k] = 0;
		}

		std::thread threads[MAX_THREADS];
		auto start_t = high_resolution_clock::now();
		for (int t = 0; t < n; ++t)
			threads[t] = std::thread{ thread_func, t };
		for (int t = 0; t < n; ++t)
			threads[t].join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;

		std::cout << "Threads = " << n
			<< ", Sum = " << sum
			<< ", Time = " << duration_cast<milliseconds>(exec_t).count() << " ms"
			<< std::endl;
	}
}
