/*
    Audio Scout - audio content indexing software
    Copyright (C) 2010  D. Grant Starkweather & Evan Klinger
    
    Audio Scout is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    D. Grant Starkweather - dstarkweather@phash.org
    Evan Klinger          - eklinger@phash.org
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <getopt.h>
#include <syslog.h>
#include <time.h>
#include <zmq.h>
#include <assert.h>
#include "phash_audio.h"
#include "serialize.h"
#include "zmqhelper.h"

#define WORKING_DIR "/tmp"
#define LOG_IDENT "tblserv"
#define LOCKFILE "tableserver.lock"
#define MAX_INDEX_FILE_SIZE 65
#define INPROC_PIPE "inproc://pipe"
#define NB_BUCKETS_TABLE_SIZE (1<<25)
#define NB_BUCKETS_TMP_TABLE_SIZE (1<<20)
#define TIME_WAIT_FOR_TMP_INDEX (10*60)

static const char *opt_string = "w:l:s:i:p:b:t:n:vh?";
static const char *init_str = "INIT";
static const char *kill_str = "KILL";

static const struct option longOpts[] = {
    { "wd", required_argument, NULL, 'w'          },
    { "level", required_argument, NULL, 'l'       },
    { "index", required_argument, NULL, 'i'       },
    { "server", required_argument, NULL, 's'      },
    { "port", required_argument, NULL, 'p'        },
    { "blocksize", required_argument, NULL, 'b'   },
    { "threshold", required_argument, NULL, 't'   },
    { "threads", required_argument, NULL, 'n'     },
    { "verbose", no_argument, NULL, 'v'           },
    { "help", no_argument, NULL, 'h'              },
    { NULL, no_argument, NULL, 0                  }
};

struct globalargs_t {
    char *wd;              /* working directory          */
    char *server_address;  /* name of server address     */
    char *index_name;
    int level;
    int port;              /* starting port range on server */
    int blocksize;         /* block size for lookup on hash */
    float threshold;       /* threshold for look up comparisons */
    int nbthreads;         /* number worker threads to service incoming queries */
    int verboseflag;
    int helpflag;
} GlobalArgs;

void init_options(){
    GlobalArgs.wd = NULL;
    GlobalArgs.level = LOG_UPTO(LOG_ERR);
    GlobalArgs.server_address = NULL;
    GlobalArgs.index_name = NULL;
    GlobalArgs.port = 4005;
    GlobalArgs.blocksize = 128; 
    GlobalArgs.threshold = 0.050; 
    GlobalArgs.nbthreads = 60;
    GlobalArgs.verboseflag = 0;
    GlobalArgs.helpflag = 0;
}

void parse_options(int argc, char **argv){
    int longIndex;
    char opt = getopt_long(argc, argv,opt_string, longOpts, &longIndex);
    while (opt != -1){
	switch(opt){
	case 'w':
	    GlobalArgs.wd = optarg;
	    break;
	case 'l':
	    GlobalArgs.level = LOG_UPTO(atoi(optarg));
	    break;
	case 'i':
	    GlobalArgs.index_name = optarg;
	    break;
	case 's':
	    GlobalArgs.server_address = optarg;
	    break;
	case 'p':
	    GlobalArgs.port = atoi(optarg);
	    break;
	case 'b':
	    GlobalArgs.blocksize = atoi(optarg);
	    break;
	case 't':
	    GlobalArgs.threshold = atof(optarg);
	    break;
	case 'v':
	    GlobalArgs.verboseflag = 1;
	    break;
	case 'n':
	    GlobalArgs.nbthreads = atoi(optarg);
	    break;
	case 'h' :
	    GlobalArgs.helpflag = 1;
	    break;
	case '?':
	    GlobalArgs.helpflag = 1;
	    break;
	default:
	    break;
	}
	opt = getopt_long(argc, argv, opt_string, longOpts, &longIndex);
    }
}

