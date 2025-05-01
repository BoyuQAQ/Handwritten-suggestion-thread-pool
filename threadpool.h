#ifndef _THREADPOOL_H
#define _THREADPOOL_H

//对poolthread.c源文件中各个函数以及线程池的初始化声明{

typedef struct threadPool threadPool;
//创建线程池并且初始化
threadPool* threadPoolCreate(int min, int max, int queueSize);

//销毁线程池
int threadPoolDestroy(threadPool* pool);

//给线程池添加任务
void threadPoolAdd(threadPool* pool, void(*func)(void*), void* arg);

//获取线程池中工作的线程的个数
int threadPoolBusyNum(threadPool* pool);

//获取线程池中存货的线程的个数
int threadPoolLiveNum(threadPool* pool);

//工作中的线程（消费者线程）任务函数
void* worker(void* arg);

//管理者线程任务函数
void* manager(void* arg);

//单个线程的退出
void threadExit(threadPool* pool);


#endif  //_THREADPOOL_H
