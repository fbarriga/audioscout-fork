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
#include <stdint.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <time.h>
#include <getopt.h>
#include <locale.h>
#include <syslog.h>
#include <assert.h>
#include "zmq.h"
#include "audiodata.h"
#include "serialize.h"
#include "zmqhelper.h"

#define WORKING_DIR "/tmp"
#define LOG_IDENT  "auscoutd"
#define LOCKFILE "auscout.lock"
#define MAX_TABLES 255

#define WAIT_TIME_SECONDS 5
#define INPROC_REQUEST_ADDRESS  "inproc://querys"
#define INPROC_PUBLISH_ADDRESS "inproc://publish"
#define INPROC_RESULT_ADDRESS  "inproc://result"

static const char *opt_string = "w:l:t:p:n:d:vh?";
static const char *query_topic = "phash.q|";
static const char *submit_topic = "phash.s|";
static const char delim = 30;
static const char space = 32;
static const char *resp_str = "OK";
static const char *null_str = "not found";

static const struct option longOpts[] = {
    { "wd", required_argument, NULL, 'w' },
    { "audiodb", required_argument, NULL, 'd' },
    { "level", required_argument, NULL, 'l' },
    { "threshold", required_argument,NULL, 't'},
    { "port", required_argument, NULL, 'p'},
    { "nbthreads", required_argument, NULL, 'n'},
    { "verbose", no_argument, NULL, 'v'},
    { "help", no_argument, NULL, 'h'},
    { NULL, no_argument, NULL, 0 }
};

struct globalargs_t {
    char *wd;
    char *audiodb_addr;
    int level;            /* log level - as defined in syslog.h */ 
    int port;
    float threshold;
    int nbthreads;
    int verboseflag;
    int helpflag;
} GlobalArgs;

void init_options(){
    GlobalArgs.wd = WORKING_DIR;
    GlobalArgs.level = LOG_UPTO(LOG_ERR);
    GlobalArgs.audiodb_addr = NULL;
    GlobalArgs.port = 4005;
    GlobalArgs.threshold = 0.050;
    GlobalArgs.nbthreads = 10;
    GlobalArgs.verboseflag = 0;
    GlobalArgs.helpflag = 0;
}

void parse_options(int argc, char **argv){
    int longIndex;
    char opt = getopt_long(argc,argv,opt_string, longOpts, &longIndex);
    while (opt != -1){
	switch(opt){
	case 'w':
	    GlobalArgs.wd = optarg;
	    break;
	case 'l':
	    GlobalArgs.level = LOG_UPTO(atoi(optarg));
	    break;
	case 'd':
	    GlobalArgs.audiodb_addr = optarg;
	    break;
	case 't':
	    GlobalArgs.threshold = atof(optarg);
	    break;
	case 'p':
	    GlobalArgs.port = atoi(optarg);
	    break;
	case 'n':
	    GlobalArgs.nbthreads = atoi(optarg);
	    break;
	case 'v':
	    GlobalArgs.verboseflag = 1;
	    break;
	case 'h':
	    GlobalArgs.helpflag = 1;
	    break;
	case '?':
	    GlobalArgs.helpflag = 1;
	default:
	    break;
	}
	opt = getopt_long(argc, argv, opt_string, longOpts, &longIndex);
    }
}

void auscout_usage(){
    fprintf(stdout,"auscoutd [options]\n\n");
    fprintf(stdout,"options:\n");
    fprintf(stdout,"-p <integer>        integer port to start 4 port range - default 4005\n");
    fprintf(stdout,"-d <tcp://addr:port>  metadata db server address - mandatory\n");
    fprintf(stdout,"-w <working dir>    working directory for the server - default %s\n",\
                                                                           WORKING_DIR);
    fprintf(stdout,"-l <logging level>  log level 0-7 - default LOG_ERR\n");
    fprintf(stdout,"-t <threshold>      threshold value (default 0.050\n");
    fprintf(stdout,"-n <nbthreads>      number of threads to run in server, default 10\n");
    fprintf(stdout,"-v verbose flag     more logging information - not implemented\n");
    fprintf(stdout,"-h help info        print this help \n");
    fprintf(stdout,"?  help info        print this help \n");
}

static int lock_fd;

/* array of table to track which tables are enabled */
/* use tbl_mutex when setting/unsetting a particular table */
static int nb_tables = 0;
static uint8_t tables[MAX_TABLES];
static pthread_mutex_t tbl_mutex = PTHREAD_MUTEX_INITIALIZER; 

