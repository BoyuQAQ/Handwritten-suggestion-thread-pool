#pragma once 
#include <queue>
#include <pthread.h>


using namespace std;
using callback = void (*)(void* arg);
//����ṹ��

template<class T>
struct Task
{
	//struct��C++��Ҳ�൱��һ���࣬���д��һ�����캯�����ڳ�ʼ��
	Task<T>()
	{
		function = nullptr;
		arg = nullptr;
	}
	Task<T>(callback f, void* arg)
	{
		this->arg =static_cast<T*>(arg);	//��ʾ����ת��
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

	//�������
	void addTask(Task<T> task);
	void addTask(callback f, void* arg);//Ϊ�˱���������������һ�����غ���
	//ȡ��һ������d
	Task<T> takeTask();
	//��ȡ��ǰ����ĸ���
	inline size_t taskNumber()
	{
		return m_taskQ.size();
	}

private:
	pthread_mutex_t	m_mutex;
	queue<Task<T>> m_taskQ;
};