void tableserver_usage(){
    fprintf(stdout,"tblservd [options]\n\n");
    fprintf(stdout,"options:\n");
    fprintf(stdout," -s <mainserver address> network address of the main server e.g. 192.0.0.100");
    fprintf(stdout,"                         mandatory\n");
    fprintf(stdout," -p <portnumber>         start of port range  - e.g. 5555");
    fprintf(stdout,"                         mandatory\n");
    fprintf(stdout," -w <working dir>        working dir to run tableserver default \"tmp\"\n"); 
    fprintf(stdout," -l <log level>          log level 0 and 1  to 8(0,LOG_EMERG...LOG_DEBUG\n");
    fprintf(stdout,"                         log levels correspond to those in syslog.h\n");
    fprintf(stdout," -b <block size>         blocksize for performing lookup,  default 128\n");
    fprintf(stdout," -t <threshold>          threshold for performing lookup, default 0.050\n");
    fprintf(stdout," -n <threads>            number of worker threads, default is 60\n");
    fprintf(stdout," -i <index name>         path and name of index file - mandatory\n");
}  

static uint8_t table_number = 0;

static char indexfile[FILENAME_MAX];
static char tmpindexfile[FILENAME_MAX];

/* Number currently accessing the index. */
/* Access this variable through its associated mutex. */
/* Do not access directly, but through the wait_for/post functions. */
/* When changing the value, signal on associated condition */
static int nb_index_access = 0;
static pthread_mutex_t  access_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   access_cond = PTHREAD_COND_INITIALIZER;
 
/* control access to the main index. */
/* use wait_for() and post() around access to the main index */
/* this allows a set nb of lookups to occur in parallel */
static AudioIndex audioindex = NULL;


/* call when thread is done accessing index */
void post_main_index(){
    pthread_mutex_lock(&access_mutex);
    nb_index_access--;
    pthread_cond_signal(&access_cond);
    pthread_mutex_unlock(&access_mutex);
}

/* call when thread about to access index */
/* waits until the value of 'nb_index_access' variable is below the parameter, nb */
void waitfor_main_index(int nb){
  pthread_mutex_lock(&access_mutex);
  while (nb_index_access > nb) {
    pthread_cond_wait(&access_cond, &access_mutex);
  }
  nb_index_access++;
  pthread_mutex_unlock(&access_mutex);
}


/* control access to tmp index */
/* wrap all access to tmp index with waitfor_index()/pos_index() */
/* native semaphore implementation to allow unlimited access, but the */
/* ability to unplug the index during updates by waiting for the count to go to zero */
static int nb_tmp_index_access = 0;
static pthread_mutex_t tmpaccess_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  tmpaccess_cond  = PTHREAD_COND_INITIALIZER;

/* tmp index for new additions */
/* sandwich all access with waitfor_tmp()/post_tmp() function calls*/
static AudioIndex audioindex_tmp = NULL;

/* control access to tmp index */
/* with post_tmp_index()/waitfor_tmp_index() */
void post_tmp_index(){
    pthread_mutex_lock(&tmpaccess_mutex);
    nb_tmp_index_access--;
    pthread_cond_signal(&tmpaccess_cond);
    pthread_mutex_unlock(&tmpaccess_mutex);
}

void waitfor_tmp_index(int nb){
  pthread_mutex_lock(&tmpaccess_mutex);
  while (nb_tmp_index_access > nb) {
    pthread_cond_wait(&tmpaccess_cond, &tmpaccess_mutex);
  }
  nb_tmp_index_access++;
  pthread_mutex_unlock(&tmpaccess_mutex);
}


/* DO NOT USE  - only saved here in order to shut down zmq messaging properly on term signal */
static void *main_ctx = NULL;

void init_process();
void kill_process();
int init_server();
int kill_server();
int init_index();
int kill_index();