/* not for use - here only to enable quick shutdown with TERM, HUP signals */
static void *main_ctx = NULL;

void kill_process();
void init_process();

void kill_server(){
}

void handle_signal(int sig){

    switch (sig){
    case SIGHUP:
    case SIGTERM:
	syslog(LOG_DEBUG,"recieved signal %d", sig);
	kill_server();
	kill_process();
        /* if zmq_term function invoked */
        /* need to close sockets in threads first */
        /* and set socket option ZMQ_LINGER with 0 value */
	/* zmq_term(main_ctx); */
	exit(0);
	break;
    }
}

void init_process(){
    int fv,i;

    if (getpid() == 1) {
	return;
    }
    fv = fork();
    if (fv < 0){fprintf(stderr,"cannot fork\n");  exit(1);}
    if (fv > 0)exit(0);
    
    /* daemon continues */
    setsid();

    /* close all file descrs */ 
    for (i=getdtablesize();i >= 0; --i) close(i);

    /* redirect stdin, stdout, stderr */ 
    i = open("/dev/null", O_RDWR);
    dup(i);
    dup(i);
    
    umask(0);
    chdir(GlobalArgs.wd);

    lock_fd = open(LOCKFILE, O_RDWR|O_CREAT, 0755);
    if (i < 0)exit(1);
    if (lockf(lock_fd, F_TLOCK, 0) < 0) exit(0);

    /* open logger */ 
    openlog(LOG_IDENT, LOG_PID, LOG_USER);
    setlogmask(GlobalArgs.level);

    /*first instance only */
    signal(SIGCHLD, SIG_IGN); /* ignore child */ 
    signal(SIGTSTP, SIG_IGN); /* ignore tty signals */ 
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, handle_signal);
    signal(SIGTERM, handle_signal);

    syslog(LOG_DEBUG, "process init'd");
}

void kill_process(){
    lockf(lock_fd, F_ULOCK, 0);
    close(lock_fd);
    closelog();
    syslog(LOG_DEBUG, "process kill'd");
}

/* parameter to do_queue or do_forward threads */
typedef struct dev_param_t {
    void *ctx;                  /* zmq context */
    char *skt1_addr;            /* from */
    char *skt2_addr;            /* to   */
}DevParam;


void* do_queue(void *arg){
    DevParam *dp = (DevParam*)arg;

    if (!dp || !dp->ctx || !dp->skt1_addr || !dp->skt2_addr){
	syslog(LOG_CRIT,"QUEUE ERROR: null arg");
	exit(1);
    }

    void *front_skt = socket_bind(dp->ctx, ZMQ_XREP, dp->skt1_addr);
    if (!front_skt){
	syslog(LOG_CRIT,"QUEUE ERROR: unable to bind to %s", dp->skt1_addr);
	exit(1);
    }

    void *back_skt = socket_bind(dp->ctx, ZMQ_XREQ, dp->skt2_addr);
    if (!back_skt){
	syslog(LOG_CRIT,"QUEUE ERROR: unable to bind to %s", dp->skt2_addr);
	exit(1);
    }

    zmq_device(ZMQ_QUEUE, front_skt, back_skt);

    return NULL;
}

void* do_forward(void *arg){
    DevParam *dp = (DevParam*)arg;

    if (!dp || !dp->ctx || !dp->skt1_addr || !dp->skt2_addr){
	syslog(LOG_CRIT,"FWD ERROR: null arg");
	exit(1);
    }

    void *pubskt = socket_bind(dp->ctx, ZMQ_PUB, dp->skt2_addr);
    if (!pubskt){
	syslog(LOG_CRIT, "FWD ERROR: unable to create pub skt on %s", dp->skt2_addr);
	exit(1);
    }

    void *pullskt = socket_bind(dp->ctx, ZMQ_PULL, dp->skt1_addr);
    if (!pullskt){
	syslog(LOG_CRIT,"FWDERR: unable to create pull skt on %s", dp->skt1_addr);
	exit(1);
    }

    zmq_msg_t msg;
    int64_t more;
    size_t more_size = sizeof(int64_t);
    int i = 0;
    while (1){
	zmq_msg_init(&msg);
	zmq_recv(pullskt, &msg, 0);
	zmq_getsockopt(pullskt, ZMQ_RCVMORE, &more, &more_size);
	zmq_send(pubskt, &msg, (more) ? ZMQ_SNDMORE : 0);
	zmq_msg_close(&msg);

	while (more){
	    zmq_msg_init(&msg);
	    zmq_recv(pullskt, &msg, 0);
	    zmq_getsockopt(pullskt, ZMQ_RCVMORE, &more, &more_size);
	    zmq_send(pubskt, &msg, (more) ? ZMQ_SNDMORE : 0);
	    zmq_msg_close(&msg);
	}
    }

    return NULL;
}


