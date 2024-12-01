#pragma once
#include "PThreadPool.h"
#include <pthread.h>
#include <string.h>//memset
#include <unistd.h>//sleep
#include <stdlib.h>//malloc
#include <stdio.h>//printf

struct ThreadPool;
//创建线程池并初始化
ThreadPool * CreateThreadPool(int min, int max, int queueSize);

//销毁线程池
int threadPoolDestroy(ThreadPool* pool);

//给线程池添加任务
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg);

//获取线程池中工作的线程个数
int threadPoolBusyNum(ThreadPool* pool);

//获取线程池中活着的线程个数
int threadPoolAliveNum(ThreadPool* pool);

//工作线程
void* worker(void* arg);

//管理者线程
void* manager(void* arg);

//将退出的thread id置为0
void threadExit(ThreadPool* pool);