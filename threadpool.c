#include "threadpool.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

const int NUMBER = 2;	//���һ��ȫ�ֳ������ͺ궨������
//����������нṹ��
typedef struct Task
{
	void (*function)(void* arg);	//�Զ��庯��ָ��ָ����һ������
	void* arg;						//�Զ���ĺ���ʵ������
}Task;

//�̳߳ؽṹ��
typedef struct threadPool
{
	//�������
	Task* taskQ;
	int queueCapacity;	//����
	int queueSize;		//��ǰ�������
	int queueFront;		//�������ͷ-->ȡ����
	int queueRear;		//�������β-->������

	pthread_t managerID;	//�������߳�ID
	pthread_t* threadIDs;	//�����߳�ID�������ж��ID���Զ����һ��ָ������
	int busyNum;			//æµ���̸߳���
	int liveNum;			//�����̸߳���
	int minNum;				//��С���߳�����
	int maxNum;				//�����߳�����
	int exitNum;			//Ҫ���ٵ��̸߳���
	pthread_mutex_t mutexPool;	//�̳߳صĻ��������������߳�
	pthread_mutex_t mutexBusy;	//æµ���̸߳�����������������������̣߳���ֹ��������з����ٽ�����æµ�̸߳���
	pthread_mutex_t mutexOutput;	//���߳��˳�ʱ�ϵ�һ�����������ֹ�߳��˳�ʱ���Ի����������
	//��������
	pthread_cond_t	notFull;	//��������Ƿ�����
	pthread_cond_t	notEmpty;	//��������Ƿ�Ϊ��

	int shutdown;			//�Ƿ������̳߳أ�Ҫ���ٿ�1����Ҫ��0
}threadPool;

//���̳߳صĳ�ʼ��
threadPool* threadPoolCreate(int min, int max, int queueSize)
{
	threadPool* pool = (threadPool*)malloc(sizeof(threadPool));
	do
	{
		if (pool == NULL)
		{
			printf("malloc threadpool faild...\n");
			break;
		}

		//���߳�ID���г�ʼ��
		pool->threadIDs = (threadPool*)malloc(sizeof(pthread_t) * max);
		if (pool->threadIDs == NULL)
		{
			printf("malloc threadIDs faild...\n");
			break;
		}
		memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0;
		pool->liveNum = min;	//����С�������
		pool->exitNum = 0;

		//�Ի�����������������ʼ��
		if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexOutput, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0)

		{
			printf("mutex or condition init faild...\n");
			break;
		}

		//������еĳ�ʼ��
		pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueRear = 0;

		pool->shutdown = 0;

		//���������ߺ͹������߳�
		pthread_create(&pool->managerID, NULL, manager, pool);
		for (int i = 0; i < min; ++i)
		{
			pthread_create(&pool->threadIDs[i], NULL, worker, pool);
		}
		return pool;
	} while (0);

	//�ͷ���Դ
	if (pool && pool->threadIDs)free(pool->threadIDs);
	if (pool && pool->taskQ)free(pool->taskQ);
	if (pool)free(pool);

	return NULL;
}

//���̳߳��������
void threadPoolAdd(threadPool* pool, void (*func)(void*), void* arg)
{
	pthread_mutex_lock(&pool->mutexPool);

	//�жϵ�ǰ�����Ƿ�����
	while (pool->queueSize == pool->queueCapacity && pool->shutdown != 1)
	{
		//�����������߳�
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}
	if (pool->shutdown)
	{
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}
	//�������
	pool->taskQ[pool->queueRear].function = func;
	pool->taskQ[pool->queueRear].arg = arg;
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queueSize++;

	//�������ˣ����������ڹ��������е��߳�
	pthread_cond_signal(&pool->notEmpty);
	pthread_mutex_unlock(&pool->mutexPool);
}