int waitresults(void *skt, uint32_t *id){
    int err = 0;
    time_t curr;
    time(&curr);

    *id = 0;
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    do {
	err = zmq_recv(skt, &msg, ZMQ_NOBLOCK);
    } while (err && time(NULL) < curr + WAIT_TIME_SECONDS);
    if (err == 0) memcpy(id, zmq_msg_data(&msg), sizeof(uint32_t));
    zmq_msg_close(&msg);

    return err;
}

int select_table(){
    int table_n = -1;
    /* find an active table */
    if (nb_tables > 0){
	do {
	    table_n = rand()%MAX_TABLES;
	} while (tables[table_n] == 0);
    }
    return table_n;
}

int handle_request(uint8_t thrn, void *qskt, void *pushskt, void *rskt, AudioDataDB mdata_db){
    uint8_t cmd, table_n, perms;
    uint32_t nb, nb_local, id, sid;
    int64_t more;
    size_t more_size = sizeof(int64_t);
    size_t msg_size;
    uint8_t *data = NULL;
    char *mdata_inline = NULL, *topic_str = NULL;
    int i;

    id = 0;
    table_n = 0;
    perms = 0;

    /* wait for cmd */
    recieve_msg(qskt, &msg_size, &more, &more_size, (void**)&data);
    if (msg_size != sizeof(uint8_t) || !more){
	syslog(LOG_DEBUG,"WORKER%d: inconsistent msg size=%ld", thrn, msg_size);
	flushall_msg_parts(qskt);
	free(data);
	send_empty_msg(qskt);
	return -1;
    }
    memcpy(&cmd, data, sizeof(uint8_t));
    free(data);

    /* wait for nb frames */
    recieve_msg(qskt, &msg_size, &more, &more_size, (void**)&data);
    if (msg_size != sizeof(uint32_t) || !more){
	syslog(LOG_DEBUG,"WORKER%d: inconsitent msg size=%ld", thrn, msg_size);
	flushall_msg_parts(qskt);
	free(data);
	send_empty_msg(qskt);
	return -1;
    }
    memcpy(&nb, data, sizeof(uint32_t));
    free(data);
    nb_local = nettohost32(nb);

    switch (cmd) {
    case 1:
	/* send topic, cmd, nb */
	topic_str = strdup(query_topic);
	sendmore_msg_data(pushskt, topic_str , strlen(topic_str), free_fn, NULL);
	sendmore_msg_vsm(pushskt, &cmd, sizeof(uint8_t));
	sendmore_msg_vsm(pushskt, &nb, sizeof(uint32_t));
	
	/* recieve and send hash msg */
	recvsnd(qskt, pushskt, &msg_size, &more, &more_size, NULL, NULL);
	sendmore_msg_vsm(pushskt, &thrn, sizeof(uint8_t));
	
	if (more){
	    /* send id */ 
	    sendmore_msg_vsm(pushskt, &id, sizeof(uint32_t));
	    
	    /* recieve and send  P value, or bytes per row to process */
	    recvsnd(qskt, pushskt, &msg_size, &more, &more_size, (void**)&data, NULL);
	    memcpy(&perms, data, sizeof(uint8_t));
	    free(data);
	    i = 0;
	    while (more && i < nb_local){
		recvsnd(qskt, pushskt, &msg_size, &more, &more_size, NULL, NULL);
		i++;
	    }
	    if (more) flushall_msg_parts(qskt);
	    
	    syslog(LOG_DEBUG,"WORKER%d: cmd = %u, nb = %u, thrn=%u,id=%u,p=%u,rows read=%u",\
                                     thrn, cmd, nb_local, thrn, id, perms,i);

	    /* close out the msg */ 
	    send_empty_msg(pushskt);
	} else {
	    /* send id as last msg*/
	    send_msg_vsm(pushskt, &id, sizeof(uint32_t));
	    syslog(LOG_DEBUG,"WORKER%d: thrn=%u,id=%u", thrn, id);
	}
	waitresults(rskt, &id);
	
	/* retrieve metadata */
	if (id){
	    mdata_inline = retrieve_audiodata(mdata_db, id);
	} else {
	    mdata_inline = strdup(null_str);
	}
	syslog(LOG_DEBUG,"WORKER%d: results id=%u, metadata = %s", thrn, id, mdata_inline);
	
	/* send reply */
	send_msg_data(qskt,  mdata_inline, strlen(mdata_inline)+1, free_fn, NULL);
	break;
    case 2:
	/* submission */
	recieve_msg(qskt, &msg_size, &more, &more_size, (void**)&data);
	size_t data_size = msg_size;

	id = 0;
	if (more){
	    recieve_msg(qskt, &msg_size, &more, &more_size, (void**)&mdata_inline);
	    if (store_audiodata(mdata_db, mdata_inline, &id) == 0){
		int select =  select_table();

		/* send topic, cmd, nb frames */
		topic_str = strdup(submit_topic);
		sendmore_msg_data(pushskt, topic_str, strlen(topic_str), free_fn, NULL);
		sendmore_msg_vsm(pushskt, &cmd, sizeof(uint8_t));
		sendmore_msg_vsm(pushskt, &nb, sizeof(uint32_t));
		sendmore_msg_data(pushskt, data, data_size, free_fn, NULL);
		sid = hosttonet32(id);
		table_n = (uint8_t)select;
		sendmore_msg_vsm(pushskt, &table_n, sizeof(uint8_t));
		send_msg_vsm(pushskt, &sid, sizeof(uint32_t));

	    } else {
		syslog(LOG_ERR,"WORKER%d: unable to store new data", thrn);
		free(data);
	    }
	    syslog(LOG_DEBUG,"cmd=%u,nb=%u,%s, to table %d", cmd, nb_local, mdata_inline, select);
	    if (more) flushall_msg_parts(qskt);
	    free(mdata_inline);
	    mdata_inline = NULL;
	} else {
	    /* no metadata found at end */
	    syslog(LOG_DEBUG,"WORKER%d ERR: cmd=%u,nb=%u, no metadata found", thrn,cmd,nb_local);
	    free(data);
	}
	/* send id reply */
	syslog(LOG_DEBUG,"WORKER%d: send reply, id = %u", thrn, id);
	send_msg_vsm(qskt, &sid, sizeof(uint32_t));
	break;
    default:
	syslog(LOG_DEBUG,"WORKER%d: unrecognized cmd, %u", thrn, cmd);
	flushall_msg_parts(qskt);
	send_empty_msg(qskt);
    }

    return 0;
}

