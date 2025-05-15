#include "ThreadPool.h"
#include <unistd.h>
#include <pthread.h>

//对线程池的初始化
template <class T>
ThreadPool<T>::ThreadPool(int min, int max)
{
	//实例化任务队列	
	do
	{
		taskQ = new TaskQueue<T>;
		if (taskQ == nullptr)
		{
			cout << ("malloc taskQ faild...") << endl;
			break;
		}
		//对线程ID进行初始化
		threadIDs = new pthread_t[max];
		if (threadIDs ==nullptr)
		{
			cout << ("malloc threadIDs faild...") << endl;
			break;
		}
		memset(threadIDs, 0, sizeof(pthread_t) * max);
		minNum = min;
		maxNum = max;
		busyNum = 0;
		liveNum = min;	//和最小个数相等
		exitNum = 0;

		//对互斥锁和条件变量初始化
		if (pthread_mutex_init(&mutexPool, NULL) != 0 ||
			pthread_mutex_init(&mutexOutput, NULL) != 0 ||
			pthread_cond_init(&notEmpty, NULL) != 0 )
		{
			cout<<("mutex or condition init faild...")<<endl;
			break;
		}

		shutdown = false;

		//创建管理者和工作的线程
		pthread_create(&managerID, NULL, manager, this);
		for (int i = 0; i < min; ++i)
		{
			pthread_create(&threadIDs[i], NULL, worker, this);
		}
		return;
	} while (0);

	//释放资源
	if (threadIDs)delete[]threadIDs;
	if (taskQ)delete[]taskQ;
}

//给线程池添加任务
template <class T>
void ThreadPool<T>::addTask(Task<T> task)
{
	if (shutdown)
	{
		return;
	}
	//添加任务
	taskQ->addTask(task);

	//有任务了，唤醒阻塞在工作函数中的线程
	pthread_cond_signal(&notEmpty);
}

//销毁线程池
template <class T>
ThreadPool<T>::~ThreadPool()
{	
	//关闭线程池
	shutdown = true;
	//阻塞并回收管理者线程
	pthread_join(managerID, NULL);
	for (int i = 0; i < liveNum; ++i)
	{
		pthread_cond_signal(&notEmpty);
	}
	//释放堆内存
	if (taskQ)
	{
		delete taskQ;
	}
	if (threadIDs)
	{
		delete []threadIDs;
	}

	pthread_mutex_destroy(&mutexPool);
	pthread_cond_destroy(&notEmpty);
}

//获取线程中工作的线程个数
template <class T>
int ThreadPool<T>::getBusyNum()
{
	pthread_mutex_lock(&mutexPool);
	int busyNum = this->busyNum;
	pthread_mutex_unlock(&mutexPool);
	return busyNum;
}

//获取线程中活着的线程个数
template <class T>
int ThreadPool<T>::getLiveNum()
{
	pthread_mutex_lock(&mutexPool);
	int liveNum = this->liveNum;
	pthread_mutex_unlock(&mutexPool);
	return liveNum;
}

template <class T>
void* ThreadPool<T>::worker(void* arg)
{
	ThreadPool* pool = static_cast<ThreadPool*>(arg);

	while (1)
	{
		pthread_mutex_lock(&pool->mutexPool);
		//判断线程池是否被关闭了
		if (pool->shutdown == 1)
		{
			pthread_mutex_unlock(&pool->mutexPool);
			pool->threadExit();
		}

		//判断当前队列是否为空
		while (pool->taskQ->taskNumber() == 0 && pool->shutdown != 1)	//当消费者消费后，该线程队列中这个位置就为空了，继续往下就会阻塞在下一语句了，这就是为什么要写while
		{
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

			//判断是否要销毁线程
			if (pool->exitNum > 0)
			{
				pool->exitNum--;
				if (pool->liveNum > pool->minNum)
				{
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutexPool);
					pool->threadExit();
				}
			}
		}

		//再次检查shutdown
		if (pool->shutdown == 1)
		{
			pthread_mutex_unlock(&pool->mutexPool);
			pool->threadExit();
		}

		//从任务队列中取出一个任务
		Task<T> task=pool->taskQ->takeTask();			
		//解锁
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexPool);

		//任务函数和添加忙碌线程
		pthread_mutex_lock(&pool->mutexOutput);
		cout << "thread" <<to_string(pthread_self())<< "start working..." << endl;
		pthread_mutex_unlock(&pool->mutexOutput);		
		task.function(task.arg);
		delete task.arg;
		task.arg = nullptr;

		//任务函数和线程创建和添加成功
		pthread_mutex_lock(&pool->mutexOutput);
		cout<<"thread " <<to_string(pthread_self())<< "end working..."<<endl;
		pthread_mutex_unlock(&pool->mutexOutput);
		pthread_mutex_lock(&pool->mutexPool);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexPool);
	}
	return NULL;
}

template <class T>
void* ThreadPool<T>::manager(void *arg)
{
	ThreadPool* pool = static_cast<ThreadPool*>(arg);
	while (pool->shutdown != 1)
	{
		//每间隔3S检测一次
		sleep(3);

		//取出线程池中的任务的数量和当前线程的数量
		//取出之前先加上锁，防止再取出的时候别的线程正在写入
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->taskQ->taskNumber();
		int liveNum = pool->liveNum;
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexPool);

		//取出忙的线程个数

		//添加线程
		//任务的个数>存活的线程个数&&存活的线程数<最大线程数
		if (queueSize > liveNum && liveNum < pool->maxNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			int counter = 0;
			for (int i = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; ++i)
			{
				if (pool->threadIDs[i] == 0)
				{
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					counter++;
					pool->liveNum++;
				}
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}
		//销毁线程
		//忙的线程*2<存活的线程数&&存活的线程数>最小线程个数
		if (pool->busyNum * 2 < pool->liveNum && pool->liveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			//让工作的线程自杀
			for (int i = 0; i < NUMBER; ++i)
			{
				pthread_cond_signal(&pool->notEmpty);
			}
		}
	}
	return NULL;
}

template <class T>
void ThreadPool<T>::threadExit()
{
	pthread_t tid = pthread_self();
	for (int i = 0; i < maxNum; ++i)
	{
		if (threadIDs[i] == tid)
		{
			threadIDs[i] = 0;
			cout << "threadExit() called,<<to_string(tid) exiting..."<<endl;
			break;
		}
	}
	pthread_exit(NULL);
}