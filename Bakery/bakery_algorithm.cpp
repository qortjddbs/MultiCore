#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

const int MAX_THREADS = 8;
const int TOTAL = 5000'0000;	// 전체 반복 횟수 (한 번에 +2 => 최종 1억). 빠른 테스트는 500'0000 (=> 1천만)

int n;							// 이번 실행의 스레드 개수 (스레드 생성 전에 main에서 설정)
volatile int sum;				// 4개 버전 공통. 한 번에 한 버전만 실행하고, 실행 전에 0으로 리셋
std::mutex mtx;

// ---------------------------------------------------------------
// 빵집 알고리즘 : volatile 버전 (슬라이드 2-29)
// ---------------------------------------------------------------
volatile bool flag[MAX_THREADS];
volatile int label[MAX_THREADS];

// 슬라이드 15번째 줄 : (∃k != i)( flag[k] && (label[k],k) << (label[i],i) )
bool Exists_Higher_Priority(int i)
{
	for (int k = 0; k < n; ++k) {
		if (k == i) continue;
		if (flag[k] == false) continue;
		int lk = label[k];
		int li = label[i];
		if (lk < li || (lk == li && k < i)) return true;
	}
	return false;
}

void Lock(int i)
{
	flag[i] = true;

	int max_label = 0;
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

// ---------------------------------------------------------------
// 빵집 알고리즘 : atomic 버전 (위 코드에서 volatile을 atomic<T>로 대체한 것 외에는 동일)
// ---------------------------------------------------------------
std::atomic<bool> a_flag[MAX_THREADS];
std::atomic<int> a_label[MAX_THREADS];

bool Exists_Higher_Priority_atomic(int i)
{
	for (int k = 0; k < n; ++k) {
		if (k == i) continue;
		if (a_flag[k] == false) continue;
		int lk = a_label[k];
		int li = a_label[i];
		if (lk < li || (lk == li && k < i)) return true;
	}
	return false;
}

void Lock_atomic(int i)
{
	a_flag[i] = true;

	int max_label = 0;
	for (int k = 0; k < n; ++k) {
		int l = a_label[k];
		if (l > max_label) max_label = l;
	}
	a_label[i] = max_label + 1;

	while (Exists_Higher_Priority_atomic(i)) {}
}

void Unlock_atomic(int i)
{
	a_flag[i] = false;
}

// ---------------------------------------------------------------
// 스레드 함수 4종 : 각 스레드가 TOTAL / n 번 sum += 2
// ---------------------------------------------------------------
void thread_func_nolock(int)
{
	for (auto i = 0; i < TOTAL / n; ++i)
		sum = sum + 2;
}

void thread_func_mutex(int)
{
	for (auto i = 0; i < TOTAL / n; ++i) {
		mtx.lock();
		sum = sum + 2;
		mtx.unlock();
	}
}

void thread_func_bakery_volatile(int thid)
{
	for (auto i = 0; i < TOTAL / n; ++i) {
		Lock(thid);
		sum = sum + 2;
		Unlock(thid);
	}
}

void thread_func_bakery_atomic(int thid)
{
	for (auto i = 0; i < TOTAL / n; ++i) {
		Lock_atomic(thid);
		sum = sum + 2;
		Unlock_atomic(thid);
	}
}

// ---------------------------------------------------------------
// 스레드 1, 2, 4, 8개로 실행하고 결과/시간 출력
// ---------------------------------------------------------------
void run(const char* name, void (*func)(int))
{
	using namespace std::chrono;

	for (n = 1; n <= MAX_THREADS; n *= 2) {
		sum = 0;
		for (int k = 0; k < MAX_THREADS; ++k) {
			flag[k] = false;
			label[k] = 0;
			a_flag[k] = false;
			a_label[k] = 0;
		}

		std::thread threads[MAX_THREADS];
		auto start_t = high_resolution_clock::now();
		for (int t = 0; t < n; ++t)
			threads[t] = std::thread{ func, t };
		for (int t = 0; t < n; ++t)
			threads[t].join();
		auto end_t = high_resolution_clock::now();

		std::cout << name << " | Threads = " << n
			<< ", Sum = " << sum
			<< ", Time = " << duration_cast<milliseconds>(end_t - start_t).count() << " ms"
			<< std::endl;
	}
}

int main()
{
	run("No Lock        ", thread_func_nolock);
	run("Mutex          ", thread_func_mutex);
	run("Bakery volatile", thread_func_bakery_volatile);
	run("Bakery atomic  ", thread_func_bakery_atomic);
}