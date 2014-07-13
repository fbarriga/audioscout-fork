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
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <zmq.h>
#include "zmqhelper.h"
#include "serialize.h"
#include "phash_audio.h"
#include "audiodata.h"

/* simulate this number threads driving tblservd - not actually opening this many threads */
#define NB_THREADS 255

static unsigned int NumberFiles = 0;

/* generates random deviate from poisson distribution around average, lambda */
unsigned int next_arrival(int lambda){
    double L = exp(-lambda);
    double U, P = 1.0;
    unsigned int K = 0;
    do {
	K++;
	U = ((double)rand()+1)/(double)RAND_MAX;
	P = P*U;
    } while (P > L);
    return K-1;
}

/* compute difference in time between timespec's */
struct timespec diff_timespec(struct timespec start, struct timespec end){
    struct timespec diff;
    if (end.tv_nsec - start.tv_nsec < 0){
	diff.tv_sec = end.tv_sec - start.tv_sec - 1;
	diff.tv_nsec = 1000000000+end.tv_nsec- start.tv_nsec;
    } else {
	diff.tv_sec = end.tv_sec - start.tv_sec;
	diff.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return diff;
}

typedef struct thr_param_t {
    void *ctx;
    int port;
}ThrParam;

static const char *query_topic = "phash.q";
static const char *submission_topic = "phash.s";

/* recieve results from table server for the querys */
/* sent from the main thread of this program        */
void* result_listener(void *arg){
    char addr[32];
    ThrParam *thrparam = (ThrParam*)arg;
    void *ctx = thrparam->ctx;
    int port = thrparam->port;

    /* socket to recieve results */
    snprintf(addr, 32, "tcp://*:%d", port);
    void *skt = socket_bind(ctx, ZMQ_REP, addr);
    assert(skt);

    /* connect sockets to pipes opened in main */
    unsigned int i;
    void *rskts[NB_THREADS];
    for (i=0;i<NB_THREADS;i++){
	snprintf(addr,32, "inproc://timers%d", i);
	rskts[i] = socket_connect(ctx, ZMQ_PAIR, addr);
	assert(rskts[i]);
    }

    uint8_t thrdnum;
    uint32_t uid, count = 0;
    int64_t more;
    float cs;
    size_t msg_size, more_size = sizeof(int64_t);
    void *data;
    struct timespec ts, end_ts;
    unsigned long long  item_start, item_end, start_ul=0ULL, end_ul;
    unsigned long long latency, sum = 0ULL, max_latency = 0ULL, min_latency = 9999999ULL;

    /* recieves result messages from table server        */
    /* in multipart message form [ thrdnum | id | cs ]   */
    do {
	/* start recieving a message */
	recieve_msg(skt, &msg_size, &more, &more_size, &data);
	assert(msg_size == sizeof(uint8_t));
	memcpy(&thrdnum, data, sizeof(uint8_t));
	free(data);

	recieve_msg(skt, &msg_size, &more, &more_size, &data);
	assert(msg_size == sizeof(uint32_t));
	memcpy(&uid, data, sizeof(uint32_t));
	uid = nettohost32(uid);
	free(data);

	recieve_msg(skt, &msg_size, &more, &more_size, &data);
	assert(msg_size == sizeof(float));
	memcpy(&cs, data, sizeof(float));
	cs = nettohostf(cs);
	free(data);

	/* respond to table server */
	send_empty_msg(skt);
	
	/* recieve the time the query was sent from the respective pipe */
	recieve_msg(rskts[thrdnum], &msg_size, &more, &more_size, &data);
	assert(msg_size == sizeof(unsigned long long));
	memcpy(&item_start, data, sizeof(unsigned long long));
	free(data);

	/* save start time */ 
	if (count == 0) start_ul = item_start;

	/* get time recieved and track some time stats */
	clock_gettime(CLOCK_MONOTONIC, &ts);
	item_end = 1000000000*ts.tv_sec + ts.tv_nsec;
	latency = item_end - item_start;
	sum += latency;
	if (latency > max_latency) max_latency = latency;
	if (latency < min_latency) min_latency = latency;
	

	fprintf(stdout,"-->Recieve(%d) - %u thrd,%u uid,%f cs in %llu nsecs\n",\
                                           count+1, thrdnum,uid,cs, latency);
    } while (++count < NumberFiles);

    /* get end of time and compute time stats */
    clock_gettime(CLOCK_MONOTONIC, &end_ts);
    end_ul = 1000000000*end_ts.tv_sec + end_ts.tv_nsec;

    unsigned long long total_ul = end_ul - start_ul;
    float total_secs  = (float)total_ul/1000000000.0f;
    float ave_latency = (float)sum/(float)count/1000000000.0f;
    float rate        = (float)count/total_secs;

    float max_latency_f = (float)max_latency/1000000000.0f;
    float min_latency_f = (float)min_latency/1000000000.0f;

    fprintf(stdout,"Recieved %d query results in %f secs ", count, total_secs);
    fprintf(stdout,"at %f querys/sec\n", rate);
    fprintf(stdout,"ave latency %f secs\n", ave_latency);
    fprintf(stdout,"max latency %f secs\n", max_latency_f);
    fprintf(stdout,"min latency %f secs\n", min_latency_f);

    for (i=0;i<NB_THREADS;i++){
	zmq_close(rskts[i]);
    }
    zmq_close(skt);
    pthread_exit(NULL);
}

int table_listener(void *skt){
    fprintf(stdout,"Start/Stop table server now.\n");
    uint8_t tblnum = 0;
    void *data;
    int64_t more;
    size_t msg_size, more_size = sizeof(int64_t);
    recieve_msg(skt, &msg_size, &more, &more_size, &data);
    send_msg_vsm(skt, &tblnum, sizeof(uint8_t));
    free(data);
    return 0;
}

int get_hashes(const char *dir,unsigned int *nbfiles,uint32_t ***hashes,\
                                             uint32_t **nbframes,const int sr){

    char **files = readfilenames(dir, nbfiles);
    assert(files);
    float *sigbuf = (float*)malloc(1<<26);
    assert(sigbuf);
    const unsigned int buflen = (1<<26)/sizeof(float);
    
    fprintf(stdout, "\nreading %u files from %s ...\n", *nbfiles, dir);

    *hashes = (uint32_t**)malloc(*nbfiles*sizeof(uint32_t*));
    assert(hashes);
    *nbframes = (uint32_t*)malloc(*nbfiles*sizeof(uint32_t));
    assert(nbframes);

    AudioHashStInfo *hash_st = NULL;
    uint32_t *hash, nb;
    unsigned int i, tmpbuflen;
    int err;
    for (i=0;i<*nbfiles;i++){
	float nbsecs;
	do {
	    nbsecs = (float)(rand()%60);
	} while (nbsecs < 30.0f);
	tmpbuflen = buflen;
	
	float *buf = readaudio(files[i], sr, sigbuf, &tmpbuflen, nbsecs, NULL, &err);
	assert(buf);

	audiohash(buf, &hash, NULL, NULL, NULL, &nb, NULL, NULL, tmpbuflen, 0, sr, &hash_st);
	(*hashes)[i] = hash;
	(*nbframes)[i] = nb;

	char *name = strrchr(files[i],'/')+1;
	fprintf(stdout,"(%d) %.1f secs of \"%s\"\n", i+1, nbsecs, name);
	if (buf != sigbuf) free(buf);
	free(files[i]);
    }
    fprintf(stdout,"\n");

    ph_hashst_free(hash_st);
    free(files);
    free(sigbuf);
    return 0;
}

int main(int argc, char **argv){
    if (argc < 4){
	fprintf(stdout,"not enough input args\n");
	fprintf(stdout,"usage: progname <cmd> <port> <dirname>\n");
	fprintf(stdout,"   cmd  - 1 for query mode, 2 for submission mode\n");
	fprintf(stdout,"   port - mock auscoutd port - e.g. 4005\n");
	fprintf(stdout,"   dir  - directory of audio files to form queries\n");
	return 0;
    }
    char addr[32];
    const int command = (uint8_t)atoi(argv[1]);
    const int port = atoi(argv[2]);
    const char *dir = argv[3];

    const int sr = 6000;
    const int table_port = port + 1;
    const int result_port = port + 3;
    const int pub_port = port + 2;
    int arr;

    /* seed pseudo-random number generator */
    struct timespec seed_ts;
    clock_gettime(CLOCK_REALTIME, &seed_ts);
    srand(seed_ts.tv_nsec);

    uint32_t **hashes = NULL, *nbframes = NULL;
    unsigned int nbfiles;
    assert(get_hashes(dir, &nbfiles, &hashes, &nbframes, sr)== 0);
    assert(hashes);
    assert(nbframes);


    fprintf(stdout,"\n%u queries from %s\n\n", nbfiles, dir);
    NumberFiles = nbfiles;

    void *ctx = zmq_init(1);
    assert(ctx);

    /* port to publish queries */
    snprintf(addr, 32, "tcp://*:%d", pub_port);
    void *pubskt = socket_bind(ctx, ZMQ_PUB, addr);
    assert(pubskt);
    sleep(1);

    /* inproc sockets to send the times at which the queries are sent to the results thread  */
    unsigned int i;
    void *rskts[NB_THREADS];
    for (i=0;i<NB_THREADS;i++){
	snprintf(addr, 32, "inproc://timers%d", i);
	rskts[i] = socket_bind(ctx, ZMQ_PAIR, addr);
	assert(rskts[i]);
    }
    sleep(1);

    /* socket to listen for table server start and stop */
    snprintf(addr, 32, "tcp://*:%d", table_port);
    void *tblskt = socket_bind(ctx, ZMQ_REP, addr);
    assert(tblskt);
    sleep(1);

    char retchar;
    fprintf(stdout,"\nEnter average inter-arrival time in milliseconds:");
    scanf("%d%c", &arr, &retchar);
    fprintf(stdout,"\nsimulate with %d ms inter-arrival time\n\n", arr);

    table_listener(tblskt);
    sleep(1);
    
    ThrParam resparam;
    resparam.ctx = ctx;
    resparam.port = result_port;
    pthread_t res_thr;
    assert(pthread_create(&res_thr, NULL, result_listener, &resparam) == 0);
    sleep(1);

    int pause = 0;
    uint8_t thrdnum, cmd = command;
    uint32_t uid = 0;
    struct timespec ts, start_ts, end_ts, diff_ts;
    unsigned long long start_ul;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);
    for (i=0;i<nbfiles;i++){
	char *topic = (cmd == 1) ? strdup(query_topic) : strdup(submission_topic);
	
	clock_gettime(CLOCK_MONOTONIC, &ts);
	start_ul = 1000000000*ts.tv_sec + ts.tv_nsec;
	
	send_msg_vsm(rskts[thrdnum], &start_ul, sizeof(unsigned long long));
	
	sendmore_msg_data(pubskt, topic, strlen(topic), free_fn, NULL);
	sendmore_msg_vsm(pubskt, &cmd, sizeof(uint8_t));
	
	uint32_t snbframes = hosttonet32(nbframes[i]);
	sendmore_msg_vsm(pubskt, &snbframes, sizeof(uint32_t));
	sendmore_msg_data(pubskt, hashes[i], nbframes[i]*sizeof(uint32_t), NULL, NULL);
	sendmore_msg_vsm(pubskt, &thrdnum, sizeof(uint8_t));
	
	uint32_t suid = hosttonet32(uid);
	send_msg_vsm(pubskt, &suid, sizeof(uint32_t));
	
	fprintf(stdout,"send(%d) %u thrd, %u uid, %u nbframes\n",i+1,thrdnum,uid,nbframes[i]);
	
	thrdnum++;
	thrdnum = thrdnum%NB_THREADS;
	
	if (i < nbfiles-1){
	    /* pause */
	    pause = arr; /*next_arrival(arr);*/
	    usleep(1000*pause);
	}
	
    }

    clock_gettime(CLOCK_MONOTONIC, &end_ts);
    diff_ts = diff_timespec(start_ts, end_ts);
    unsigned long long total_ns  = 1000000000*diff_ts.tv_sec + diff_ts.tv_nsec;

    float total_secs = (float)total_ns/1000000000.0f;
    float rate       = (float)nbfiles/total_secs;

    fprintf(stdout,"\nSent %u querys in %f secs- %f querys/sec\n",nbfiles,total_secs,rate);
    
    assert(pthread_join(res_thr, NULL)==0);
    
    table_listener(tblskt);
    sleep(1);

    fprintf(stdout,"Done.\n");

    /* cleanup */
    free(hashes);
    free(nbframes);
    zmq_close(pubskt);
    zmq_close(tblskt);
    zmq_term(ctx);
    return 0;
}
