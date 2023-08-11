#include "threadpool.h"

void task1(void* arg){
	int num=*(int*)arg;
	printf("thread %ld is working, number=%d (task1)\n",pthread_self(),num);
	if(num%2){
		sleep(1);
	}
}

void task2(void* arg){
	int num=*(int*)arg;
	printf("thread %ld is working, number=%d (task2)\n",pthread_self(),num);
	if(num%2){
		sleep(1);
	}
}

int main(){
	ThreadPool* pool=threadPoolCreate(3,10,100);
	for(int i=0;i<=100;i++){
		int* num = (int*)malloc(sizeof(int));
		*num=i;
		threadPoolAdd(pool,task1,num);
	}

	for(int i=101;i<=200;i++){
		int* num = (int*)malloc(sizeof(int));
		*num=i;
		threadPoolAdd(pool,task2,num);
	}
	sleep(30);
	threadPoolDestroy(pool);
	return 0;
}
