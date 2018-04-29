#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
struct job
{
    void* (*callback_function)(void *arg);    //线程回调函数
    void *arg;                                //回调函数参数
    struct job *next;
};

struct threadpool
{
    int thread_num;                   //线程池中开启线程的个数
    int queue_max_num;                //队列中最大job的个数
    struct job *head;                 //指向job的头指针
    struct job *tail;                 //指向job的尾指针
    pthread_t *pthreads;              //线程池中所有线程的pthread_t
    pthread_mutex_t mutex;            //互斥信号量
    pthread_cond_t queue_empty;       //队列为空的条件变量
    pthread_cond_t queue_not_empty;   //队列不为空的条件变量
    pthread_cond_t queue_not_full;    //队列不为满的条件变量
    int queue_cur_num;                //队列当前的job个数
    int queue_close;                  //队列是否已经关闭
    int pool_close;                   //线程池是否已经关闭
};
struct threadpool* threadpool_init(int thread_num, int queue_max_num);
int threadpool_add_job(struct threadpool *pool, void* (*callback_function)(void *arg), void *arg);
int threadpool_destroy(struct threadpool *pool);
void* threadpool_function(void* arg);