#ifndef LIBEVENT_THREAD_H
#define LIBEVENT_THREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <event.h>
#include "list.h"
#include "queue.h"

typedef struct {
    pthread_t thread_id;        
    struct event_base *base;    
} LIBEVENT_DISPATCHER_THREAD;

typedef struct {
	pthread_t thread_id;
	struct event_base *base;
	struct event notify_event;
	int notify_receive_fd;
	int notify_send_fd;
	Queue new_conn_queue;
} LIBEVENT_THREAD;


static pthread_mutex_t worker_hang_lock;

static LIBEVENT_DISPATCHER_THREAD dispatcher_thread;
static LIBEVENT_THREAD *threads;

static int init_count = 0;
static pthread_mutex_t init_lock;
static pthread_cond_t init_cond;

static void create_worker(void *(*func)(void *), void *arg);
void thread_init(int nthread, struct event_base *main_base);

#endif
