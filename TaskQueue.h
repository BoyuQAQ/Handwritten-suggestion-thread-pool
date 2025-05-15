#pragma once 
#include <queue>
#include <pthread.h>


using namespace std;
using callback = void (*)(void* arg);
//任务结构体

template<class T>
struct Task
{
	//struct在C++中也相当于一个类，因此写了一个构造函数用于初始化
	Task<T>()
	{
		function = nullptr;
		arg = nullptr;
	}
	Task<T>(callback f, void* arg)
	{
		this->arg =static_cast<T*>(arg);	//显示类型转换
		function = f;
	}
	callback function;
	T* arg;
};

template<class T>
class TaskQueue
{
public:
	TaskQueue();
	~TaskQueue();

	//添加任务
	void addTask(Task<T> task);
	void addTask(callback f, void* arg);//为了编译操作，可以添加一个重载函数
	//取出一个任务d
	Task<T> takeTask();
	//获取当前任务的个数
	inline size_t taskNumber()
	{
		return m_taskQ.size();
	}

private:
	pthread_mutex_t	m_mutex;
	queue<Task<T>> m_taskQ;
};