/* parameter struct to pass to dowork thread */
typedef struct worker_param_t {
    void *ctx;
    int thr_n;
} WorkerParam;

void* dowork(void *arg){
    char scratch[32];

    WorkerParam *wp = (WorkerParam*)arg;
    if (!wp){
	syslog(LOG_CRIT,"WORKER ERR: null arg");
	exit(1);
    }
    void *ctx = wp->ctx;
    if (!ctx){
	syslog(LOG_CRIT,"WORKER ERR: null ctx arg");
	exit(1);
    }

    uint8_t threadnum = (uint8_t)(wp->thr_n);

    void *qskt = socket_connect(ctx, ZMQ_REP, INPROC_REQUEST_ADDRESS);
    if (!qskt){
	syslog(LOG_CRIT,"WORKER%d ERR: unable to connect to %s", threadnum,INPROC_REQUEST_ADDRESS);
	exit(1);
    }

    void *pushskt = socket_connect(ctx, ZMQ_PUSH, INPROC_PUBLISH_ADDRESS);
    if (!pushskt){
	syslog(LOG_CRIT,"WORKER%d ERR: unable to connect to %s", threadnum,INPROC_PUBLISH_ADDRESS);
	exit(1);
    }

    snprintf(scratch, 32, "%s%d", INPROC_RESULT_ADDRESS, threadnum);
    void *rskt = socket_bind(ctx, ZMQ_PAIR,scratch);
    if (!rskt){
	syslog(LOG_CRIT,"WORKER%d ERR: unable to bind to %s", threadnum, scratch);
	exit(1);
    }

    AudioDataDB mdata_db = open_audiodata_db(ctx, GlobalArgs.audiodb_addr);
    if (!mdata_db){
	syslog(LOG_CRIT,"WORKER%d ERR: unable to open database", threadnum);
	exit(1);
    }

    while (1){
	if (handle_request(threadnum, qskt, pushskt, rskt, mdata_db) < 0){
	    syslog(LOG_ERR,"WORKER%d ERR: bad request, skipping", threadnum);
	}
    }
    
    /* never reaches here */ 
    return NULL;
}

