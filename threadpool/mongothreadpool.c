#include "mongothreadpool.h"

void mongothreadpool_init(int max_thread_num)
{
	pool = (mongothreadpool *)malloc(sizeof(mongothreadpool));

	pthread_mutex_init(&(pool->queue_lock), NULL);
	pthread_cond_init(&(pool->queue_ready), NULL);
	
	queue_init(&(pool->work_queue),free);
	
	pool->max_thread_num = max_thread_num;
	pool->cur_queue_size = 0;
	pool->shutdown = 0;
	
	pool->thread_ids = (pthread_t *)malloc(max_thread_num * sizeof(pthread_t));
	int i;
	for (i=0; i < max_thread_num; i++){
		pthread_create(&(pool->thread_ids[i]), NULL , mongothreadpool_handle, NULL);
	}

	//printf("%d\n",pool->cur_queue_size);

}

int mongothreadpool_add_worker(void *(*process)(void *arg), void* conn)
{
	mongothreadworker *new_worker = (mongothreadworker *)malloc(sizeof(mongothreadworker));
	new_worker->process = process;
	new_worker->arg = conn;
	
	pthread_mutex_lock(&(pool->queue_lock));
	queue_enqueue(&(pool->work_queue) , new_worker);
	pool->cur_queue_size++;
	//printf("queue_size: %d\n",pool->cur_queue_size);
	pthread_mutex_unlock(&(pool->queue_lock));
	
	pthread_cond_signal(&(pool->queue_ready));
	return 0;
}

int mongothreadpool_destroy()
{
	if (pool->shutdown)
		return -1;
	pool->shutdown = 1;

	pthread_cond_broadcast(&(pool->queue_ready));

	int i;
	for(i=0; i < pool->max_thread_num; i++){
		pthread_join(pool->thread_ids[i], NULL);
	}
	free(pool->thread_ids);
	
	while (queue_peek(&(pool->work_queue)) != NULL){
		mongothreadworker *worker = queue_peek(&(pool->work_queue));
		queue_dequeue(&(pool->work_queue), (void **)&worker);
	}

	pthread_mutex_destroy(&(pool->queue_lock));
	pthread_cond_destroy(&(pool->queue_ready));

	free(pool);
	pool = NULL;
	return 0;
}

void *mongothreadpool_handle(void *arg)
{
	//printf("starting thread 0x%x  \n", pthread_self());
	//printf("%d,%d\n",pool->cur_queue_size, pool->shutdown);
	while (1){
		pthread_mutex_lock(&(pool->queue_lock));
		//printf("%d,%d\n",pool->cur_queue_size, pool->shutdown);
		while (pool->cur_queue_size == 0 && !pool->shutdown){
			//printf("thread 0x%x is waiting\n", pthread_self());
			pthread_cond_wait(&(pool->queue_ready), &(pool->queue_lock));
		}

		if (pool->shutdown){
			pthread_mutex_unlock(&(pool->queue_lock));
			//printf("thread 0x%x will exit\n", pthread_self());
			pthread_exit(NULL);
		}

		//printf("thread 0x%x is starting to work\n", pthread_self());

		assert(pool->cur_queue_size != 0);
		assert(queue_peek(&(pool->work_queue)) != NULL);
		
		pool->cur_queue_size--;
		mongothreadworker *worker = queue_peek(&(pool->work_queue));
		queue_dequeue(&(pool->work_queue), (void **)&worker);
		pthread_mutex_unlock(&(pool->queue_lock));
		
		(*(worker->process))(worker->arg);
		free(worker);
		worker = NULL;
	}
	pthread_exit(NULL);
}


