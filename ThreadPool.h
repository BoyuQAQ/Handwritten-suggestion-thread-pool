#pragma once 
#include "TaskQueue.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <pthread.h>
using namespace std;
#include <iostream>

template<class T>
class ThreadPool
{
public:
	//创建线程池并且初始化
	ThreadPool<T>(int min, int max);

	//销毁线程池
	~ThreadPool();

	//给线程池添加任务
	void addTask(Task<T> task);

	//获取线程池中工作的线程的个数
	int getBusyNum();

	//获取线程池中存货的线程的个数
	int getLiveNum();
private:
	//工作中的线程（消费者线程）任务函数
	static void* worker(void* arg);

	//管理者线程任务函数
	static void* manager(void* arg);

	//单个线程的退出
	void threadExit();
private:
	//任务队列
	TaskQueue<T>* taskQ;

	pthread_t managerID;	//管理者线程ID
	pthread_t* threadIDs;	//工作线程ID，由于有多个ID所以定义成一个指针类型
	int busyNum;			//忙碌的线程个数
	int liveNum;			//存活的线程个数
	int minNum;				//最小的线程数量
	int maxNum;				//最大的线程数量
	int exitNum;			//要销毁的线程个数
	pthread_mutex_t mutexPool;	//线程池的互斥锁，锁整个线程
	pthread_mutex_t mutexOutput;	//在线程退出时上的一把输出锁，防止线程退出时候仍会有输出竞争
	//条件变量
	pthread_cond_t	notEmpty;	//任务队列是否为空
	static const int NUMBER = 2;	//添加一个全局常量，和宏定义类似

	bool shutdown;			//是否销毁线程池，要销毁扣1，不要扣0
};