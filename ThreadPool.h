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
	//�����̳߳ز��ҳ�ʼ��
	ThreadPool<T>(int min, int max);

	//�����̳߳�
	~ThreadPool();

	//���̳߳��������
	void addTask(Task<T> task);

	//��ȡ�̳߳��й������̵߳ĸ���
	int getBusyNum();

	//��ȡ�̳߳��д�����̵߳ĸ���
	int getLiveNum();
private:
	//�����е��̣߳��������̣߳�������
	static void* worker(void* arg);

	//�������߳�������
	static void* manager(void* arg);

	//�����̵߳��˳�
	void threadExit();
private:
	//�������
	TaskQueue<T>* taskQ;

	pthread_t managerID;	//�������߳�ID
	pthread_t* threadIDs;	//�����߳�ID�������ж��ID���Զ����һ��ָ������
	int busyNum;			//æµ���̸߳���
	int liveNum;			//�����̸߳���
	int minNum;				//��С���߳�����
	int maxNum;				//�����߳�����
	int exitNum;			//Ҫ���ٵ��̸߳���
	pthread_mutex_t mutexPool;	//�̳߳صĻ��������������߳�
	pthread_mutex_t mutexOutput;	//���߳��˳�ʱ�ϵ�һ�����������ֹ�߳��˳�ʱ���Ի����������
	//��������
	pthread_cond_t	notEmpty;	//��������Ƿ�Ϊ��
	static const int NUMBER = 2;	//���һ��ȫ�ֳ������ͺ궨������

	bool shutdown;			//�Ƿ������̳߳أ�Ҫ���ٿ�1����Ҫ��0
};