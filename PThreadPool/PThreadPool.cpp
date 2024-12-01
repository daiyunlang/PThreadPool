
#include "PThreadPool.h"

const int NUMBER = 2;

//任务结构体
typedef struct Task {
	void (*function)(void* arg);
	void* arg;
} Task;

//线程池结构体
typedef struct ThreadPool {
	//任务队列
	Task* taskQ;//数组
	int queueCapacity;//容量
	int queueSize;//当前任务个数
	int queueFront;//队头
	int queueTail;//队尾
	pthread_t managerID; //管理者线程id
	pthread_t* threadIDs; //工作的线程id，数组
	int minNum;//最小线程数
	int maxNum;//最大线程数
	int busyNum;//忙的线程个数
	int liveNum;//存活的线程个数
	int exitNum;//要销毁的线程个数
	pthread_mutex_t mutexPool;//锁整个线程池
	pthread_mutex_t mutexBusy;//锁busyNum变量
	pthread_cond_t condIsFull;
	pthread_cond_t condIsEmpty;
	int shutDown; //是否需要销毁线程池，销毁为1，不销毁为0


}ThreadPool;

ThreadPool * CreateThreadPool(int min, int max, int queueSize)
{
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	do {
		if (pool == NULL) {
			printf("malloc threadpool failed \n");
			break;
		}
		pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);
		if (pool->threadIDs == NULL) {
			printf("malloc threadids fail \n");
			break;
		}
		memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0;
		pool->liveNum = min;
		pool->exitNum = 0;

		if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
			pthread_cond_init(&pool->condIsEmpty, NULL) != 0 ||
			pthread_cond_init(&pool->condIsFull, NULL) != 0) {
			printf("mutex or condition initialize failed \n");
			break;
		}

		//任务队列
		pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueTail = 0;
		pool->shutDown = 0;

		//创建线程
		pthread_create(&pool->managerID, NULL, manager, pool);
		for (int i = 0; i < min; ++i) {
			pthread_create(&pool->threadIDs[i], NULL, worker, pool);
		}
		return pool;
	} while (0);
	//资源释放
	if (pool->threadIDs) free(pool->threadIDs);
	if (pool->taskQ) free(pool->taskQ);
	if (pool) free(pool);
	return NULL;
}

int threadPoolDestroy(ThreadPool* pool)
{
	if (pool == NULL) {
		return -1;
	}
	//关闭线程池
	pool->shutDown = 1;
	//阻塞回收管理者线程
	pthread_join(pool->managerID, NULL);
	//唤醒阻塞的消费者线程
	for (int i = 0; i < pool->liveNum; ++i) {
		pthread_cond_signal(&pool->condIsEmpty);
	}
	//释放堆内存
	if (pool->taskQ) {
		free(pool->taskQ);
	}
	if (pool->threadIDs) {
		free(pool->threadIDs);
	}
	
	pthread_mutex_destroy(&pool->mutexPool);
	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_cond_destroy(&pool->condIsFull);
	pthread_cond_destroy(&pool->condIsEmpty);
	free(pool);
	pool = NULL;

	return 0;
}

void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg)
{
	pthread_mutex_lock(&pool->mutexPool); 
	while (pool->queueSize == pool->queueCapacity
		&& !pool->shutDown) {
		//阻塞生产者线程
		pthread_cond_wait(&pool->condIsFull, &pool->mutexPool);
	}
	if (pool->shutDown) {
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}
	//添加任务
	pool->taskQ[pool->queueTail].function = func;
	pool->taskQ[pool->queueTail].arg = arg;
	pool->queueTail = (pool->queueTail + 1) % pool->queueCapacity;
	pool->queueSize++;

	pthread_cond_signal(&pool->condIsEmpty);

	pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexBusy);
	int busyNum = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexBusy);
	return busyNum;
}

int threadPoolAliveNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexPool);
	int aliveNum = pool->liveNum;
	pthread_mutex_unlock(&pool->mutexPool);
	return aliveNum;
}

void* worker(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	while (1) {
		pthread_mutex_lock(&pool->mutexPool);
		while (pool->queueSize == 0 && !pool->shutDown) {
			//阻塞工作线程
			pthread_cond_wait(&pool->condIsEmpty, &pool->mutexPool);
			//判断是否需要销毁
			if (pool->exitNum > 0) {
				pool->exitNum--; 
				if (pool->liveNum > pool->minNum) {
					pool->liveNum--;
					//此处需要注意，退出前需要解锁
					pthread_mutex_unlock(&pool->mutexPool);
					threadExit(pool);
				}
			}
		}
		//判断线程池是否被关闭了
		if (pool->shutDown) {
			pthread_mutex_unlock(&pool->mutexPool);
			threadExit(pool);
		}
		//从任务队列中取出一个任务
		Task task;
		task.function = pool->taskQ[pool->queueFront].function;
		task.arg = pool->taskQ[pool->queueFront].arg;
		//移动头结点
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		pool->queueSize--;
		//解锁生产者线程
		pthread_cond_signal(&pool->condIsFull);
		pthread_mutex_unlock(&pool->mutexPool);

		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);
		task.function(task.arg);
		//释放内存
		free(task.arg);
		task.arg = NULL;

		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
		
	}
	return nullptr;
}

void* manager(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	while (!pool->shutDown) {
		//每三s检查一次
		sleep(3);

		//取出线程池中任务的数量和当前线程池的数量
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		//取出忙的线程的数量
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		//添加线程
		//任务的个数 > 存活的线程个数 && 存活的线程个数 < 最大线程数
		if (queueSize > liveNum && liveNum < pool->maxNum) {
			pthread_mutex_lock(&pool->mutexPool);
			int counter = 0;//每次加几个线程
			for (int i = 0; i < pool->maxNum && counter < NUMBER
				&& pool->liveNum < pool->maxNum; ++i) {
				if (pool->threadIDs[i] == 0) {
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					counter++;
					pool->liveNum++;
				}
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}

		//销毁线程
		//忙的线程 * 2 < 存活的线程数 && 存活的线程数 > 最小线程数
		if (busyNum * 2 < liveNum && liveNum > pool->minNum) {
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER; 
			pthread_mutex_unlock(&pool->mutexPool);

			//让线程自杀
			for (int i = 0; i < NUMBER; ++i) {
				pthread_cond_signal(&pool->condIsEmpty);   
			}
		}

	}
	return nullptr;
}

void threadExit(ThreadPool* pool)
{
	pthread_t tid = pthread_self();
	for (int i = 0; i < pool->maxNum; ++i) {
		if (pool->threadIDs[i] == tid) {
			pool->threadIDs[i] = 0;
			break;
		}
	}
	pthread_exit(NULL);
}
