#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

typedef struct ThreadPool ThreadPool;

// 线程池的初始化
ThreadPool* threadPoolCreate(int min,int max,int queueSize);

// 往线程池中添加任务
void threadPoolAdd(ThreadPool* pool,void(*func)(void* arg),void* arg);

// 获取工作中的线程数
int getBusyNum(ThreadPool* pool);

// 获取存活的线程
int getLiveNum(ThreadPool* pool);

// 单个线程退出线程池
void threadExit(ThreadPool* pool);

// 管理者工作函数
void* manager(void* arg);

// 工作者任务函数
void* worker(void* arg);

// 销毁线程池
int threadPoolDestroy(ThreadPool* pool);

#endif
