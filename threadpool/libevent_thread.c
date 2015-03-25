#include "libevent_thread.h"

static void thread_libevent_process(int fd, short which, void *arg)
{
	LIBEVENT_THREAD *me = arg;
    char buf[1];

    if (read(fd, buf, 1) != 1){
   		fprintf(stderr, "Can't read from libevent pipe\n");
    }else {
		printf("thread_libevent_process thread 0x%x\n", pthread_self());
    }
}

static void setup_thread(LIBEVENT_THREAD *me)
{
	me->base = event_init();
	if (!me->base){
		fprintf(stderr, "Can't malloc event base\n");
		exit(1);
	}

	event_set(&me->notify_event,
			me->notify_receive_fd,
			EV_READ | EV_PERSIST, thread_libevent_process,me
		);
	event_base_set(me->base, &me->notify_event);

	if (event_add(&me->notify_event, 0) == -1){
		fprintf(stderr, "Can't monitor libevent notify pipe\n");
		exit(1);
	}
	queue_init(&(me->new_conn_queue),free);
	printf("setup_thread\n");
}

static void create_worker(void *(*func)(void *), void *arg)
{
	pthread_t		thread;
	pthread_attr_t	attr;
	int 			ret;

	pthread_attr_init(&attr);

	if ((ret = pthread_create(&thread, &attr, func, arg)) != 0){
		fprintf(stderr, "Can't create thread: %s\n", strerror(ret));
		exit(1);
	}
}

static void *worker_libevent(void *arg)
{
	LIBEVENT_THREAD *me = arg;

	pthread_mutex_lock(&init_lock);
    init_count++;
    pthread_cond_signal(&init_cond);
    pthread_mutex_unlock(&init_lock);
    /* Force worker threads to pile up if someone wants us to */
    pthread_mutex_lock(&worker_hang_lock);
    pthread_mutex_unlock(&worker_hang_lock);

    printf("worker_libevent thread 0x%x  \n", pthread_self());
    event_base_loop(me->base, 0);
    return NULL;
}

static void wait_for_thread_registration(int nthreads) {
    while (init_count < nthreads) {
        pthread_cond_wait(&init_cond, &init_lock);
    }
}

void dispatch_conn(int sfd){
	char buf[1];
	buf[0] = 'c';

	int tid = (last_thread + 1) % 5;
	if (write(threads[tid].notify_send_fd, buf, 1) != 1){
		perror("Writing to thread notify pipe");
	}
	last_thread = tid;
	//printf("0 notify_send_fd %d  \n", threads[0].notify_send_fd);
	//printf("1 notify_send_fd %d  \n", threads[1].notify_send_fd);
	//printf("2 notify_send_fd %d  \n", threads[2].notify_send_fd);
}

void thread_init(int nthreads, struct event_base *main_base)
{

	int i;

	pthread_mutex_init(&init_lock, NULL);
    pthread_cond_init(&init_cond, NULL);

	threads = malloc(sizeof(LIBEVENT_THREAD)*nthreads);
	if (!threads){
		perror("Can't malloc thread descriptors");
		exit(1);
	}

	dispatcher_thread.base = main_base;
    dispatcher_thread.thread_id = pthread_self();

	printf("starting thread 0x%x  \n", pthread_self());
	for (i = 0; i < nthreads; i++){
		int fds[2];
		if (pipe(fds)){
			perror("Can't create notify pipe");
			exit(1);
		}

		threads[i].notify_receive_fd = fds[0];
		threads[i].notify_send_fd = fds[1];

		setup_thread(&threads[i]);
	}

	for (i = 0; i < nthreads; i++){
		create_worker(worker_libevent, &threads[i]);
	}


	pthread_mutex_lock(&init_lock);
    wait_for_thread_registration(nthreads);
    pthread_mutex_unlock(&init_lock);
}