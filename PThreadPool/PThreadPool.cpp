
#include "PThreadPool.h"

const int NUMBER = 2;

//����ṹ��
typedef struct Task {
	void (*function)(void* arg);
	void* arg;
} Task;

//�̳߳ؽṹ��
typedef struct ThreadPool {
	//�������
	Task* taskQ;//����
	int queueCapacity;//����
	int queueSize;//��ǰ�������
	int queueFront;//��ͷ
	int queueTail;//��β
	pthread_t managerID; //�������߳�id
	pthread_t* threadIDs; //�������߳�id������
	int minNum;//��С�߳���
	int maxNum;//����߳���
	int busyNum;//æ���̸߳���
	int liveNum;//�����̸߳���
	int exitNum;//Ҫ���ٵ��̸߳���
	pthread_mutex_t mutexPool;//�������̳߳�
	pthread_mutex_t mutexBusy;//��busyNum����
	pthread_cond_t condIsFull;
	pthread_cond_t condIsEmpty;
	int shutDown; //�Ƿ���Ҫ�����̳߳أ�����Ϊ1��������Ϊ0


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

		//�������
		pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueTail = 0;
		pool->shutDown = 0;

		//�����߳�
		pthread_create(&pool->managerID, NULL, manager, pool);
		for (int i = 0; i < min; ++i) {
			pthread_create(&pool->threadIDs[i], NULL, worker, pool);
		}
		return pool;
	} while (0);
	//��Դ�ͷ�
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
	//�ر��̳߳�
	pool->shutDown = 1;
	//�������չ������߳�
	pthread_join(pool->managerID, NULL);
	//�����������������߳�
	for (int i = 0; i < pool->liveNum; ++i) {
		pthread_cond_signal(&pool->condIsEmpty);
	}
	//�ͷŶ��ڴ�
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
		//�����������߳�
		pthread_cond_wait(&pool->condIsFull, &pool->mutexPool);
	}
	if (pool->shutDown) {
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}
	//�������
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
			//���������߳�
			pthread_cond_wait(&pool->condIsEmpty, &pool->mutexPool);
			//�ж��Ƿ���Ҫ����
			if (pool->exitNum > 0) {
				pool->exitNum--; 
				if (pool->liveNum > pool->minNum) {
					pool->liveNum--;
					//�˴���Ҫע�⣬�˳�ǰ��Ҫ����
					pthread_mutex_unlock(&pool->mutexPool);
					threadExit(pool);
				}
			}
		}
		//�ж��̳߳��Ƿ񱻹ر���
		if (pool->shutDown) {
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
		//�����������߳�
		pthread_cond_signal(&pool->condIsFull);
		pthread_mutex_unlock(&pool->mutexPool);

		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);
		task.function(task.arg);
		//�ͷ��ڴ�
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
		//ÿ��s���һ��
		sleep(3);

		//ȡ���̳߳�������������͵�ǰ�̳߳ص�����
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		//ȡ��æ���̵߳�����
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		//����߳�
		//����ĸ��� > �����̸߳��� && �����̸߳��� < ����߳���
		if (queueSize > liveNum && liveNum < pool->maxNum) {
			pthread_mutex_lock(&pool->mutexPool);
			int counter = 0;//ÿ�μӼ����߳�
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

		//�����߳�
		//æ���߳� * 2 < �����߳��� && �����߳��� > ��С�߳���
		if (busyNum * 2 < liveNum && liveNum > pool->minNum) {
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER; 
			pthread_mutex_unlock(&pool->mutexPool);

			//���߳���ɱ
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