void result_listener(void *arg){
    char addr[32];
    void *ctx = arg;

    snprintf(addr, 32, "tcp://*:%d", GlobalArgs.port+3);
    void *skt = socket_bind(ctx, ZMQ_REP, addr);
    if (skt == NULL){
	syslog(LOG_CRIT,"RESULTLISTENER ERR: unable to bind to %s", addr);
	return;
    }
    
    void **rskts = (void**)malloc(GlobalArgs.nbthreads*sizeof(void*));
    if (rskts == NULL){
	syslog(LOG_CRIT,"RESULTLISTENER ERR: mem alloc error");
	return;
    }
    int i;
    for (i = 0;i < GlobalArgs.nbthreads;i++){
	snprintf(addr,32,"%s%d", INPROC_RESULT_ADDRESS, i);
	rskts[i] = socket_connect(ctx, ZMQ_PAIR, addr);
	if (rskts[i] == NULL){
	    syslog(LOG_CRIT,"RESULTLISTENER ERR: unable to bin to %s", addr);
	    return;
	}
    }

    uint32_t uid;
    uint8_t thrnum;
    float cs;
    double cs_double;
    int64_t more;
    size_t msg_size, more_size = sizeof(int64_t);
    void *data;
    char *tmpstr;
    while (1){
	data = NULL;
	tmpstr = NULL;

	/* recieve results */ 
	recieve_msg(skt, &msg_size, &more, &more_size, &data);
	if (msg_size != sizeof(uint8_t) || !more){
	    syslog(LOG_DEBUG,"RESULTLISTENER ERR: inconsistent size, not uint8_t");
	    if (more) flushall_msg_parts(skt);
	    send_empty_msg(skt);
	    free(data);
	    continue;
	}
	memcpy(&thrnum, data, sizeof(uint8_t));
	free(data);

	recieve_msg(skt, &msg_size, &more, &more_size, &data);
	if (msg_size != sizeof(uint32_t) || !more){
	    syslog(LOG_DEBUG,"RESULTLISTENER ERR: inconsistent size, not uint32_t");
	    if (more) flushall_msg_parts(skt);
	    send_empty_msg(skt);
	    free(data);
	    continue;
	}
	memcpy(&uid, data, sizeof(uint32_t));
	uid = nettohost32(uid);
	free(data);
	syslog(LOG_DEBUG,"RESULTLISTENER: uid = %u", uid);

	recieve_msg(skt, &msg_size, &more, &more_size, &data);
	if (msg_size != sizeof(float)){
	    syslog(LOG_DEBUG,"RESULTLISTENER ERR: inconsistent size, not float");
	    if (more)flushall_msg_parts(skt);
	    send_empty_msg(skt);
	    free(data);
	    continue;
	}

	if (more)flushall_msg_parts(skt);

	memcpy(&cs, data, sizeof(float));
	free(data);
	cs_double = cs = nettohostf(cs);

	syslog(LOG_DEBUG,"RESULTLISTENER: thrn=%u, uid=%u,cs=%f",thrnum,uid,cs);
	
	/* send response */ 
	tmpstr = strdup(resp_str);
	send_msg_data(skt, tmpstr, strlen(tmpstr)+1, free_fn, NULL);

	if (cs >= GlobalArgs.threshold){
	    /* send result through proper thrn pipe */ 
	    syslog(LOG_DEBUG,"RESULTLISTENER: send uid=%u to %u thread", uid, thrnum);
	    send_msg_vsm(rskts[thrnum], &uid, sizeof(uint32_t));
	}
    }

    return;
}

