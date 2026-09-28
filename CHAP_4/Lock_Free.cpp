#include <iostream>
#include <atomic>
#include <queue>

class POINT { int x, y; };
class POINT3D { int x, y, z; };
int main()
{
	std::cout << std::boolalpha;

	std::atomic <int > atomicInt(0);
	std::cout << " atomic <int > is lock free : " << atomicInt.is_lock_free() << std::endl;

	std::atomic <POINT > point;
	std::cout << " atomic <POINT> is lock free : " << point.is_lock_free() << std::endl;

	std::atomic <POINT3D > point3d;
	std::cout << " atomic <POINT3D> is lock free : " << point3d.is_lock_free() << std::endl;

	std::atomic <std::queue <int >> int_queue;
	std::cout << " atomic <std :: queue <int >> is lock free : " << int_queue.is_lock_free() << std::endl;
}