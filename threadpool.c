#include "threadpool.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const int NUMBER = 2;  // 添加一个全局常量，和宏定义类似
// 创建任务队列结构体
typedef struct Task {
    void (*function)(void* arg);  // 自定义函数指针指向哪一个函数
    void* arg;			  // 自定义的函数实参类型
} Task;

// 线程池结构体
typedef struct threadPool {
    // 任务队列
    Task* taskQ;
    int queueCapacity;	// 容量
    int queueSize;	// 当前任务个数
    int queueFront;	// 任务队列头-->取数据
    int queueRear;	// 任务队列尾-->放数据

    pthread_t managerID;  // 管理者线程ID
    pthread_t* threadIDs;  // 工作线程ID，由于有多个ID所以定义成一个指针类型
    int busyNum;		// 忙碌的线程个数
    int liveNum;		// 存活的线程个数
    int minNum;			// 最小的线程数量
    int maxNum;			// 最大的线程数量
    int exitNum;		// 要销毁的线程个数
    pthread_mutex_t mutexPool;	// 线程池的互斥锁，锁整个线程
    pthread_mutex_t
	mutexBusy;  // 忙碌的线程个数互斥锁，抢到这把锁的线程，防止在任务队列访问临界区的忙碌线程个数
    // 条件变量
    pthread_cond_t notFull;   // 任务队列是否已满
    pthread_cond_t notEmpty;  // 任务队列是否为空

    int shutdown;  // 是否销毁线程池，要销毁扣1，不要扣0
} threadPool;

// 对线程池的初始化
threadPool* threadPoolCreate(int min, int max, int queueSize) {
    threadPool* pool = (threadPool*)malloc(sizeof(threadPool));
    do {
	if (pool == NULL) {
	    printf("malloc threadpool faild...\n");
	    break;
	}

	// 对线程ID进行初始化
	pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);
	if (pool->threadIDs == NULL) {
	    printf("malloc threadIDs faild...\n");
	    break;
	}
	memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
	pool->minNum = min;
	pool->maxNum = max;
	pool->busyNum = 0;
	pool->liveNum = min;  // 和最小个数相等
	pool->exitNum = 0;

	// 对互斥锁和条件变量初始化
	if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
	    pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
	    pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
	    pthread_cond_init(&pool->notFull, NULL) != 0) {
	    printf("mutex or condition init faild...\n");
	    break;
	}

	// 任务队列的初始化
	pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
	pool->queueCapacity = queueSize;
	pool->queueSize = 0;
	pool->queueFront = 0;
	pool->queueRear = 0;

	pool->shutdown = 0;

	// 创建管理者和工作的线程
	pthread_create(&pool->managerID, NULL, manager, pool);
	for (int i = 0; i < min; ++i) {
	    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
	}
	return pool;
    } while (0);

    // 释放资源
    if (pool && pool->threadIDs) free(pool->threadIDs);
    if (pool && pool->taskQ) free(pool->taskQ);
    if (pool) free(pool);

    return NULL;
}

// 给线程池添加任务
void threadPoolAdd(threadPool* pool, void (*func)(void*), void* arg) {
    pthread_mutex_lock(&pool->mutexPool);

    // 判断当前队列是否已满
    while (pool->queueSize == pool->queueCapacity && pool->shutdown != 1) {
	// 阻塞消费者线程
	pthread_cond_wait(&pool->notFull, &pool->mutexPool);
    }
    if (pool->shutdown) {
	pthread_mutex_unlock(&pool->mutexPool);
	return;
    }
    // 添加任务
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    pool->queueSize++;

    // 有任务了，唤醒阻塞在工作函数中的线程
    pthread_cond_signal(&pool->notEmpty);
    pthread_mutex_unlock(&pool->mutexPool);
}

