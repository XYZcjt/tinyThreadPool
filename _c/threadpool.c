#include "threadpool.h"
const int NUMBER=2;

// 任务结构体
typedef struct Task{
	void (*func)(void* arg);
	void* arg;
}Task;

// 线程池结构体
struct ThreadPool{

	Task* taskQ;
	int queueCapacity;
	int queueSize;
	int queueFront;
	int queueRear;

	pthread_t managerID;
	pthread_t *threadIDs;
	int minNum;
	int maxNum;
	int busyNum;
	int liveNum;
	int exitNum;

	pthread_mutex_t mutexPool;
	pthread_mutex_t mutexBusy;

	pthread_cond_t is_Full;
	pthread_cond_t is_Empty;

	int shutdown;

};


ThreadPool* threadPoolCreate(int min,int max,int queueSize){
	// 给线程池分配空间
	ThreadPool* pool=(ThreadPool*)malloc(sizeof(ThreadPool));
	if(pool==NULL){
		perror("malloc pool");
		free(pool);
		pool=NULL;
		return NULL;
	}
	
	// 给工作线程分配空间
	pool->threadIDs=(pthread_t*)malloc(sizeof(pthread_t)*max);
	if(pool->threadIDs==NULL){
		free(pool);
		pool=NULL;
		perror("malloc threadIDs");
		return NULL;
	}

	// 初始化
	memset(pool->threadIDs,0,sizeof(pthread_t)*max);
	pool->minNum=min;
	pool->maxNum=max;
	pool->busyNum=0;
	pool->liveNum=min;
	pool->exitNum=0;

	pool->taskQ=(Task*)malloc(sizeof(Task)*queueSize);
	if(pool->taskQ==NULL){
		free(pool->threadIDs);
		free(pool);
		pool=NULL;
		perror("malloc taskQ");
		return NULL;
	}
	pool->queueCapacity=queueSize;
	pool->queueFront=0;
	pool->queueRear=0;
	pool->queueSize=0;

	// 初始化锁
	if(pthread_mutex_init(&pool->mutexPool,NULL)!=0||
			pthread_mutex_init(&pool->mutexBusy,NULL)!=0||
			pthread_cond_init(&pool->is_Full,NULL)!=0||
			pthread_cond_init(&pool->is_Empty,NULL)!=0){
		if(pool->taskQ){
			free(pool->taskQ);
		}
		if(pool->threadIDs){
			free(pool->threadIDs);
		}
		if(pool){
			free(pool);
		}
		pool=NULL;
		perror("mutex or cond init failed");
		return NULL;
	}

	// 是否关闭线程池，1关闭，0不关
	pool->shutdown=0;


	// 创建管理者线程
	pthread_create(&pool->managerID,NULL,manager,pool);
	// 创建工作者线程
	for(int i=0;i<min;i++){
		pthread_create(&pool->threadIDs[i],NULL,worker,pool);
	}

	return pool;
}
void threadPoolAdd(ThreadPool* pool,void (*func)(void* arg),void* arg){
	pthread_mutex_lock(&pool->mutexPool);
	// 任务队列满了，阻塞
	while(pool->queueSize==pool->queueCapacity&&!pool->shutdown){
		pthread_cond_wait(&pool->is_Full,&pool->mutexPool);
	}

	// 线程池关闭，解锁退出
	if(pool->shutdown){
		pthread_mutex_unlock(&pool->mutexPool);
		threadExit(pool);
	}

	// 添加任务
	pool->taskQ[pool->queueRear].func=func;
	pool->taskQ[pool->queueRear].arg=arg;
	pool->queueRear=(pool->queueRear+1)%pool->queueCapacity;
	pool->queueSize++;
	pthread_cond_signal(&pool->is_Empty);
	pthread_mutex_unlock(&pool->mutexPool);
}


int getBusyNum(ThreadPool* pool){
	pthread_mutex_lock(&pool->mutexBusy);
	int busyNum=pool->busyNum;
	pthread_mutex_unlock(&pool->mutexBusy);
	return busyNum;
}