void handle_signal(int sig){
    switch (sig){
    case SIGUSR1:
    case SIGUSR2:
      syslog(LOG_DEBUG, "recieved SIGUSR %d", sig);
      if (kill_server() < 0) { 
	syslog(LOG_CRIT,"SIGHANDLER: unable to kill server");
	break;
      }
      if (kill_index() < 0) { 
	syslog(LOG_CRIT,"SIGHANDLER: unable to kill index");
	break;
      }
      if (init_index() < 0) {
	syslog(LOG_CRIT,"SIGHANDLER: unable to init index - index STILL down");
	break;
      }
      if (init_server(main_ctx) < 0) {
	syslog(LOG_CRIT,"SIGHANDLER, unable to init server - index up but unknown by main server");
	break;
      }
      break;
    case SIGHUP:
    case SIGTERM:
	syslog(LOG_DEBUG, "recieved SIGTERM %d", sig);
	if (kill_server() < 0) syslog(LOG_CRIT,"SIGHANDLER: unable to kill server");
	if (kill_index() < 0) syslog(LOG_CRIT,"SIGHANDLER: unable to kill index");
	kill_process();
	/* should this be called ??? */
        /* if called, must close sockets in threads */
        /* before calling                           */
	/* zmq_term(main_ctx); */
	exit(0);
    }
}


int init_index(){
    /* init tmp index for new submissions */
    indexfile[0] = '\0';
    tmpindexfile[0] = '\0';
    snprintf(indexfile, FILENAME_MAX, "%s.idx", GlobalArgs.index_name);
    snprintf(tmpindexfile, FILENAME_MAX, "%s.tmp", GlobalArgs.index_name);
    syslog(LOG_DEBUG,"init index");

    int err;
    struct stat tmpidx_info, idx_info;
    if (!stat(tmpindexfile, &tmpidx_info) && !stat(indexfile, &idx_info)){
	/* if size if greater enough, merge temp into permanent index */
        syslog(LOG_DEBUG,"merge %s into %s", tmpindexfile, indexfile);
	err = merge_audioindex(indexfile, tmpindexfile);
	if (err < 0){
	    syslog(LOG_ERR, "unable to merge %s into %s", tmpindexfile, indexfile);
	} else if (err > 0){
	    syslog(LOG_DEBUG,"no need to merge %s into %s", tmpindexfile, indexfile);
	}
    } 

    syslog(LOG_DEBUG,"open index, %s", indexfile);
    pthread_mutex_lock(&access_mutex);
    audioindex = open_audioindex(indexfile, 0, NB_BUCKETS_TABLE_SIZE);
    if (audioindex) pthread_cond_signal(&access_cond);
    pthread_mutex_unlock(&access_mutex);
    if (audioindex == NULL){
	syslog(LOG_CRIT, "unable to open index, %s", indexfile);
	return -1;
    }
   
    syslog(LOG_DEBUG, "open tmp index, %s", tmpindexfile);
    pthread_mutex_lock(&tmpaccess_mutex);
    audioindex_tmp = open_audioindex(tmpindexfile, 1, NB_BUCKETS_TMP_TABLE_SIZE);
    if (audioindex_tmp) pthread_cond_signal(&tmpaccess_cond);
    pthread_mutex_unlock(&tmpaccess_mutex);
    if (audioindex_tmp == NULL){
	syslog(LOG_CRIT, "unable to open index, %s", tmpindexfile);
	return -1;
    } 

    return 0;
}

int kill_index(){
    int err = 0;

    syslog(LOG_DEBUG,"KILLINDEX: close index");

    /* wait for number accessing the index to be 0 */
    waitfor_main_index(0);
    err = close_audioindex(audioindex, 0);
    audioindex = NULL;
    post_main_index();

    if (err < 0) {
      syslog(LOG_ERR,"KILLINDEX: unable to close index");
      return -1;
    }

    syslog(LOG_DEBUG,"KILLINDEX: close tmp audioindex");

    /* wait for tmp index to close and NULL it */
    waitfor_tmp_index(0);
    err = flush_audioindex(audioindex_tmp, tmpindexfile);
    err = close_audioindex(audioindex_tmp, 1);
    audioindex_tmp = NULL;
    post_tmp_index();

    if (err < 0) {
      syslog(LOG_ERR,"KILLINDEX: unable to close tmp index, err = %d", err);
      err = -2;
    }

    return err;
}

