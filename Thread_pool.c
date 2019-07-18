//
// Created by 谭太云飞 on 2019-07-18.
//

#include "Thread_pool.h"
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

// 内部调用执行任务的函数
void*
_run_task(void* pthis) {
    cthread_pool_t* pool = (cthread_pool_t*) pthis;

    while (1) {
        // 上锁
        pthread_mutex_lock(&(pool->queue_lock));

        // 当任务队列为空，且不准备销毁线程池的时候
        while ((pool->current_wait_task_num == 0) &&
               (pool->shutdown == 0)) {
            // 释放当前锁，并等待信号量的通知
            // 当得到信号量通知，且重新加锁后，将解除阻塞
            // 注意解除阻塞后该线程是获得互斥锁的
            pthread_cond_wait(&(pool->queue_ready), &(pool->queue_lock));
        }

        // 如果要销毁线程池
        if (pool->shutdown) {
            // 解锁
            pthread_mutex_unlock(&(pool->queue_lock));
            // 退出
            pthread_exit(NULL);
        }

        assert(pool->ptask_queue != NULL);
        assert(pool->current_wait_task_num != 0);

        // 获取任务等待队列的第一个任务
        pool->current_wait_task_num -= 1;
        pool->current_task_num += 1;
        pool->current_free_num -= 1;
        cwork_node_t * work = pool->ptask_queue;
        pool->ptask_queue = pool->ptask_queue->next;

        // 开锁
        pthread_mutex_unlock(&(pool->queue_lock));

        // 执行获取到的任务
        (*(work->process))(work->arg);
        free(work);
        work = NULL;  // 防止悬空指针

        // 获取锁
        pthread_mutex_lock(&(pool->queue_lock));
        pool->current_task_num -= 1;
        pool->current_free_num += 1;

        // 如果当前空闲线程大于最大空闲线程，则释放当前线程
        if (pool->current_free_num > pool->MAX_FREE_THREAD_NUM) {
            pool->current_thread_num -= 1;
            pool->current_free_num -= 1;
            pthread_mutex_unlock(&(pool->queue_lock));
            break;
        }

        // 开锁
        pthread_mutex_unlock(&(pool->queue_lock));

    }

    pthread_exit(NULL);
}

// 添加一个任务
int
add_task
        (void* pthis, void*(*process)(void* arg), void* arg) {
    cthread_pool_t* pool = (cthread_pool_t*) pthis;

    // 创建新的任务节点,并初始化
    cwork_node_t* work = (cwork_node_t*)malloc(sizeof(cwork_node_t));
    work->pre = NULL;
    work->next = NULL;
    work->process = process;
    work->arg = arg;

    // 开始向队列添加任务
    // 获取队列锁
    pthread_mutex_lock(&(pool->queue_lock));

    // 如果任务队列为空
    if (pool->current_wait_task_num == 0) {
        pool->ptask_queue = work;
    } else {  // 不为空则在最后添加任务
        cwork_node_t* cursor = pool->ptask_queue;

        while(cursor->next != NULL) {
            cursor = cursor->next;
        }

        cursor->next = work;
        work->pre = cursor;
    }

    // 如果任务队列仍为空，则不通过
    assert(pool->ptask_queue != NULL);

    // 任务数加1
    pool->current_wait_task_num += 1;

    // 当前没有空线程，且总线程数量没有超过最大线程数量
    if ((pool->current_free_num == 0) &&
        (pool->current_thread_num < pool->MAX_THREAD_NUM)) {
        // 创建新线程节点并初始化
        pool->current_thread_num += 1;
        pool->current_free_num += 1;
        cpthread_node_t* new_thread = (cpthread_node_t*)malloc(sizeof(cpthread_node_t));
        printf("New thread born.\n");
        new_thread->pre = NULL;
        new_thread->next = NULL;
        pthread_create(&(new_thread->pthread), NULL, pool->_run_task, (void*)pool);

        cpthread_node_t* cursor = pool->pthread_queue;

        if (cursor == NULL) {   // 空线程队列
            pool->pthread_queue = new_thread;
        } else {    // 线程队列不为空

            while (cursor->next != NULL) {
                cursor = cursor->next;
            }

            cursor->next = new_thread;
        }

        // 顺序争议处
        pthread_cond_signal(&(pool->queue_ready));
        pthread_mutex_unlock(&(pool->queue_lock));


        return 0;
    }

    // 顺序争议处
    pthread_cond_signal(&(pool->queue_ready));
    pthread_mutex_unlock(&(pool->queue_lock));

    return 0;
}

// 销毁线程池
int destory_pool(void* pthis) {
    cthread_pool_t* pool = (cthread_pool_t*)pthis;

    if (pool->shutdown == 1) {
        // 重复销毁
        return -1;
    }

    pool->shutdown = 1;

    // 唤醒所有线程
    pthread_cond_broadcast(&(pool->queue_ready));

    // 等待正在运行的所有任务完成,并释放线程队列内存
    while (pool->pthread_queue->next != NULL) {
        pthread_join(pool->pthread_queue->pthread, NULL);
        cpthread_node_t* current_node = pool->pthread_queue;
        pool->pthread_queue = pool->pthread_queue->next;
        free(current_node);
        printf("One thread die.\n");
        current_node = NULL;
    }

    if (pool->pthread_queue != NULL) {
        pthread_join(pool->pthread_queue->pthread, NULL);
        free(pool->pthread_queue);
        printf("One thread die.\n");
        pool->pthread_queue = NULL;
    }

    // 释放任务队列内存
    if (pool->ptask_queue != NULL) {
        while (pool->ptask_queue->next != NULL) {
            cwork_node_t* current = pool->ptask_queue;
            pool->ptask_queue = pool->ptask_queue->next;
            free(current);
            current = NULL;
        }

        if (pool->ptask_queue != NULL) {
            free(pool->ptask_queue);
            pool->ptask_queue = NULL;
        }

    }

    // 销毁锁和信号量
    pthread_mutex_destroy(&(pool->queue_lock));
    pthread_cond_destroy(&(pool->queue_ready));

    // 释放线程池总体内存
    free(pool);
    pool = NULL;

    return 0;
}


// 初始化一个线程池
cthread_pool_t*
create_pool() {
    // 申请线程池所需要的内存
    cthread_pool_t* pool = (cthread_pool_t*)malloc(sizeof(cthread_pool_t));

    if (pool == NULL) {
        return NULL;
    }

    // 标准操作
    memset(pool, 0, sizeof(cthread_pool_t));

    // 初始化锁和信号量
    pthread_mutex_init(&(pool->queue_lock), NULL);
    pthread_cond_init(&(pool->queue_ready), NULL);

    pool->MAX_THREAD_NUM          = 4;         // 最大线程数量
    pool->MAX_FREE_THREAD_NUM     = 2;         // 最大空闲线程数量

    pool->current_thread_num      = 0;          // 当前线程总数
    pool->current_wait_task_num   = 0;          // 当前等待的任务数
    pool->current_task_num        = 0;          // 当前正在执行的任务数量
    pool->current_free_num        = 0;          // 当前空闲线程数量

    pool->shutdown                = 0;          // 如果为1则已经销毁

    // 线程队列和任务队列
    pool->pthread_queue = NULL;
    pool->ptask_queue   = NULL;

    pool->add_task = add_task;
    pool->destory_pool = destory_pool;
    pool->_run_task = _run_task;

    return pool;
}