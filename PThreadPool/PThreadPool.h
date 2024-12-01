#pragma once
#include "PThreadPool.h"
#include <pthread.h>
#include <string.h>//memset
#include <unistd.h>//sleep
#include <stdlib.h>//malloc
#include <stdio.h>//printf

struct ThreadPool;
//�����̳߳ز���ʼ��
ThreadPool * CreateThreadPool(int min, int max, int queueSize);

//�����̳߳�
int threadPoolDestroy(ThreadPool* pool);

//���̳߳��������
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg);

//��ȡ�̳߳��й������̸߳���
int threadPoolBusyNum(ThreadPool* pool);

//��ȡ�̳߳��л��ŵ��̸߳���
int threadPoolAliveNum(ThreadPool* pool);

//�����߳�
void* worker(void* arg);

//�������߳�
void* manager(void* arg);

//���˳���thread id��Ϊ0
void threadExit(ThreadPool* pool);