void init_process(){
    int fv, i;

    if (getpid() == 1) return; 

    fv = fork();
    if (fv < 0){
	fprintf(stderr,"cannot fork\n"); 
	exit(1);
    }
    if (fv > 0) exit(0);

    /* daemon continues */
    setsid();
    
    /* close all file descrs */ 
    for (i=getdtablesize();i >= 0; --i) close(i);

    /* redirect stdin, stdout, stderr */ 
    i = open("/dev/null", O_RDWR);
    if (i < 0) exit(1);
    dup(i);
    dup(i);
    
    umask(0);

    chdir(GlobalArgs.wd);

    openlog(LOG_IDENT, LOG_PID, LOG_USER);
    setlogmask(GlobalArgs.level);

    /*first instance only */
    signal(SIGCHLD, SIG_IGN); /* ignore child */ 
    signal(SIGTSTP, SIG_IGN); /* ignore tty signals */ 
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP,  SIG_IGN);
    signal(SIGTERM, handle_signal);
    signal(SIGUSR1, handle_signal);
    signal(SIGUSR2, handle_signal);

    syslog(LOG_DEBUG, "INITPROCESS: init completed");
}

int init_server(void *ctx){
    char addr[32];

    /* send out init signal to main server  and get the table number */ 
    snprintf(addr, 32, "tcp://%s:%d", GlobalArgs.server_address, GlobalArgs.port+1);
    void *skt = socket_connect(ctx, ZMQ_REQ, addr);
    if (!skt){
	syslog(LOG_CRIT,"INITSERVER: unable to get req skt");
	exit(1);
    }

    char *str = strdup(init_str);
    send_msg_data(skt, str, strlen(str)+1, free_fn, NULL);

    /* wait for response */
    zmq_msg_t msg;
    zmq_msg_init(&msg);

    time_t curr_time;
    time(&curr_time);
    int err = 0;
    /* wait until error is recieved or until time alloted is up */
    do {
	err = zmq_recv(skt, &msg, ZMQ_NOBLOCK);
	sleep(1);
    } while (err && time(NULL) <= curr_time + 10);
    if (err){
	syslog(LOG_CRIT,"INITSERVER: no response from main server - timed out");
	err = -1;
    }

    uint8_t tn= 0;
    
    if (err == 0){
	memcpy(&tn, zmq_msg_data(&msg), sizeof(uint8_t));
    }
    zmq_msg_close(&msg);

    /* set static global variable */
    table_number = tn;
    zmq_close(skt);

    syslog(LOG_DEBUG, "INITSERVER: recived table number %u, init complete", tn);

    return err;
}

int kill_server(){
    char addr[32];
    void *ctx = main_ctx;

    snprintf(addr, 32, "tcp://%s:%d", GlobalArgs.server_address, GlobalArgs.port + 1);
    void *skt = socket_connect(ctx, ZMQ_REQ, addr);
    if (!skt){
	syslog(LOG_CRIT,"KILLSERVER: unable to get req skt");
	return -1;
    }

    uint8_t tn = table_number;
    table_number = 0;

    char *tmpstr = strdup(kill_str);
    sendmore_msg_data(skt, tmpstr, strlen(tmpstr)+1, free_fn, NULL);
    send_msg_vsm(skt, &tn, sizeof(uint8_t));

    time_t curr_time;
    time(&curr_time);

    zmq_msg_t msg;
    zmq_msg_init(&msg);
    int err = 0;
    do {
	err = zmq_recv(skt, &msg, ZMQ_NOBLOCK);
	sleep(1);
    } while (err && time(NULL) < curr_time + 10 );
    if (err){
	syslog(LOG_ERR, "KILLSERVER: no ack recieved - ignore");
	err = -1;
    }

    zmq_close(skt);
    syslog(LOG_DEBUG,"KILLSERVER: server killed");

    return err;
}

