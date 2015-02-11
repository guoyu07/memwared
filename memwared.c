/*
 * gcc -std=gnu99 -o memwared memwared.c -levent
 *
 *
 **/
#include "memwared.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sysexits.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <sys/resource.h>
#include <pwd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>
#include <bson.h>
#include <mongoc.h>

/* defaults */
 static void settings_init(void);

/* event handling, network IO*/
void do_accept( int sfd, short event, void *arg);
void do_read(int sfd, short event, void *arg);
void do_write(int sfd, short event, void *arg);
void *conn_work_process(void *arg);
void *mongo_clt_worker (void *data);
static void conn_init(void);

void memwared_close(int sig);


/** exported globals **/
struct stats stats;
struct settings settings;


static struct event_base *main_base;
mongoc_client_pool_t *mongoc_pool;
mongoc_uri_t *mongoc_uri;






static void settings_init(void) {
    settings.use_cas = true;
    settings.access = 0700;
    settings.port = 11211;
    settings.udpport = 11211;
    /* By default this string should be NULL for getaddrinfo() */
    settings.inter = NULL;
    settings.maxbytes = 64 * 1024 * 1024; /* default is 64MB */
    settings.maxconns = 1024;         /* to limit connections-related memory to about 5MB */
    settings.verbose = 0;
    settings.oldest_live = 0;
    settings.evict_to_free = 1;       /* push old items out of cache when memory runs out */
    settings.socketpath = NULL;       /* by default, not using a unix socket */
    settings.factor = 1.25;
    settings.chunk_size = 48;         /* space for a modest key and value */
    settings.num_threads = 4;         /* N workers */
    settings.num_threads_per_udp = 0;
    settings.prefix_delimiter = ':';
    settings.detail_enabled = 0;
    settings.reqs_per_event = 20;
    settings.backlog = 1024;
    settings.binding_protocol = negotiating_prot;
    settings.item_size_max = 1024 * 1024; /* The famous 1MB upper limit. */
    settings.maxconns_fast = false;
    settings.lru_crawler = false;
    settings.lru_crawler_sleep = 100;
    settings.lru_crawler_tocrawl = 0;
    settings.hashpower_init = 0;
    settings.slab_reassign = false;
    settings.slab_automove = 0;
    settings.shutdown_command = false;
    settings.tail_repair_time = TAIL_REPAIR_TIME_DEFAULT;
    settings.flush_enabled = true;
}

static void conn_init(void){
	
}



void do_accept(int fd, short event, void *arg)
{
	int sfd;
	struct sockaddr_in sin;
	socklen_t addrlen;
	addrlen = sizeof(sin);
	sfd = accept(fd, (struct sockaddr *)&sin, &addrlen);
	if (sfd == -1){
		perror("accept failed");
		close(sfd);
		return ((void)1);
	}else {
		//printf("accept: sfd[%d]\n",sfd);
	}

	if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL) | O_NONBLOCK) < 0){
		perror("setting O_NONBLOCK");
		close(sfd);
		return ((void)2);
	}
	

	threadpool_add_worker(conn_work_process, sfd);
	//conn_work_process(&fds[0]);

	//printf("main_base ptr: %p\n",main_base);
	/*struct event *rev = (struct event*)malloc(sizeof(struct event));
	//struct event rev;
	event_set(rev, sfd, EV_READ|EV_PERSIST,do_read, rev);
	event_base_set(main_base,rev);
	event_add(rev, 0);*/


	//event_del(arg);
	//close(sfd);
	return ;
}

void *conn_work_process(void *arg)
{
	//printf("thread_id: 0x%x, working on task sfd %d\n",pthread_self(), *(int*)arg);
	struct event *rev = (struct event*)malloc(sizeof(struct event));
	event_set(rev, *(int*)arg, EV_READ|EV_PERSIST,do_read, rev);
	event_base_set(main_base,rev);
	event_add(rev, 0);
	return NULL;
}

void do_read(int sfd, short event, void *arg){
	//printf("read fd: %d\n", sfd);
	char buffer[1024];
	int rev_size;
	memset(buffer, 0, 1024);
	rev_size = recv(sfd, buffer, 1024, 0);
	if (rev_size > 0){
		//printf("recv: %s", buffer);
		//mongo_clt_worker(mongoc_pool);
	}else {
		printf("error recv \n");
	}
	
	struct event *send = (struct event*)malloc(sizeof(struct event));
	event_set(send, sfd, EV_WRITE|EV_PERSIST, do_write, send);
	event_base_set(main_base,send);
	event_add(send, 0);
	event_del(arg);
	//close(sfd);
	return ;
}

void do_write(int sfd, short event, void *arg){
	char buffer[1024];
	int send_size;
	memset(buffer,0,1024+1);
	mongo_clt_worker(mongoc_pool);
	sprintf(buffer,"send buffer fd: %d",sfd);
	send_size = send(sfd,buffer, sizeof(buffer)+1,0);
	if (send_size < 0){
		printf("error send \n");
	}else {
		//printf("send: %s", buffer);
	}
	event_del(arg);
	close(sfd);
	return ;
}

