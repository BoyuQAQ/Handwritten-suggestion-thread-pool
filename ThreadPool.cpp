#include "ThreadPool.h"
#include <unistd.h>
#include <pthread.h>

//���̳߳صĳ�ʼ��
template <class T>
ThreadPool<T>::ThreadPool(int min, int max)
{
	//ʵ�����������	
	do
	{
		taskQ = new TaskQueue<T>;
		if (taskQ == nullptr)
		{
			cout << ("malloc taskQ faild...") << endl;
			break;
		}
		//���߳�ID���г�ʼ��
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
		liveNum = min;	//����С�������
		exitNum = 0;

		//�Ի�����������������ʼ��
		if (pthread_mutex_init(&mutexPool, NULL) != 0 ||
			pthread_mutex_init(&mutexOutput, NULL) != 0 ||
			pthread_cond_init(&notEmpty, NULL) != 0 )
		{
			cout<<("mutex or condition init faild...")<<endl;
			break;
		}

		shutdown = false;

		//���������ߺ͹������߳�
		pthread_create(&managerID, NULL, manager, this);
		for (int i = 0; i < min; ++i)
		{
			pthread_create(&threadIDs[i], NULL, worker, this);
		}
		return;
	} while (0);

	//�ͷ���Դ
	if (threadIDs)delete[]threadIDs;
	if (taskQ)delete[]taskQ;
}

//���̳߳��������
template <class T>
void ThreadPool<T>::addTask(Task<T> task)
{
	if (shutdown)
	{
		return;
	}
	//�������
	taskQ->addTask(task);

	//�������ˣ����������ڹ��������е��߳�
	pthread_cond_signal(&notEmpty);
}

//�����̳߳�
template <class T>
ThreadPool<T>::~ThreadPool()
{	
	//�ر��̳߳�
	shutdown = true;
	//���������չ������߳�
	pthread_join(managerID, NULL);
	for (int i = 0; i < liveNum; ++i)
	{
		pthread_cond_signal(&notEmpty);
	}
	//�ͷŶ��ڴ�
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

//��ȡ�߳��й������̸߳���
template <class T>
int ThreadPool<T>::getBusyNum()
{
	pthread_mutex_lock(&mutexPool);
	int busyNum = this->busyNum;
	pthread_mutex_unlock(&mutexPool);
	return busyNum;
}

//��ȡ�߳��л��ŵ��̸߳���
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
		//�ж��̳߳��Ƿ񱻹ر���
		if (pool->shutdown == 1)
		{
			pthread_mutex_unlock(&pool->mutexPool);
			pool->threadExit();
		}

		//�жϵ�ǰ�����Ƿ�Ϊ��
		while (pool->taskQ->taskNumber() == 0 && pool->shutdown != 1)	//�����������Ѻ󣬸��̶߳��������λ�þ�Ϊ���ˣ��������¾ͻ���������һ����ˣ������ΪʲôҪдwhile
		{
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

			//�ж��Ƿ�Ҫ�����߳�
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

		//�ٴμ��shutdown
		if (pool->shutdown == 1)
		{
			pthread_mutex_unlock(&pool->mutexPool);
			pool->threadExit();
		}

		//�����������ȡ��һ������
		Task<T> task=pool->taskQ->takeTask();			
		//����
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexPool);

		//�����������æµ�߳�
		pthread_mutex_lock(&pool->mutexOutput);
		cout << "thread" <<to_string(pthread_self())<< "start working..." << endl;
		pthread_mutex_unlock(&pool->mutexOutput);		
		task.function(task.arg);
		delete task.arg;
		task.arg = nullptr;

		//���������̴߳�������ӳɹ�
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
		//ÿ���3S���һ��
		sleep(3);

		//ȡ���̳߳��е�����������͵�ǰ�̵߳�����
		//ȡ��֮ǰ�ȼ���������ֹ��ȡ����ʱ�����߳�����д��
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->taskQ->taskNumber();
		int liveNum = pool->liveNum;
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexPool);

		//ȡ��æ���̸߳���

		//����߳�
		//����ĸ���>�����̸߳���&&�����߳���<����߳���
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
		//�����߳�
		//æ���߳�*2<�����߳���&&�����߳���>��С�̸߳���
		if (pool->busyNum * 2 < pool->liveNum && pool->liveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			//�ù������߳���ɱ
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