void kill_process(){
    closelog();
}

/* aux function to worker threads for sending the results */
static int send_results(void *skt, uint8_t threadnb, uint32_t id, float cs){

    syslog(LOG_DEBUG,"SEND: send thr = %u, id = %u, cs = %f", threadnb, id, cs);
    sendmore_msg_vsm(skt, &threadnb, sizeof(uint8_t));
    sendmore_msg_vsm(skt, &id, sizeof(uint32_t));
    send_msg_vsm(skt, &cs, sizeof(float));

    int err =0;
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    time_t curr_time;
    time(&curr_time);
    do {
      err = zmq_recv(skt, &msg, ZMQ_NOBLOCK);
      sleep(1);
    } while (err && time(NULL) < curr_time + 10 );
    if (!err){
      syslog(LOG_DEBUG,"SEND: reply recieved, msg size = %d", zmq_msg_size(&msg));
    } else {
      syslog(LOG_DEBUG,"SEND: no reply recieved");
    }

    return 0;
}

/* assign int value to worker thread for logging purposes */
static int thread_count = 0;


/* aux function to worker threads to retrieve optional message parts */
static uint8_t** retrieve_extra(void *pullskt, uint32_t nbframes, uint8_t *perms){
    uint8_t **toggles = NULL;
    void *data;
    int64_t more;
    size_t msg_size, more_size = sizeof(int64_t);

    /* gather toggles at end */
    recieve_msg(pullskt, &msg_size, &more, &more_size, &data);
    if (msg_size == 0){
	if (more) flushall_msg_parts(pullskt);
	return NULL;
    }
    if (msg_size != sizeof(uint8_t) && !more){
	if (more) flushall_msg_parts(pullskt);
	free(data);
	return NULL;
    }
    memcpy(perms, data, sizeof(uint8_t));
    free(data);

    toggles = malloc(nbframes*sizeof(uint8_t*));
    if (!toggles){
	if (more) flushall_msg_parts(pullskt);
	syslog(LOG_ERR,"RETRIEVEEXTRA: mem alloc error");
	return NULL;
    }

    memset(toggles, 0, nbframes*sizeof(uint8_t*));

    int i = 0;
    while (more && i < nbframes){
	recieve_msg(pullskt, &msg_size, &more, &more_size, &data);
	if (msg_size == 0) break;
	toggles[i++] = data;
    }

    for ( ; i<nbframes;i++){
	toggles[i] = malloc((*perms)*sizeof(uint8_t));
	if (!toggles[i]){
	    syslog(LOG_ERR,"RETRIEVEEXTRA: mem alloc error");
	    free(toggles);
	    toggles = NULL;
	    break;
	}
	memset(toggles[i], 0, *perms);
    }
    if (more) flushall_msg_parts(pullskt);

    return toggles;
}