void *mongo_clt_worker (void *data)
{
	mongoc_client_pool_t *pool = data;
	mongoc_client_t *client;
	mongoc_collection_t *collection;
	mongoc_cursor_t *cursor;
	const bson_t *doc;
	bson_t *query;
	char *str;

	client = mongoc_client_pool_pop(pool);
	if (!client){
		return NULL;
	}
	collection = mongoc_client_get_collection(client, "gamedb","entity_ff14_ClassJob");
	query = bson_new();
	BSON_APPEND_INT32(query, "Key", 1);
	cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE,0,0,0,query, NULL, NULL);
	while(mongoc_cursor_next(cursor, &doc)){
		str = bson_as_json(doc, NULL);
		//printf("%s\n", str);
		bson_free(str);
	}
	bson_destroy(query);
	mongoc_cursor_destroy(cursor);
	mongoc_collection_destroy(collection);



	mongoc_client_pool_push(pool,client);
	
	return NULL;
}

void memwared_close(int sig)
{
	printf("memwared close, please 'ctrl+c' to shutdown \n");
	threadpool_destroy();
	signal(SIGINT, SIG_DFL);
}

/*static int server_socket(const char *interface,
						int prot,
						enum network_transport transport,
						FILE *portnumber_file){
	int sfd;
	struct linger ling = {0,0};
	struct addrinfo *ai;
	struct addrinfo *next;
	struct addrinfo hints = {.ai_flags = AI_PASSIVE,
							.ai_family = AF_UNSPEC};
	char port_buf[NI_MAXSERV];
	int error;
	int success = 0;
	int flags = 1;

	hints.ai_socktype = IS_UDP(transport) ? SOCK_DGRAM : SOCKSTREAM;

	if (port == -1){
		port = 0;
	}
	snprintf(port_buf, sizeof(port_buf), "%d", port);
	error = getaddrinfo(interface, port_buf, &hints, &ai);
	if (error != 0){
		if (error != EAI_SYSTEM){
			fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(error));
		}else {
			perror("getaddrinfo()");
			return 1;
		}
	}

	for (next = ai; next; next = next->ai_next){
		conn *listen_conn_add;
		if ((sfd = new_socket(next)) == -1){
			if (errno = EMFILE){
				perror("server_socket");
				exit(EX_OSERR);
			}
			continue;
		}

		setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));
	}

}

static int server_sockets(int port, enum network_transport transport,
		FILE *portnumber_file)
{
	if (settings.inter == NULL)	{
		return server_socket(settings_inter, port, transport, portnumber_file);
	}else {
		char *b;
		int ret = 0;
		char *list = strdup(settings.inter);

		if (list == NULL){
			fprintf(stderr, "Failed to allocate memory for parsing server interface string\n");
			return 1;
		}
		for (char *p = strtok_r(list, ";,", &b);
				p != NULL;
				p = strtok_r(NULL, ";,", &b)){
			int the_port = port;
			char *s = strchr(p, ':');
			if (s != NULL){
				*s = '\0';
				++s;
				if (!safe_strtol(s, &the_port)){
					fprintf(stderr, "Invalid port number: \"%s\"", s);
					return 1;
				}
			}
			if (strcmp(p, "*") == 0){
				p = NULL;
			}
			ret |= server_socket(p, the_port, transport, portnumber_file);
		}
		free(list);
		return ret;
	}
}
*/


static void 
sig_handler(const int sig)
{
	printf("Signal handled: %s.\n", strsignal(sig));
	exit(EXIT_SUCCESS);
}

static bool 
sanitycheck(void)
{
	const char *ever = event_get_version();
	printf("libevent version :%s\n",ever);
	if (ever != NULL){
		if (strncmp(ever, "1.", 2) == 0){
			if ((ever[2] == '1' || ever[2] == '2') && !isdigit(ever[3])){
				fprintf(stderr, "You are using libevent %s.\nPlease upgrade to "
						"a more recent version (1.3 or newer)\n",
						event_get_version());
				return false;
			}
		}
	}
	return true;
}

