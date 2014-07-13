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
#include <stdint.h>
#include <unistd.h>
#include <zmq.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include "serialize.h"
#include "audiodata.h"
#include "phash_audio.h" 
#include "zmqhelper.h"

/* inter-arrival rate in milliseconds */
static unsigned int Arr = 500; 


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

/* parameter to pass to threads */
typedef struct worker_param_t {
    void *ctx;                /* zmq context */
    char *mdatastr;          /* meta data string */
    char *address;            /* server address of metadatadb server */
    unsigned int start_file;  /* uid to start at (for query mode) */
    unsigned int nbfiles;     /* number querys/files */
    int thrn;                 
} WThrParam;


/* thread function to do queries */
void* query_thread(void *arg){
    WThrParam *wp = (WThrParam*)arg;
    assert(wp);
    void *ctx = wp->ctx;
    assert(ctx);

    void *skt = socket_connect(ctx, ZMQ_REQ, wp->address);
    assert(skt);

    uint8_t cmd = 2; /* cmd to query the database */
    uint32_t i;
    void *data;
    int64_t more;
    size_t msg_size, more_size = sizeof(int64_t);
    struct timespec t1_ts, t2_ts, diff_ts;
    unsigned long sum_ull = 0ULL, latency_ull;
    for (i = wp->start_file; i < wp->start_file + wp->nbfiles;i++){

	clock_gettime(CLOCK_MONOTONIC, &t1_ts);

	/* send [cmd | uid ] */
	sendmore_msg_vsm(skt, &cmd, sizeof(uint8_t));
	send_msg_vsm(skt, &i, sizeof(uint32_t)); 

	/* recieve string [metadata string] */
	recieve_msg(skt, &msg_size, &more, &more_size, &data);

	clock_gettime(CLOCK_MONOTONIC, &t2_ts);
	diff_ts = diff_timespec(t1_ts, t2_ts);
	latency_ull = 1000000000*diff_ts.tv_sec + diff_ts.tv_nsec;
	sum_ull += latency_ull;

	fprintf(stdout,"thr%d: query for uid=%u\n", wp->thrn,i);
	fprintf(stdout,"====> in %llu nsecs, %s\n", latency_ull, (char*)data);
	free(data);

	/* simulate interarrivals for thread */
	unsigned int pause = Arr;
	usleep(pause*1000);
    }

    /* copy the summation of latency times to a return */
    void *ptr = malloc(sizeof(unsigned long long));
    assert(ptr);
    memcpy(ptr, &sum_ull, sizeof(unsigned long long));

    /* cleanup */
    free(wp->address);
    free(wp);
    zmq_close(skt);

    pthread_exit(ptr);
}


/* thread function to perform submissions */
void* submit_thread(void *arg){
    WThrParam *wp = (WThrParam*)arg;
    assert(wp);
    assert(wp->ctx);
    assert(wp->address);
    assert(wp->mdatastr);

    void *skt = socket_connect(wp->ctx, ZMQ_REQ, wp->address);
    assert(skt);

    int i;
    uint8_t cmd = 1; /* cmd for metadatadb server submission */
    int64_t more;
    size_t msg_size, more_size = sizeof(int64_t);
    uint32_t uid;    
    void *data;      
    struct timespec t1_ts, t2_ts, diff_ts; 
    unsigned long long sum_ull = 0ULL, ave_latency;
    for (i=0;i<wp->nbfiles;i++){

	clock_gettime(CLOCK_MONOTONIC, &t1_ts);

	/* send submission */
	sendmore_msg_vsm(skt, &cmd, sizeof(uint8_t));
	send_msg_data(skt, wp->mdatastr, strlen(wp->mdatastr)+1, NULL, NULL);

	/* recieve uid response */
	recieve_msg(skt, &msg_size, &more, &more_size, &data);
	assert(msg_size == sizeof(uint32_t));
	memcpy(&uid, data, sizeof(uint32_t));
	uid = nettohost32(uid);
	
	clock_gettime(CLOCK_MONOTONIC, &t2_ts);
	diff_ts = diff_timespec(t1_ts,t2_ts);
	ave_latency = 1000000000*diff_ts.tv_sec + diff_ts.tv_nsec;
	sum_ull += ave_latency;

	fprintf(stdout,"thrd%d: submit, %s\n", wp->thrn, wp->mdatastr);
	fprintf(stdout,"====>  in  %llu nsecs, uid = %u\n", ave_latency, uid);
	free(data);

	/* simulate interarrivals for thread */
	unsigned int pause = Arr;
	usleep(1000*pause);
    }

    /* cleanup */
    zmq_close(skt);
    free(wp->mdatastr);
    free(wp->address);
    free(wp);

    /* return sum of latency times to main thread */
    void *ptr = malloc(sizeof(unsigned long long));
    assert(ptr);
    memcpy(ptr, &sum_ull, sizeof(unsigned long long));
    pthread_exit(ptr);
}