/* aux function to worker thread to execute commands */
static int execute_command(uint8_t thrn, uint8_t cmd, uint32_t* hash,\
                           uint8_t **toggles, uint8_t perms, uint32_t nbframes,\
                           uint8_t threadnum, uint32_t *id, float *cs){
    int err = 0;
    uint8_t table_n;
    *cs = -1.00f;

    switch (cmd){
    case 1:
	if (audioindex){
	    waitfor_main_index(GlobalArgs.nbthreads+1);
	    syslog(LOG_DEBUG,"WORKER%d: do lookup for hash[%d]", thrn, nbframes);
	    err = lookupaudiohash(audioindex, (uint32_t*)hash,(uint8_t**)toggles,nbframes,perms,\
				  GlobalArgs.blocksize, GlobalArgs.threshold, id, cs);
	    post_main_index();

	    if (err < 0){
		syslog(LOG_ERR,"WORKER%d: could not do lookup - err %d", thrn, err);
		err = -1;
	    }
	} else {
	    syslog(LOG_DEBUG,"WORKER%d: index is down, unable to do lookup", thrn);
	    err = -2;
	}
	break;
    case 2:
	table_n = threadnum;
	if (table_n == table_number) { /* if meant for this table */
	    waitfor_tmp_index(GlobalArgs.nbthreads+1);
	    while (audioindex_tmp == NULL){
		pthread_cond_wait(&tmpaccess_cond, &tmpaccess_mutex);
	    }
	    syslog(LOG_DEBUG,"WORKER%d: inserting id = %d, hash[%d]", thrn, *id, nbframes);
	    err = insert_into_audioindex(audioindex_tmp, *id, (uint32_t*)hash, nbframes);
	    post_tmp_index();
	    if (err < 0){
		syslog(LOG_ERR,"WORKER%d: unable to insert hash - err %d", thrn, err);
		err = -3;
	    }
	} 
	break;
    default:
	syslog(LOG_DEBUG, "WORKER%d: cmd not recognized, %u", thrn, cmd);
	err = -4;
    }

    return err;
}

/* aux message to worker thread to a message */
static int pull_message(int thrn, void *pullskt, void *resultskt){
    uint8_t cmd, perms = 0, threadnum, table_n, **toggles = NULL;
    uint32_t nbframes, id = 0;
    void *hash = NULL, *data = NULL;
    int i, err;
    int64_t more;
    size_t msg_size, more_size = sizeof(int64_t);
    float cs = -1.0f;

    /* pull cmd msg part */
    recieve_msg(pullskt, &msg_size, &more, &more_size, &data);
    if (msg_size != sizeof(uint8_t) || !more){
	if (more) flushall_msg_parts(pullskt);
	free(data);
	return -1;
    }
    memcpy(&cmd, data, sizeof(uint8_t));
    free(data);

    /* pull nbframes msg part */
    recieve_msg(pullskt, &msg_size, &more, &more_size, &data);
    if (msg_size != sizeof(uint32_t) || !more){
	if (more) flushall_msg_parts(pullskt);
	free(data);
	return -2;
    }
    memcpy(&nbframes, data, sizeof(uint32_t));
    nbframes = nettohost32(nbframes);
    free(data);

    /* pull hash msg part */
    recieve_msg(pullskt, &msg_size, &more, &more_size, &hash);
    if (msg_size != nbframes*sizeof(uint32_t) || !more){
	syslog(LOG_DEBUG,"WORKER%d: inconsistent hash msg part size = %d", thrn, msg_size);
	if (more) flushall_msg_parts(pullskt);
	free(hash);
	return -3;
    }
	
    /* recieve threadnum (for cmd== 1) or table_num (for cmd == 2)*/
    recieve_msg(pullskt, &msg_size, &more, &more_size, (void**)&data);
    if (msg_size != sizeof(uint8_t) || !more){
	syslog(LOG_DEBUG,"WORKER%d: inconsistent threadnum msg size, %d", thrn, msg_size);
	if (more) flushall_msg_parts(pullskt);
	free(data);
	free(hash);
	return -4;
    }
    memcpy(&threadnum, data, sizeof(uint8_t));
    free(data);

    recieve_msg(pullskt, &msg_size, &more, &more_size, &data);
    if (msg_size != sizeof(uint32_t)){
	syslog(LOG_DEBUG,"WORKER%d: inconsistent uid msg size = %d", thrn, msg_size);
	free(data);
	free(hash);
	return -5;
    }
    memcpy(&id, data, sizeof(uint32_t));
    id = nettohost32(id);
    free(data);

    /* de-serialization */
    for (i=0;i<nbframes;i++){
	((uint32_t*)hash)[i] = nettohost32(((uint32_t*)hash)[i]);
    }

    if (more){
	toggles = retrieve_extra(pullskt, nbframes, &perms); 
    }

    err = execute_command(thrn, cmd, hash, toggles, perms,nbframes, threadnum, &id, &cs);
    if (err < 0){
	syslog(LOG_DEBUG,"WORKER%d: unable to execute command, err=%d", err);
    }
    
    if (toggles) {
	for (i = 0;i<nbframes;i++){
	    free(toggles[i]);
	}
	free(toggles);
	toggles = NULL;
    }
    free(hash);
    hash = NULL;
   
    if (cs >= GlobalArgs.threshold){
	syslog(LOG_DEBUG,"WORKER%d: %u threadnum, %f cs, %u id", thrn, threadnum, cs, id);
	id = hosttonet32(id);
	cs = hosttonetf(cs);
	send_results(resultskt, threadnum, id, cs);
    }

    return 0;
}