int 
main (int argc, char **argv)
{
	int c;
	enum hashfunc_type hash_type = JENKINS_HASH;
	int maxcore = 0;
	struct rlimit rlim;
	char *username = NULL;
	struct passwd *pw;


	if (!sanitycheck()){
		return EX_OSERR;
	}

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	/* set stderr non-buffering */
	//setbuf(stderr, NULL);
	
	while (-1 != (c = getopt(argc, argv,
			"p:" // TCP port
			"P:"
			"r:" // maxcore core file limit
			"u:" // user identity to run as
		))){
		switch (c){
		case 'p':
			settings.port = atoi(optarg);
			printf("TCP port: %d\n",atoi(optarg));
			break;
		case 'r':
			maxcore = 1;
			break;
		case 'u':
			username = optarg;
			break;
		default:
			fprintf(stderr, "Illegal argument \"%c\"\n",c );
			return 1;
		}
	}

	/*if (hash_init(hash_type) != 0){
		fprintf(stderr, "Failed to initialize hash_algorithm!\n");
		exit(EX_USAGE);
	}*/

	/* maxcore file limit */
	/*if (maxcore != 0){
		struct rlimit rlim_new;
		if (getrlimit(RLIMIT_CORE, &rlim) == 0){
			rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
			if (setrlimit(RLIMIT_CORE, &rlim_new) != 0){
				rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
				(void)setrlimit(RLIMIT_CORE, &rlim_new);
			}
		}

		if ((getrlimit(RLIMIT_CORE, &rlim) != 0) || rlim.rlim_cur == 0){
			fprintf(stderr, "failed to ensure corefile creation\n");
			exit(EX_OSERR);
		}
	}
	
	if (getrlimit(RLIMIT_NOFILE, &rlim) != 0){
		fprintf(stderr, "failed to getrlimit number of files\n");
		exit(EX_OSERR);
	}else {
		rlim.rlim_cur = 1024;
		rlim.rlim_max = 1024;
		if (setrlimit(RLIMIT_NOFILE, &rlim) != 0){
			fprintf(stderr, "failed to set rlimit for open files. Try starting as root or requesting smaller maxconns value.\n");
			exit(EX_OSERR);
		}
	}*/

	/* start allow root user */
	if (getuid() == 0 || geteuid() == 0){
		if (username == 0 || *username == '\0'){
			fprintf(stderr, "can't run as root without the -u switch\n");
			exit(EX_USAGE);
		}
		if ((pw = getpwnam(username)) == 0){
			fprintf(stderr, "can't find the user %s to switch to \n", username);
			exit(EX_NOUSER);
		}
		if (setgid(pw->pw_gid) < 0 || setuid(pw->pw_uid) < 0){
			fprintf(stderr, "failed to assume identity of user %s \n", username);
			exit(EX_OSERR);
		}
	}

	/* initialize main thread libevent instance */
	main_base = event_init();

	/* initialize other stuff*/
	//assoc_init(settings.hashpower_init);
	//conn_init();
	
	if (sigignore(SIGPIPE) == -1){
		perror("failed to ignore SIGPIPE; sigaction");
		exit(EX_OSERR);
	}
	
	/* thread_pool_init */
	threadpool_init(4);

	/* socket tcp ,bind */
	/*if (settings.socketpath == NULL){
		const char *portnumber_filename = getenv("MEMCACHE_PORT_FILENAME");
		char temp_portnumber_filename[PATH_MAX];
		FILE *portnumber_file = NULL;
		
		if (portnumber_filename != NULL){
			snprintf(temp_portnumber_filename,
					sizeof(temp_portnumber_filename),
					"%s.lck",portnumber_filename);
			portnumber_file = fopen(temp_portnumber_filename, "a");
			if (portnumber_file == NULL){
				fprintf(stderr, "Failed to open \"%s\": %s\n",
						temp_portnumber_filename, strerror(errno));
			}
		}

		errno = 0;
		if (settings.port && server_sockets(settings.port, tcp_transport,
					portnumber_file)){
			vperror("failed to listen on TCP port %d", settings.port);
			exit(EX_OSERR);
		}

		if (portnumber_file){
			fclose(portnumber_file);
			rename(temp_portnumber_filename, portnumber_filename);
		}
	}*/

	int sfd;
	int flags;
	struct sockaddr_in serv_addr;
	if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) < 0){
		perror("sokcet error");
		return -1;
	}
	if ((flags = fcntl(sfd, F_GETFL, 0) < 0 || 
				fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0)){
		perror("setting O_NONBLOCK");
		close(sfd);
		return -1;
	}
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(settings.port);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	int ireuseadd_on = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &ireuseadd_on, sizeof(ireuseadd_on));
	
	if (bind(sfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0){
		perror("bind() error");
		close(sfd);
		return -1;
	}

	if (listen(sfd, settings.backlog) < 0){
		perror("listen() error");
		close(sfd);
		return -1;
	}else {
		printf("listenning...\n");
	}

	mongoc_init();
	mongoc_uri = mongoc_uri_new("mongodb://root:root@127.0.0.1:27017/?authSource=gamedb&minPollSize=16");
	mongoc_pool = mongoc_client_pool_new(mongoc_uri);

	struct event ev;
	printf("socket fd: %d\n",sfd);
	printf("main_base ptr: %p\n",main_base);
	event_set(&ev, sfd, EV_READ | EV_PERSIST, do_accept, &ev);
	event_base_set(main_base, &ev);
	event_add(&ev, 0);
	
	signal(SIGINT, memwared_close);

	if (event_base_loop(main_base, 0) != 0){
		exit(EX_OSERR);
	}


	printf("close memwared bye!\n");
	mongoc_uri_destroy(mongoc_uri);
	mongoc_cleanup();
	close(sfd);
	return 0;
}
