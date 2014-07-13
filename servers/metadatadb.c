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
#include <sqlite3.h>
#include <syslog.h>
#include <assert.h>
#include "zmq.h"
#include "serialize.h"
#include "zmqhelper.h"

#define WORKING_DIR "/tmp"
#define LOG_IDENT  "metadatadb"
#define LOCKFILE "metadatadb.lock"

static const char *opt_string = "w:d:l:p:n:vh?";

static const struct option longOpts[] = {
    { "wd", required_argument, NULL, 'w' },
    { "db", required_argument, NULL, 'd'},
    { "level", required_argument, NULL, 'l' },
    { "port", required_argument, NULL, 'p'},
    { "threads", required_argument, NULL, 'n'},
    { "verbose", no_argument, NULL, 'v'},
    { "help", no_argument, NULL, 'h'},
    { NULL, no_argument, NULL, 0 }
};

struct globalargs_t {
    char *wd;
    char *dbname;
    int level;         /* log level, as defined in syslog.h */
    int port;
    int nbthreads;
    int verboseflag;
    int helpflag;
} GlobalArgs;

void init_options(){
    GlobalArgs.wd = WORKING_DIR;
    GlobalArgs.level = LOG_UPTO(LOG_ERR);
    GlobalArgs.dbname = NULL;
    GlobalArgs.port = 4000;
    GlobalArgs.nbthreads = 10;
    GlobalArgs.verboseflag = 0;
    GlobalArgs.helpflag = 0;
}