/* worker thread code */
void* dowork(void *arg){
    char addr[32];
    int thr_n  = thread_count++;/* for logging */
    void *ctx = arg;
    
    /* do not respond to SIGUSR1 or SIGUSR2 */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    if (pthread_sigmask(SIG_UNBLOCK, &set, NULL)){
      syslog(LOG_CRIT,"WORKER%d: unable to set sigmask", thr_n);
    }

    /* open socket to main auscoutd server */
    snprintf(addr, 32, "tcp://%s:%d", GlobalArgs.server_address, GlobalArgs.port+3);
    void *result_skt = socket_connect(ctx, ZMQ_REQ, addr);
    if (!result_skt){
	syslog(LOG_CRIT,"WORKER%d: unable to get req skt", thr_n);
	exit(1);
    }

    void *pullskt = socket_connect(ctx, ZMQ_PULL, INPROC_PIPE);
    if (!pullskt){
	syslog(LOG_CRIT,"WORKER%d: unable to create pull skt", thr_n);
	exit(1);
    }

    while (1){
	int err = pull_message(thr_n, pullskt, result_skt);
	if (err < 0){
	    syslog(LOG_DEBUG,"WORKER%d: unable to recieve message - err %d, skipping", err,thr_n);
	}
    }

    return NULL;
}

/* aux function to recieve new message from central server */
static int recieve_incoming_msg(void *subskt, void *pushskt){
    uint8_t cmd, default_threadnum = 0;
    void *data = NULL;
    char *topic = NULL;
    uint32_t nbframes, nbframes_local, default_id = 0;
    int64_t more;
    size_t msg_size;
    size_t more_size = sizeof(int64_t);

    /* wait for new msg - recieve topic in first part */
    recieve_msg(subskt, &msg_size, &more, &more_size, (void**)&topic);
    free(topic);
    topic = NULL;
    if (!more) return -1;

    /* recieve cmd msg part */
    recieve_msg(subskt, &msg_size, &more, &more_size, &data);
    if (msg_size != sizeof(uint8_t) || !more){
	free(data);
	if (more) flushall_msg_parts(subskt);
	return -2;
    }
    memcpy(&cmd, data, sizeof(uint8_t));
    free(data);

    /*recieve nbframes msg part */
    recieve_msg(subskt, &msg_size, &more, &more_size, (void**)&data);
    if (msg_size != sizeof(uint32_t) || !more){
	free(data);
	if (more) flushall_msg_parts(subskt);
	return -3;
    }
    memcpy(&nbframes, data, sizeof(uint32_t));
    nbframes_local = nettohost32(nbframes);
    free(data);

    sendmore_msg_vsm(pushskt, &cmd, sizeof(uint8_t));
    sendmore_msg_vsm(pushskt, &nbframes, sizeof(uint32_t));
    recvsnd(subskt, pushskt, &msg_size, &more, &more_size, NULL, NULL);
    if (msg_size != nbframes_local*sizeof(uint32_t) || !more){
	if (more) flushall_msg_parts(subskt);
	send_empty_msg(pushskt);
	return -4;
    }

    recvsnd(subskt, pushskt, &msg_size, &more, &more_size, NULL, NULL);
    if (msg_size != sizeof(uint8_t) || !more ){
	if (more) flushall_msg_parts(subskt);
	send_empty_msg(pushskt);
	return -5;
    }

    /* recieve/send id msg part */
    recvsnd(subskt, pushskt, &msg_size, &more, &more_size, NULL, NULL);
    if (msg_size != sizeof(uint32_t)){
	if (more) flushall_msg_parts(subskt);
	send_empty_msg(pushskt);
	return -6;
    }

    int i = 0;
    if (more){
	while (more && i < nbframes_local){
	    recvsnd(subskt, pushskt, &msg_size, &more, &more_size, NULL, NULL);
	    i++;
	}
	if (more)flushall_msg_parts(subskt);
    }
    send_empty_msg(pushskt);

    syslog(LOG_DEBUG,"SUBSCR: recieved cmd=%u,nb=%u,rows=%d", cmd, nbframes_local,i);

    return 0;
}

