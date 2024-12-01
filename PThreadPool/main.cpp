#include "PThreadPool.h"

void taskFunc(void* arg) {
    int num = *(int*)arg;
    printf("thread %ld is working, number = %d \n",
        pthread_self(), num);
    usleep(1000);
}

int main()
{
    //创建线程池
    ThreadPool* pool = CreateThreadPool(3, 10, 100);
    for (int i = 0; i < 100; ++i) {
        int* num = (int*)malloc(sizeof(int));
        *num = i + 100;
        threadPoolAdd(pool, taskFunc, num);
    }

    sleep(5);
    threadPoolDestroy(pool);
    return 0;
}