//�����̳߳�
int threadPoolDestroy(threadPool* pool)
{
	pthread_mutex_lock(&pool->mutexOutput);
	printf("���������̳߳�");
	pthread_mutex_unlock(&pool->mutexOutput);
	//�жϵ�ַ�����Ƿ�����Ч���ݣ����û��ֱ�ӽ�����һ�����ٲ���
	if (pool == NULL)
	{
		return -1;
	}
	//�ر��̳߳�
	pool->shutdown = 1;
	//���������չ������߳�
	pthread_join(pool->managerID, NULL);
	pthread_mutex_lock(&pool->mutexOutput);
	printf("�������߳��ѻ���");
	pthread_mutex_unlock(&pool->mutexOutput);
	for (int i = 0; i < pool->liveNum; ++i)
	{
		pthread_cond_signal(&pool->notEmpty);
	}
	//�ͷŶ��ڴ�
	if (pool->taskQ)
	{
		free(pool->taskQ);
	}
	if (pool->threadIDs)
	{
		free(pool->threadIDs);
	}

	pthread_mutex_destroy(&pool->mutexPool);
	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_cond_destroy(&pool->notFull);
	pthread_cond_destroy(&pool->notEmpty);

	free(pool);
	pool = NULL;

	return 0;
}

//��ȡ�߳��й������̸߳���
int threadPoolBusyNum(threadPool* pool)
{
	pthread_mutex_lock(&pool->mutexBusy);
	int busyNum = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexBusy);
	return busyNum;
}

//��ȡ�߳��л��ŵ��̸߳���
int threadPoolLiveNum(threadPool* pool)
{
	pthread_mutex_lock(&pool->mutexPool);
	int liveNum = pool->liveNum;
	pthread_mutex_unlock(&pool->mutexPool);
	return liveNum;
}

void* worker(void* arg)
{
	threadPool* pool = (threadPool*)arg;

	while (1)
	{
		pthread_mutex_lock(&pool->mutexPool);
		//�ж��̳߳��Ƿ񱻹ر���
		if (pool->shutdown == 1)
		{
			pthread_mutex_unlock(&pool->mutexPool);
			threadExit(pool);
		}

		//�жϵ�ǰ�����Ƿ�Ϊ��
		while (pool->queueSize == 0 && pool->shutdown != 1)	//�����������Ѻ󣬸��̶߳��������λ�þ�Ϊ���ˣ��������¾ͻ���������һ����ˣ������ΪʲôҪдwhile
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
					threadExit(pool);
				}
			}
		}

		//�ٴμ��shutdown
		if (pool->shutdown == 1)
		{
			pthread_mutex_unlock(&pool->mutexPool);
			threadExit(pool);
		}

		//�����������ȡ��һ������
		Task task;
		task.function = pool->taskQ[pool->queueFront].function;
		task.arg = pool->taskQ[pool->queueFront].arg;
		//�ƶ�ͷ��� 
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		pool->queueSize--;
		//����
		pthread_cond_signal(&pool->notFull);
		pthread_mutex_unlock(&pool->mutexPool);

		//�����������æµ�߳�
		pthread_mutex_lock(&pool->mutexOutput);
		printf("thread %ld start working...\n", pthread_self());
		pthread_mutex_unlock(&pool->mutexOutput);
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);
		task.function(task.arg);
		//free(task.arg);
		task.arg = NULL;

		//���������̴߳�������ӳɹ�
		pthread_mutex_lock(&pool->mutexOutput);
		printf("thread %ld end working...\n", pthread_self());
		pthread_mutex_unlock(&pool->mutexOutput);
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
	}
	return NULL;
}

void* manager(void* arg)
{
	threadPool* pool = (threadPool*)arg;
	while (pool->shutdown != 1)
	{
		//ÿ���3S���һ��
		sleep(3);

		//ȡ���̳߳��е�����������͵�ǰ�̵߳�����
		//ȡ��֮ǰ�ȼ���������ֹ��ȡ����ʱ�����߳�����д��
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		//ȡ��æ���̸߳���
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

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

void threadExit(threadPool* pool)
{
	pthread_t tid = pthread_self();
	for (int i = 0; i < pool->maxNum; ++i)
	{
		if (pool->threadIDs[i] == tid)
		{
			pool->threadIDs[i] = 0;
			printf("threadExit() called,%ld exiting...\n", tid);
			break;
		}
	}
	pthread_exit(NULL);
}