/* subscriber loop function */
void subscriber(void *arg){
    int i;
    char addr[32];
    void *ctx = arg;
    if (!ctx){
      syslog(LOG_CRIT,"SUBSCR ERR: null arg");
      return;
    }

    snprintf(addr, 32, "tcp://%s:%d", GlobalArgs.server_address, GlobalArgs.port+2);
    void *subskt = socket_connect(ctx, ZMQ_SUB, addr);
    if (!subskt){
	syslog(LOG_CRIT,"SUBSCR ERR: unable to connect subscr skt to %s", addr);
	return;
    }

    /* subscribe to messages */ 
    if (zmq_setsockopt(subskt, ZMQ_SUBSCRIBE, "phash.", 6)){
      syslog(LOG_CRIT,"SUBSCR ERR: unable to scribe subskt to messages");
      return;
    }
    
    void *pushskt = socket_bind(ctx, ZMQ_PUSH, INPROC_PIPE);
    if (!pushskt){
	syslog(LOG_CRIT,"SUBSCR ERR: unable to bind push skt to %s", INPROC_PIPE);
	return;
    }

    pthread_t worker_thr;
    for (i = 0; i < GlobalArgs.nbthreads; i++){
	if (pthread_create(&worker_thr, NULL, dowork, ctx)){
	    syslog(LOG_CRIT,"SUBSCR: unable to create worker thread, %d", i);
	    return;
	}
    }

    int err;
    while (1){
	err = recieve_incoming_msg(subskt, pushskt);
	if (err < 0){
	    syslog(LOG_DEBUG,"SUBSCR: message in non-conformance to format - %d, skipping",err);
	}
    }
}

int main(int argc, char **argv){
    init_options();
    parse_options(argc, argv);

    if (GlobalArgs.helpflag || !GlobalArgs.index_name || !GlobalArgs.server_address){
	tableserver_usage();
	return 0;
    }

    /* init daemon */ 
    init_process();
 
    if (init_index() < 0){
	syslog(LOG_CRIT,"MAIN ERR: unable to init index");
	exit(1);
    }
    
    void *ctx = zmq_init(1);
    if (!ctx){
	syslog(LOG_CRIT,"MAIN ERR: unable to init zmq ctx");
	exit(1);
    }

    /* save to global variable to be used in signal handler */
    main_ctx = ctx;

    if (init_server(ctx) < 0){
	syslog(LOG_CRIT,"MAIN ERR: unable to init server");
	exit(1);
    }

    subscriber(ctx);
    
    zmq_term(ctx);
    return 0;
}
