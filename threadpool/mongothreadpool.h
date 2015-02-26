#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "list.h"
#include "queue.h"

typedef struct _mongothreadworker {
	void *(*process)(void *arg);
	void *arg;
} mongothreadworker;

typedef struct _mongothreadpool {
	pthread_mutex_t queue_lock;
	pthread_cond_t queue_ready;
	
	Queue work_queue;
	
	int shutdown;
	pthread_t *thread_ids;
	int max_thread_num;
	int cur_queue_size;

} mongothreadpool;

static mongothreadpool *pool = NULL;

void mongothreadpool_init(int max_thread_num);
int mongothreadpool_destroy();
int mongothreadpool_add_worker(void *(*process)(void *arg), void* conn);
void *mongothreadpool_handle(void *arg);
#endif
