#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "Thread_pool.h"


void* show() {
    int i;
    for (i = 0; i < 2; ++i) {
        printf("I am %lx ------ %d\n", pthread_self(), i);
        usleep(1);
    }
}


int main() {
    // 创建一个线程池
    cthread_pool_t* pool= create_pool();

    // 不断添加任务
    int i;
    for (i = 0; i < 10; i++) {
        pool->add_task(pool, show, NULL);
        usleep(1);
    }

    // 保证所有任务执行完毕
    sleep(3);

    // 销毁线程池
    pool->destory_pool(pool);

    return 0;
}
