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

/* defaults */
 static void settings_init(void);

/* event handling, network IO*/
static void conn_init(void);


/** exported globals **/
struct stats stats;
struct settings settings;


static struct event_base *main_base;







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

	if (hash_init(hash_type) != 0){
		fprintf(stderr, "Failed to initialize hash_algorithm!\n");
		exit(EX_USAGE);
	}

	/* maxcore file limit */
	if (maxcore != 0){
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
	}

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
	conn_init();
	
	if (sigignore(SIGPIPE) == -1){
		perror("failed to ignore SIGPIPE; sigaction");
		exit(EX_OSERR);
	}

	return 0;
}