// 销毁线程池
int threadPoolDestroy(threadPool* pool) {
    // 判断地址池内是否还有有效数据，如果没有直接进行下一步销毁操作
    if (pool == NULL) {
	return -1;
    }
    // 关闭线程池
    pool->shutdown = 1;
    // 阻塞并回收管理者线程
    pthread_join(pool->managerID, NULL);
    printf("管理者线程已回收");
    for (int i = 0; i < pool->liveNum; ++i) {
	pthread_cond_signal(&pool->notEmpty);
    }
    // 释放堆内存
    if (pool->taskQ) {
	free(pool->taskQ);
    }
    if (pool->threadIDs) {
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

// 获取线程中工作的线程个数
int threadPoolBusyNum(threadPool* pool) {
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

// 获取线程中活着的线程个数
int threadPoolLiveNum(threadPool* pool) {
    pthread_mutex_lock(&pool->mutexPool);
    int liveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return liveNum;
}

void* worker(void* arg) {
    threadPool* pool = (threadPool*)arg;

    while (1) {
	pthread_mutex_lock(&pool->mutexPool);
	// 判断当前队列是否为空
	while (
	    pool->queueSize == 0 &&
	    pool->shutdown !=
		1)  // 当消费者消费后，该线程队列中这个位置就为空了，继续往下就会阻塞在下一语句了，这就是为什么要写while
	{
	    pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

	    // 判断是否要销毁线程
	    if (pool->exitNum > 0) {
		pool->exitNum--;
		if (pool->liveNum > pool->minNum) {
		    pool->liveNum--;
		    pthread_mutex_unlock(&pool->mutexPool);
		    threadExit(pool);
		}
	    }
	}
	// 判断线程池是否被关闭了
	if (pool->shutdown == 1) {
	    pthread_mutex_unlock(&pool->mutexPool);
	    threadExit(pool);
	}

	// 从任务队列中取出一个任务
	Task task;
	task.function = pool->taskQ[pool->queueFront].function;
	task.arg = pool->taskQ[pool->queueFront].arg;
	// 移动头结点
	pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
	pool->queueSize--;
	// 解锁
	pthread_cond_signal(&pool->notFull);
	pthread_mutex_unlock(&pool->mutexPool);

	// 任务函数和添加忙碌线程
	printf("thread %lu end working...\n", (unsigned long)pthread_self());
	pthread_mutex_lock(&pool->mutexBusy);
	pool->busyNum++;
	pthread_mutex_unlock(&pool->mutexBusy);
	task.function(task.arg);
	free(task.arg);
	task.arg = NULL;

	// 任务函数和线程创建和添加成功
	printf("thread %lu end working...\n", (unsigned long)pthread_self());
	pthread_mutex_lock(&pool->mutexBusy);
	pool->busyNum--;
	pthread_mutex_unlock(&pool->mutexBusy);
    }
    return NULL;
}

void* manager(void* arg) {
    threadPool* pool = (threadPool*)arg;
    while (pool->shutdown != 1) {
	// 每间隔3S检测一次
	sleep(3);

	// 取出线程池中的任务的数量和当前线程的数量
	// 取出之前先加上锁，防止再取出的时候别的线程正在写入
	pthread_mutex_lock(&pool->mutexPool);
	int queueSize = pool->queueSize;
	int liveNum = pool->liveNum;
	pthread_mutex_unlock(&pool->mutexPool);

	// 取出忙的线程个数
	pthread_mutex_lock(&pool->mutexBusy);
	int busyNum = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexBusy);

	// 添加线程
	// 任务的个数>存活的线程个数&&存活的线程数<最大线程数
	if (queueSize > liveNum - busyNum && liveNum < pool->maxNum) {
	    pthread_mutex_lock(&pool->mutexPool);
	    int counter = 0;
	    for (int i = 0; i < pool->maxNum && counter < NUMBER &&
			    pool->liveNum < pool->maxNum;
		 ++i) {
		if (pool->threadIDs[i] == 0) {
		    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
		    counter++;
		    pool->liveNum++;
		}
	    }
	    pthread_mutex_unlock(&pool->mutexPool);
	}
	// 销毁线程
	// 忙的线程*2<存活的线程数&&存活的线程数>最小线程个数
	if (pool->busyNum * 2 < pool->liveNum && pool->liveNum > pool->minNum) {
	    pthread_mutex_lock(&pool->mutexPool);
	    pool->exitNum = NUMBER;
	    pthread_mutex_unlock(&pool->mutexPool);
	    // 让工作的线程自杀
	    for (int i = 0; i < NUMBER; ++i) {
		pthread_cond_signal(&pool->notEmpty);
	    }
	}
    }
    return NULL;
}

void threadExit(threadPool* pool) {
    pthread_t tid = pthread_self();
    for (int i = 0; i < pool->maxNum; ++i) {
	if (pool->threadIDs[i] == tid) {
	    pool->threadIDs[i] = 0;
	    printf("threadExit() called,%lu exiting...\n", (unsigned long)tid);
	    break;
	}
    }
    pthread_exit(NULL);
}