int getLiveNum(ThreadPool* pool){
	pthread_mutex_lock(&pool->mutexPool);
	int liveNum=pool->liveNum;
	pthread_mutex_unlock(&pool->mutexPool);
	return liveNum;
}

void* worker(void* arg){
	ThreadPool* pool=(ThreadPool*)arg;
	while(1){
		// 任务队列为空，阻塞
		while(pool->queueSize==0&&!pool->shutdown){
			pthread_cond_wait(&pool->is_Empty,&pool->mutexPool);
			if(pool->exitNum>0){
				pool->exitNum--;
				if(pool->liveNum > pool->minNum){
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutexPool);
					threadExit(pool);
				}
			}
		}
		// 线程池关闭，解锁退出
		if(pool->shutdown){
			pthread_mutex_unlock(&pool->mutexPool);
			threadExit(pool);
		}
		
		// 工作线程获取任务
		Task task;
		task.func=pool->taskQ[pool->queueFront].func;
		task.arg=pool->taskQ[pool->queueFront].arg;
		pool->queueFront=(pool->queueFront+1)%pool->queueCapacity;
		pool->queueSize--;

		// 工作队列不满，唤醒阻塞的is_Full信号量，继续添加任务
		pthread_cond_signal(&pool->is_Full);
		pthread_mutex_unlock(&pool->mutexPool);

		printf("thread %ld start working...\n",pthread_self());
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);

		task.func(task.arg);
		free(task.arg);
		task.arg=NULL;

		printf("thread %ld end working...\n",pthread_self());
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
	}
}

void* manager(void* arg){
	ThreadPool* pool=(ThreadPool*)arg;
	while(!pool->shutdown){
		// 工作者线程每三秒工作一次
		sleep(3);
		// 获取任务数量和存活线程数
		pthread_mutex_lock(&pool->mutexPool);
		int liveNum=pool->liveNum;
		int queueSize=pool->queueSize;
		pthread_mutex_unlock(&pool->mutexPool);
		
		// 获取忙线程数
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum=pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		// 添加线程的策略，当任务>liveNum-2时，添加线程
		if(queueSize>(liveNum-2)&&liveNum<pool->maxNum){
			pthread_mutex_lock(&pool->mutexPool);
			int counter=0;
			for(int i=0;(i<pool->maxNum)&&(counter<NUMBER)&&(pool->liveNum<pool->maxNum);i++){
				if(pool->threadIDs[i]==0){
					pthread_create(&pool->threadIDs[i],NULL,worker,pool);
					pool->liveNum++;
					counter++;
				}
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}

		// 销毁线程的策略，当liveNum>busyNum*2
		if(busyNum*2<liveNum&&liveNum>pool->minNum){
			printf("busy`s Thread less than live`s Thread,destroy thread...\n");
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum=NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			// 唤醒子线程，让其自杀
			for(int i=0;i<NUMBER;i++){
				pthread_cond_signal(&pool->is_Empty);
			}
		}
	}
	return NULL;
}

void threadExit(ThreadPool* pool){
	pthread_t tid=pthread_self();
	for(int i=0;i<pool->maxNum;i++){
		if(pool->threadIDs[i]==tid){
			pool->threadIDs[i]=0;
			printf("thredExit() called,%ld exiting...\n",tid);
			break;
		}
	}
	pthread_exit(NULL);
}


int threadPoolDestroy(ThreadPool* pool){
	if(pool==NULL){
		return -1;
	}

	pool->shutdown=1;

	pthread_join(pool->managerID,NULL);
	for(int i=0;i<pool->liveNum;i++){
		pthread_cond_signal(&pool->is_Empty);
	}
	if(pool->taskQ){
		free(pool->taskQ);
	}
	if(pool->threadIDs){
		free(pool->threadIDs);
	}

	pthread_mutex_destroy(&pool->mutexPool);
	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_cond_destroy(&pool->is_Empty);
	pthread_cond_destroy(&pool->is_Full);
	free(pool);
	pool=NULL;
	return 0;
}



