//
// Created by 谭太云飞 on 2019-07-18.
//

#ifndef THREAD_POOL_PROJECT01_THREAD_POOL_H
#define THREAD_POOL_PROJECT01_THREAD_POOL_H
#include <sys/types.h>

// 线程节点
typedef struct CPTHREAD_NODE {
    struct CPTHREAD_NODE* pre;
    struct CPTHREAD_NODE* next;

    // 线程节点中的线程
    pthread_t pthread;
} cpthread_node_t;


// 任务节点
typedef struct CWORK_NODE {
    struct CWORK_NODE* pre;
    struct CWORK_NODE* next;

    // 任务节点中的具体任务
    void*(*process)(void* arg);
    // 该任务的参数
    void* arg;
} cwork_node_t;


// 线程池
typedef struct CTHREAD_POOL {
    // 任务队列锁以及信号量
    pthread_mutex_t queue_lock;
    pthread_cond_t	queue_ready;

    int MAX_THREAD_NUM;         // 最大线程数量
    int MAX_FREE_THREAD_NUM;    // 最大空闲线程数量

    int current_thread_num;     // 当前线程总数
    int current_wait_task_num;  // 当前等待的任务数
    int current_task_num;       // 当前正在执行的任务数量
    int current_free_num;       // 当前空闲线程数量

    int shutdown;               // 如果为1则已经销毁

    // 线程队列和任务队列
    cpthread_node_t* pthread_queue;
    cwork_node_t* ptask_queue;


    // 添加一个任务
    int
    (*add_task)
            (void* pthis, void*(*process)(void* arg), void* arg);


    // 等待所有任务完成，释放线程池资源
    int
    (*destory_pool)(void* pthis);


    // 执行任务
    void*
    (*_run_task)
            (void* pthis);

} cthread_pool_t;


// 初始化一个线程池
cthread_pool_t* create_pool();

#endif //THREAD_POOL_PROJECT01_THREAD_POOL_H