void* table_listener(void *arg){
    char addr[32];
    void *ctx = arg;

    snprintf(addr, 32, "tcp://*:%d", GlobalArgs.port+1);
    void *skt = socket_bind(ctx, ZMQ_REP, addr);
    if (skt == NULL){
	syslog(LOG_CRIT,"TBLLISTENER: unable to bind to %s", addr);
	exit(1);
    }

    char *init_str = NULL;
    uint8_t *data = NULL;
    uint8_t table_n;
    int64_t more;
    size_t msg_size, more_size = sizeof(int64_t);

    /* init table array */ 
    uint8_t i;
    for (i = 0;i<MAX_TABLES;i++){
	tables[i] = 0;
    }

    while (1){
	table_n = 0xff;

	/* recieve init's from tables */ 
	recieve_msg(skt, &msg_size, &more, &more_size, (void**)&init_str);

	if (!strcmp(init_str, "INIT")){
	    /* find first available table number and mark it */ 
	    pthread_mutex_lock(&tbl_mutex);
	    for (i = 0;i < MAX_TABLES;i++){
		if (tables[i]==0){
		    tables[i] = 1;
		    table_n = i;
		    nb_tables++;
		    break;
		}
	    }
	    pthread_mutex_unlock(&tbl_mutex);
	} else if (!strcmp(init_str,"KILL")){
	    if (more){
		recieve_msg(skt, &msg_size, &more, &more_size, (void**)&data);
		if (msg_size == sizeof(uint8_t)){
		    memcpy(&table_n, data, sizeof(uint8_t));

		    /* mark table number as inactive */
		    syslog(LOG_DEBUG,"TBLLISTENER: turn off table %u", table_n);
		    pthread_mutex_lock(&tbl_mutex);
		    tables[table_n] = 0;
		    nb_tables--;
		    pthread_mutex_unlock(&tbl_mutex);
		} else {
		    syslog(LOG_DEBUG,"TBLISTENER: table number msg wrong size, %ld", msg_size);
		}
		free(data);
	    } else {
		syslog(LOG_DEBUG,"TBLLISTENER: kill str not followed by table number");
	    }
	} else {
	    syslog(LOG_DEBUG,"TBLLISTENER: unrecognized cmd str: %s", init_str);
	}

	syslog(LOG_DEBUG,"TBLLISTENER: %s for %u", init_str, table_n);

	flushall_msg_parts(skt);
	free(init_str);

	/* send out table number */ 
	send_msg_vsm(skt, &table_n, sizeof(uint8_t));
    }

    /* should never reach */
    return NULL;
}


int main(int argc, char **argv){
    char client_addr[32];
    char pub_addr[32];
    int i;
    init_options();
    parse_options(argc, argv);

    if (GlobalArgs.helpflag || !GlobalArgs.audiodb_addr){
	auscout_usage();
	return 0;
    }

    /* init daemon */ 
    init_process();

    /* init zeromq */ 
    void *ctx = zmq_init(1);
    if (!ctx){
	syslog(LOG_CRIT,"MAIN ERROR: unable to get zmq context");
	exit(1);
    }

    /* save ctx  */
    main_ctx = ctx;

    snprintf(client_addr, 32, "tcp://*:%d", GlobalArgs.port);

    DevParam dp_queue;
    dp_queue.ctx = ctx;
    dp_queue.skt1_addr = client_addr;
    dp_queue.skt2_addr = strdup(INPROC_REQUEST_ADDRESS);
    pthread_t queue_thr;
    if (pthread_create(&queue_thr, NULL, do_queue, &dp_queue) != 0){
	syslog(LOG_CRIT,"MAIN ERROR: unable to create queue thread");
	exit(1);
    }
    
    snprintf(pub_addr, 32, "tcp://*:%d", GlobalArgs.port+2);
    DevParam dp_fwder;
    dp_fwder.ctx = ctx;
    dp_fwder.skt1_addr = strdup(INPROC_PUBLISH_ADDRESS);
    dp_fwder.skt2_addr = pub_addr;

    pthread_t fwd_thr;
    if (pthread_create(&fwd_thr, NULL, do_forward, &dp_fwder) != 0){
	syslog(LOG_CRIT,"MAINERROR: unable to create fwder thread");
	exit(1);
    }

    pthread_t table_thr;
    if (pthread_create(&table_thr, NULL, table_listener, ctx) != 0){
	syslog(LOG_CRIT,"MAINERROR: unable to create table_thr");
	exit(1);
    }

    sleep(1);

    pthread_t worker_thr;
    WorkerParam *wp;
    for (i=0;i < GlobalArgs.nbthreads;i++){
	wp = (WorkerParam*)malloc(sizeof(WorkerParam));
	wp->ctx = ctx;
	wp->thr_n = i;
	if (pthread_create(&worker_thr, NULL, dowork, wp) != 0){
	    syslog(LOG_CRIT,"MAINERROR: unable to create worker thr, %d", i);
	    exit(1);
	}
    }

    sleep(1);

    /* result loop */
    result_listener(ctx);
    syslog(LOG_CRIT,"MAIN ERROR: result listener fail");

    return 0;
}