int main(int argc, char **argv){
    if (argc < 6){
	printf("not enough input args\n");
	printf("usage: progr <cmd> <nbthreads> <server addr> <nb>\n\n");
	printf(" cmd            - 1 for submit test, 2 for query test\n");
	printf(" nbthreads      - number driver threads\n");
	printf(" arr            - interarraival rate (in milliseconds)\n");
	printf(" server address - address of metadata server tcp://addr:port\n");
	printf(" nb             - number of options to simulate\n");
	return 0;
    }

    const uint8_t cmd = (uint8_t)atoi(argv[1]);
    const unsigned int nbthreads = atoi(argv[2]);
    Arr = atoi(argv[3]);
    const char *addr = argv[4];
    const unsigned int nb = atoi(argv[5]);

    char mdata_str[512];

    AudioMetaData testmdata;
    init_mdata(&testmdata);
    testmdata.composer = "artist";
    testmdata.title2 = "title";
    testmdata.tpe1 = "performer";
    testmdata.date = "September 21, 2010";
    testmdata.year = 2010;
    testmdata.album = "album";
    testmdata.genre = "genre";
    testmdata.duration = 999999999;
    testmdata.partofset = 1;
    metadata_to_inlinestr(&testmdata, mdata_str, 512);
    printf("test metadata string: %s\n", mdata_str);

    /* obtain zmq context */
    void *ctx = zmq_init(1);
    assert(ctx);

    struct timespec seed_ts;
    clock_gettime(CLOCK_MONOTONIC, &seed_ts);
    srand(seed_ts.tv_nsec);

    int i, err;
    WThrParam *wp;
    struct timespec start_ts, end_ts, total_ts;
    unsigned long long sum_ull = 0ULL, *ptr_ave = NULL;
    pthread_t *thr = (pthread_t*)malloc(nbthreads*sizeof(pthread_t));
    assert(thr);
    unsigned int nb_per_thread, nb_last_thread;
    if (cmd == 1){
	printf("submit %d entries to %s\n", nb, addr);

	/* divide up the files in directory for each thread */
	nb_per_thread = nb/nbthreads;
	nb_last_thread = nb%nbthreads;

	printf("divide work into %d threads ", nbthreads);
	printf(" %d per thread, %d in last thread\n", nb_per_thread, nb_last_thread);

	clock_gettime(CLOCK_MONOTONIC, &start_ts);
	/* create threads */
	for (i=0;i < nbthreads;i++){
	    wp = (WThrParam*)malloc(sizeof(WThrParam));
	    wp->ctx = ctx;
	    wp->address = strdup(addr);
	    wp->mdatastr = strdup(mdata_str);
	    wp->nbfiles = nb_per_thread;
	    wp->thrn = i;
	    assert(pthread_create(&thr[i], NULL, submit_thread, wp) == 0);
	} 

	/* last thread gets remainder if any left */
	if (nb_last_thread){
	    wp = (WThrParam*)malloc(sizeof(WThrParam));
	    assert(wp);
	    wp->ctx = ctx;
	    wp->address = strdup(addr);
	    wp->mdatastr = strdup(mdata_str);
	    wp->nbfiles = nb_last_thread;
	    wp->thrn = nbthreads;
	    assert(pthread_create(&thr[nbthreads], NULL, submit_thread, wp) == 0);
	}    

	/* wait to join threads and retrieve the sum of latency values from each thread */

	for (i=0;i < nbthreads;i++){
	    assert(pthread_join(thr[i], (void**)&ptr_ave) == 0);
	    sum_ull += (*ptr_ave);
	}
	if (nb_last_thread){
	    assert(pthread_join(thr[nbthreads], (void**)&ptr_ave) == 0);
	    sum_ull += (*ptr_ave);
	}
	clock_gettime(CLOCK_MONOTONIC, &end_ts);

	float ave_latency = (float)sum_ull/(float)nb/1000000000.0f;
	fprintf(stdout,"ave latency, %f secs\n", ave_latency);
    } else if (cmd == 2){
	/* divide up the querys into each thread */
	nb_per_thread = nb/nbthreads;
	nb_last_thread = nb%nbthreads;

	fprintf(stdout,"query for uid = 1 to %d from %s\n\n", nb, addr);
	fprintf(stdout,"divide into %d threads ", nbthreads);
	fprintf(stdout," %d per thread, %d for last thread\n", nb_per_thread,nb_last_thread);

	clock_gettime(CLOCK_MONOTONIC, &start_ts);
	/* start each thread */
	for (i=0;i < nbthreads;i++){
	    wp = (WThrParam*)malloc(sizeof(WThrParam));
	    assert(wp);
	    wp->ctx = ctx;
	    wp->address = strdup(addr);
	    wp->nbfiles = nb_per_thread;
	    wp->start_file = i*nb_per_thread + 1;
	    wp->thrn = i;
	    assert(pthread_create(&thr[i], NULL, query_thread, wp) == 0);
	}
	
	if (nb_last_thread) {
	    wp = (WThrParam*)malloc(sizeof(WThrParam));
	    assert(wp);
	    wp->ctx = ctx;
	    wp->address = strdup(addr);
	    wp->nbfiles = nb_last_thread; 
	    wp->start_file = nbthreads*nb_per_thread + 1;
	    wp->thrn = i;
	    assert(pthread_create(&thr[nbthreads], NULL, query_thread, wp) == 0);
	}

	/* wait to join each thread and get the sums of latency values for each thread */
	for (i = 0; i < nbthreads;i++){
	    assert(pthread_join(thr[i], (void**)&ptr_ave) == 0);
	    sum_ull += *ptr_ave;
	}
	if (nb_last_thread){
	    assert(pthread_join(thr[nbthreads], (void**)&ptr_ave) == 0);
	    sum_ull += *ptr_ave;
	}
	clock_gettime(CLOCK_MONOTONIC, &end_ts);
	
	/* compute average latency */
	float ave_latency = (float)sum_ull/(float)nb/1000000000.0f;
	fprintf(stdout,"ave latency, %f secs\n", ave_latency);
    } else {
	/* error - cmd not recognized */
	fprintf(stdout,"cmd unrecognized, %u\n", cmd);
    }

    total_ts = diff_timespec(start_ts, end_ts);

    float total_secs = (float)total_ts.tv_sec + (float)total_ts.tv_nsec/1000000000.0f;
    float rate = (float)nb/total_secs;

    fprintf(stdout,"sent %d querys in %f secs - %f querys/sec\n",nb,total_secs, rate);
    printf("done\n");

    /* cleanup */
    zmq_term(ctx);

    return 0;
}