void parse_options(int argc, char **argv){
    int longIndex;
    char opt = getopt_long(argc,argv,opt_string, longOpts, &longIndex);
    while (opt != -1){
	switch(opt){
	case 'd':
	    GlobalArgs.dbname = optarg;
	    break;
	case 'w':
	    GlobalArgs.wd = optarg;
	    break;
	case 'l':
	    GlobalArgs.level = LOG_UPTO(atoi(optarg));
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

void metadatadb_usage(){
    fprintf(stdout,"auscoutd [options]\n\n");
    fprintf(stdout,"options:\n");
    fprintf(stdout,"-d --db              name of the path to the db sql file - mandatory\n");
    fprintf(stdout,"-p --port            port to bind on - default 4000\n");
    fprintf(stdout,"-n --threads         number worker threads (default 10)\n");
    fprintf(stdout,"-w --wd              working directory for the server\n");
    fprintf(stdout,"-l --level           log level, 0-7 as defined in syslog.h\n");
    fprintf(stdout,"-v --verbose         print more -- not implemented\n");
    fprintf(stdout,"-h --help            print this usage\n");
    fprintf(stdout,"?  --help            print this usage\n");
}

static int lock_fd;

void kill_process();
void init_process();

void kill_server(){
}

void handle_signal(int sig){

    switch (sig){
    case SIGHUP:
    case SIGTERM:
	syslog(LOG_INFO, "recieved signal, %d", sig);
	kill_server();
	kill_process();
	exit(0);
    }
}

void init_process(){
    int fv,i;

    if (getpid() == 1) {
	fprintf(stderr, "process already running\n");
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
    if (GlobalArgs.wd){
	chdir(GlobalArgs.wd);
    }
    else {
	chdir(WORKING_DIR);
    }

    lock_fd = open(LOCKFILE, O_RDWR|O_CREAT, 0755);
    if (i < 0)exit(1);
    if (lockf(lock_fd, F_TLOCK, 0) < 0) exit(0);

    openlog(LOG_IDENT, LOG_PID, LOG_USER);
    setlogmask(GlobalArgs.level);

    /*first instance only */
    signal(SIGCHLD, SIG_IGN); /* ignore child */ 
    signal(SIGTSTP, SIG_IGN); /* ignore tty signals */ 
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, handle_signal);
    signal(SIGTERM, handle_signal);

    syslog(LOG_INFO, "process init complete");

}
void kill_process(){

    lockf(lock_fd, F_ULOCK, 0);
    close(lock_fd);
    syslog(LOG_INFO,"kill process");
    closelog();
}

static const char delim[4] = { 0x1E, 0x00};
static const char space = 32;

int parse_into_sql(char *mdata, char *sql, int len){
    if (sql == NULL || mdata == NULL || len < 8){
	syslog(LOG_ERR,"PARSETOSQL: arg error");
	return -1;
    }
    char *save_ptr = NULL;
    char *token = NULL;
    char currtoken[256];
    sql[0] = '(';
    sql[1] = '\0';

    /* composer(str) | title(str) | perf(str) | date(str) |  album(str) | genre(str) |*/
    /*   year(int)   | durint)    | part(int) */ 

    token = strtok_r(mdata, delim, &save_ptr);
    int index = 0;
    int curr_index;
    int i;
    while (token){
	/* escape all single quotes in tokens */ 
	curr_index = 0;
	i = 0;
	/* ignore leading whitespace */ 
	while (token[i] < 33 || token[i] == 127) i++;

	/* copy string into currtoken */ 
	for ( ;i < strlen(token)+1;i++){
	    if (token[i] != '\''){
		/* fine, just copy as is */ 
		currtoken[curr_index++] = token[i];
	    } else {
		/* escape single quote with single quote  */ 
		currtoken[curr_index++] = '\'';
		currtoken[curr_index++] = '\'';
	    }
	}
	/* ignore trailing whitespace */
	if (curr_index > 0){
	    while (currtoken[curr_index-1] < 33) curr_index--;
	}
        currtoken[curr_index] = '\0';	

	/* surround appropriate tokens with single quotes */ 
	if (index <= 5) strncat(sql, "'", 1); 
	strncat(sql, currtoken, strlen(currtoken));
	if (index <= 5) strncat(sql, "'", 1);

	index++;
	token = strtok_r(NULL, delim, &save_ptr);
	strncat(sql, ",", strlen(","));
    }
    strncat(sql, "datetime('now')", 15);
    strncat(sql, ")",1);

    return 0;
}

static const char *colstr = "composer,title,performer,date,album,genre,year,dur,part,time";

uint32_t insert_db(sqlite3 *db, char *inline_str){
    uint32_t row_id = 0;
    int err;
    sqlite3_stmt *ppdstmt = NULL;

    const int blen = 1024;
    char *sql = (char*)malloc(blen);
    char *rowstr = (char*)malloc(blen);
    if (!sql || !rowstr){
	syslog(LOG_CRIT,"memalloc error");
	return row_id;
    }

    parse_into_sql(inline_str, rowstr, blen);
    snprintf(sql, blen, "INSERT INTO trcks (%s) VALUES %s ;", colstr, rowstr);
    
    syslog(LOG_DEBUG,"inline: %s", inline_str);
    syslog(LOG_DEBUG,"rowstr: %s", rowstr);
    syslog(LOG_DEBUG,"sql   : %s", sql);

    /* execute sql statement */
    err = sqlite3_prepare_v2(db, sql, strlen(sql)+1, &ppdstmt, NULL);
    if (err == SQLITE_OK){
	err = sqlite3_step(ppdstmt);
	row_id = (uint32_t)sqlite3_last_insert_rowid(db);
	syslog(LOG_DEBUG, "statement executed - rowid %d", row_id);
    }else {
	syslog(LOG_ERR,"unable to prepare statement, %d", err);
    }
    sqlite3_finalize(ppdstmt);
    free(sql);
    free(rowstr);

    return row_id;
}

char* lookup_db(sqlite3 *db, uint32_t uid){
    char sql[512];
    sql[0] = '\0';
    char rowstr[512];
    rowstr[0] = '\0';
    char **presulttable = NULL, *errstr = NULL;
    int err, nrows = 0, ncols = 0;
    
    snprintf(sql, 512, "SELECT %s FROM trcks WHERE uid == %u;", colstr, uid);
    syslog(LOG_DEBUG, "sql: %s", sql);
	
    /* execute sql */
    err = sqlite3_get_table(db, sql, &presulttable, &nrows, &ncols, &errstr);
    if (err != SQLITE_OK){
	syslog(LOG_ERR,"unable to execute sql, err: %s", errstr);
	if (errstr) sqlite3_free(errstr);
    }

    /* read table results into rowstr */
    if (nrows > 0 && ncols > 0){
	unsigned int i;
	for (i=0;i<ncols-1;i++){
	    if (presulttable[ncols+1] != NULL && strlen(presulttable[ncols+i]) > 0){
		strncat(rowstr, presulttable[ncols+i], strlen(presulttable[ncols+i]));
	    } else {
		strncat(rowstr, " ", 1);
	    }
	    strncat(rowstr, " " , 1);
	    strncat(rowstr, delim, 1);
	    strncat(rowstr, " " , 1);
	}
	strncat(rowstr, presulttable[ncols+i], strlen(presulttable[ncols+i]));
    }
	
    /* send result  */ 
    char *result = strdup(rowstr);
    sqlite3_free_table(presulttable);

    return result;
}

int handle_request(void *skt, sqlite3 *db){
    char sql[512];
    char rowstr[512];
    const int len = 512;
    char *errstr, *mdata_inline = NULL;
    uint8_t cmd, *data = NULL;
    uint32_t uid = 0;
    int err = 0;
    int64_t more;
    size_t msg_size, more_size = sizeof(int64_t);

    recieve_msg(skt, &msg_size, &more, &more_size, (void**)&data);
    if (msg_size != sizeof(uint8_t) || !more){
	if (more) flushall_msg_parts(skt);
	free(data);
	send_empty_msg(skt);
	return -1;
    }
    memcpy(&cmd, data, sizeof(uint8_t));
    free(data);

    switch (cmd){
    case 1: /* insert new entry */
	recieve_msg(skt, &msg_size, &more, &more_size, (void**)&mdata_inline);
	if (mdata_inline){
	    uid = insert_db(db, mdata_inline);
	    free(mdata_inline);
	}
	syslog(LOG_DEBUG, "assigned uid = %u", uid);
	uid = hosttonet32(uid);
	send_msg_vsm(skt, &uid, sizeof(uint32_t));
	break;
    case 2: /* look up id */ 
	rowstr[0] = '\0';
	recieve_msg(skt, &msg_size, &more, &more_size, (void**)&data);
	if (msg_size != sizeof(uint32_t)){
	    free(data);
	    send_empty_msg(skt);
	    err = -2;
	    break;
	}
	memcpy(&uid, data, sizeof(uint32_t));
	uid = nettohost32(uid);
	char *result = lookup_db(db, uid);
	send_msg_data(skt, result, strlen(result)+1, free_fn, NULL);
	break;
    default:
	syslog(LOG_DEBUG, "unrecognized cmd, %u", cmd);
	send_empty_msg(skt);
    }
    if (more)flushall_msg_parts(skt);

    return err;
   
}

void* dowork(void *arg){
    sqlite3 *db;
    int err = sqlite3_open_v2(GlobalArgs.dbname, &db,\
              SQLITE_OPEN_READWRITE|SQLITE_OPEN_SHAREDCACHE, NULL);
    if (db == NULL || err != SQLITE_OK){
	syslog(LOG_CRIT,"ERROR: unable to open db, error = %d",err);
	exit(1);
    }

    void *ctx = arg;
    if (!ctx){
	syslog(LOG_CRIT,"ERROR: no zmq context available to thread");
	exit(1);
    }

    void *skt = socket_connect(ctx, ZMQ_REP, "inproc://pipe");
    if (!skt){
	syslog(LOG_CRIT,"ERROR: no socket available for thread");
	exit(1);
    }

    while (1){
	err = handle_request(skt, db);
	if (err < 0){
	    syslog(LOG_DEBUG,"dowork: msg non-conformant to msg sizes, err=%d",err);
	}
    }
    
    return NULL;
}

int main(int argc, char **argv){
    char addr[32];

    init_options();
    parse_options(argc, argv);

    if (GlobalArgs.helpflag || GlobalArgs.dbname == NULL){
	metadatadb_usage();
	return 0;
    }

    /* init daemon */ 
    init_process();
    
    /* init zeromq */ 
    void *ctx = zmq_init(1);
    if (ctx == NULL){
	syslog(LOG_CRIT,"MAIN ERROR: unable to init zmq context");
	exit(1);
    }

    snprintf(addr, 32, "tcp://*:%d", GlobalArgs.port);
    void *clients_skt = socket_bind(ctx, ZMQ_XREP, addr);
    if (clients_skt == NULL){
	syslog(LOG_CRIT,"MAIN ERROR: unable to bind skt to %s", addr);
	exit(1);
    }

    void *workers_skt = socket_bind(ctx, ZMQ_XREQ, "inproc://pipe");
    if (workers_skt == NULL){
	syslog(LOG_CRIT,"MAIN ERROR: unable to bind workers skt");
	exit(1);
    }

    unsigned int i;
    pthread_t worker_thr;
    for (i=0;i < GlobalArgs.nbthreads;i++){
	assert(pthread_create(&worker_thr, NULL, dowork, ctx) == 0);
    }

    assert(zmq_device(ZMQ_QUEUE, clients_skt, workers_skt) == 0);

    return 0;
}
