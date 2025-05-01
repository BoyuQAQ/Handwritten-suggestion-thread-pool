#ifndef _THREADPOOL_H
#define _THREADPOOL_H

//��poolthread.cԴ�ļ��и��������Լ��̳߳صĳ�ʼ������{

typedef struct threadPool threadPool;
//�����̳߳ز��ҳ�ʼ��
threadPool* threadPoolCreate(int min, int max, int queueSize);

//�����̳߳�
int threadPoolDestroy(threadPool* pool);

//���̳߳��������
void threadPoolAdd(threadPool* pool, void(*func)(void*), void* arg);

//��ȡ�̳߳��й������̵߳ĸ���
int threadPoolBusyNum(threadPool* pool);

//��ȡ�̳߳��д�����̵߳ĸ���
int threadPoolLiveNum(threadPool* pool);

//�����е��̣߳��������̣߳�������
void* worker(void* arg);

//�������߳�������
void* manager(void* arg);

//�����̵߳��˳�
void threadExit(threadPool* pool);


#endif  //_THREADPOOL_H
