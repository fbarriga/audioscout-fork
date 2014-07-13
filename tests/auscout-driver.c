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
#include <math.h>
#include <zmq.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include "audiodata.h"
#include "phash_audio.h"
#include "serialize.h"
#include "zmqhelper.h"


/* generate random deviate of poisson distribution */
/* around average, lambda, to be used as an inter-arrival time */
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


static int Arr = 500; /* arrival delta in millisecs */

/* parameter to the query_thread thread */
typedef struct thread_param {
    void *ctx;
    char *mdatastr;
    uint32_t **hashes;
    uint32_t *nbframes;
    char *address;
    int nbfiles;
    int thrn;
    int cmd;
} ThrParam;

void* query_thread(void *arg){
    ThrParam *thrparam = (ThrParam*)arg;
    void *ctx = thrparam->ctx;
    char *mdatastr = thrparam->mdatastr;
    uint32_t **hashes = thrparam->hashes;
    uint32_t *pnbframes = thrparam->nbframes;
    unsigned int nbfiles = thrparam->nbfiles;
    uint8_t thrn = thrparam->thrn;
    uint8_t cmd = thrparam->cmd;

    void *skt = zmq_socket(ctx, ZMQ_REQ);
    assert(skt);
    fprintf(stdout,"thrn%d: connect to %s\n", thrn, thrparam->address);
    assert(zmq_connect(skt, thrparam->address)==0);

    unsigned int i,j;

    void *data;
    int64_t more;
    size_t msg_size, more_size=sizeof(int64_t);
    struct timespec t1_ts, t2_ts, diff_ts;
    unsigned long long latency, sum_ull = 0ULL;

    if (cmd == 1){ /* queries */
	for (i=0;i<nbfiles;i++){
	    clock_gettime(CLOCK_MONOTONIC, &t1_ts);

	    sendmore_msg_vsm(skt, &cmd, sizeof(uint8_t));
	    uint32_t snbframes = hosttonet32(pnbframes[i]);
	    sendmore_msg_vsm(skt, &snbframes, sizeof(uint32_t));
	    send_msg_data(skt, hashes[i], pnbframes[i]*sizeof(uint32_t), free_fn, NULL);
	    recieve_msg(skt, &msg_size, &more, &more_size, &data);
	    
	    clock_gettime(CLOCK_MONOTONIC, &t2_ts);
	    diff_ts = diff_timespec(t1_ts, t2_ts);
	    latency = 1000000000*diff_ts.tv_sec + diff_ts.tv_nsec;
	    sum_ull += latency;

	    fprintf(stdout,"thrd%d: query %u frames, recv:\"%s\" in %llu nsecs\n",\
                                                       thrn,pnbframes[i],(char*)data, latency);
	    free(data);

	    /* simulate interarrival */ 
	    unsigned int pause = Arr; /* = next_arrival(Arr);*/
	    usleep(1000*pause);
	}
    } else if (cmd == 2){ /* submissions */
	for (i=0;i<nbfiles;i++){
	    clock_gettime(CLOCK_MONOTONIC, &t1_ts);

	    sendmore_msg_vsm(skt, &cmd, sizeof(uint8_t));
	    uint32_t snbframes = hosttonet32(pnbframes[i]);
	    sendmore_msg_vsm(skt, &snbframes, sizeof(uint32_t));
	    sendmore_msg_data(skt, hashes[i], pnbframes[i]*sizeof(uint32_t), free_fn, NULL);
	    send_msg_data(skt, mdatastr, strlen(mdatastr)+1, NULL, NULL);

	    recieve_msg(skt, &msg_size, &more, &more_size, &data);
	    assert(msg_size == sizeof(uint32_t));
	    uint32_t uid;
	    memcpy(&uid, data, sizeof(uint32_t));
	    uid = nettohost32(uid);

	    clock_gettime(CLOCK_MONOTONIC, &t2_ts);

	    diff_ts = diff_timespec(t1_ts, t2_ts);
	    latency = 1000000000*diff_ts.tv_sec + diff_ts.tv_nsec;
	    sum_ull += latency;

	    fprintf(stdout,"thr%d: submit %u frames, recv %u=uid in %llu nsecs\n",\
                                                              thrn, pnbframes[i], uid, latency);

	    /* simulate interarrival */
	    unsigned int pause = Arr; /*next_arrival(Arr);*/
	    usleep(1000*pause);

	    free(data);
	}
    }

    free(thrparam);
    zmq_close(skt);

    /* return the sum of latency's */ 
    void *ptr = malloc(sizeof(unsigned long long));
    assert(ptr);
    memcpy(ptr, &sum_ull, sizeof(unsigned long long));
    pthread_exit(ptr);
}

int main(int argc, char **argv){
    char addr[32];

    if (argc < 8){
	printf("not enough input args\n");
	printf("usage: progname <cmd> <nbthreads> <server address> <nbquerys>\n");
	printf("    cmd            - 1 for query, 2 for file submission\n");
	printf("    nbthreads      - number of driver threads\n");
	printf("    arr            - ave inter-arrival time (millisecs)\n");
	printf("    server address - address of auscoutd,  e.g. \"localhost\"\n");
	printf("    sink address   - e.g. tcp://localhost:port\n");
	printf("    port           - port of the auscoutd server, e.g. 4005\n");
	printf("    nbquerys       - total number queries to submit\n");
	return 0;
    }

    /*args [cmd | nbthreads | arr | server address | nbquerys ] */
    const int cmd = (uint8_t)atoi(argv[1]);
    const int nbthreads = atoi(argv[2]);
    Arr = atoi(argv[3]);
    char *auscoutd_address = argv[4];
    char *ausink_address = argv[5];
    const int port = atoi(argv[6]);
    const int nbquerys = atoi(argv[7]);

    snprintf(addr, 32, "tcp://%s:%d", auscoutd_address, port);

    void *ctx = zmq_init(1);
    assert(ctx);

    unsigned int nb_files_per_thread = nbquerys/nbthreads;
    unsigned int nb_files_last_thread = nbquerys%nbthreads;

    fprintf(stdout,"\nfiles per thread %u\n"   , nb_files_per_thread);
    fprintf(stdout, "files in last thread %u\n", nb_files_last_thread);
    fprintf(stdout, "nb threads %d\n\n"        , nbthreads);

    uint32_t **hashes = (uint32_t**)malloc(nbquerys*sizeof(uint32_t*));
    uint32_t *ptrNbframes = (uint32_t*)malloc(nbquerys*sizeof(uint32_t));;

    /* socket to send init signal to ausink */
    void *prepskt = socket_connect(ctx, ZMQ_PAIR, ausink_address);
    assert(prepskt);

    struct timespec seed;
    clock_gettime(CLOCK_REALTIME, &seed);
    srand(seed.tv_nsec);

    struct timespec start_ts, end_ts;
    
    AudioMetaData phonymdata;
    init_mdata(&phonymdata);
    phonymdata.composer = "composer";
    phonymdata.title2 = "title";
    phonymdata.tpe1 = "performer";
    phonymdata.date = "date";
    phonymdata.year = 2010;
    phonymdata.album = "album";
    phonymdata.genre = "genre";
    phonymdata.duration = 1000000;
    phonymdata.partofset = 1;
    
    char mdata_inlinestr[512];
    metadata_to_inlinestr(&phonymdata, mdata_inlinestr, 512);
   
    int i, err;
    if (cmd == 1 || cmd == 2){ /* querys */

	for (i = 0;i < nbquerys;i++){
	    uint32_t nbframes = rand()%12500;
	    uint32_t *hash    = (uint32_t*)malloc(nbframes*sizeof(uint32_t));
	    assert(hash);

	    hashes[i] = hash;
	    ptrNbframes[i] = nbframes;
	}

	/* start of send time*/
	clock_gettime(CLOCK_MONOTONIC, &start_ts);

	/* send init signal with number of files sending to ausc sink */
	uint32_t nb = nbquerys;
	send_msg_vsm(prepskt, &nb, sizeof(uint32_t));

       	fprintf(stdout, "send to auscoutd\n");
	ThrParam *thrparam;
	pthread_t *thrs = (pthread_t*)malloc((nbthreads+1)*sizeof(pthread_t));
	assert(thrs);
	for (i=0;i<nbthreads;i++){
	    thrparam = (ThrParam*)malloc(sizeof(ThrParam));
	    assert(thrparam);
	    thrparam->ctx = ctx;
	    thrparam->cmd = cmd;
	    thrparam->thrn = i;
	    thrparam->mdatastr = mdata_inlinestr;
	    thrparam->nbfiles = nb_files_per_thread;
	    thrparam->address = addr;
	    thrparam->hashes = hashes + i*nb_files_per_thread;
	    thrparam->nbframes = ptrNbframes + i*nb_files_per_thread;
	    assert(pthread_create(&thrs[i], NULL, query_thread, thrparam) == 0);
	}
	
	if (nb_files_last_thread){
	    thrparam = (ThrParam*)malloc(sizeof(ThrParam));
	    assert(thrparam);
	    thrparam->ctx = ctx;
	    thrparam->cmd = cmd;
	    thrparam->thrn = nbthreads;
	    thrparam->mdatastr = mdata_inlinestr;
	    thrparam->nbfiles = nb_files_last_thread;
	    thrparam->address = addr;
	    thrparam->hashes = hashes + nbthreads*nb_files_per_thread;
	    thrparam->nbframes = ptrNbframes + nbthreads*nb_files_per_thread;
	    assert(pthread_create(&thrs[nbthreads], NULL, query_thread, thrparam) == 0);
	}

	/* wait for threads */
	unsigned long long sum_ull, *ptr_ave = NULL;
	for (i=0;i < nbthreads;i++){
	    pthread_join(thrs[i], (void**)&ptr_ave);
	    sum_ull += *ptr_ave;
	}
	if (nb_files_last_thread){
	    pthread_join(thrs[nbthreads], (void**)&ptr_ave);
	    sum_ull += *ptr_ave;
	}

	/* end of sending */
	clock_gettime(CLOCK_MONOTONIC, &end_ts);

	struct timespec total_ts = diff_timespec(start_ts, end_ts);
	unsigned long long  total = 1000000000*total_ts.tv_sec + total_ts.tv_nsec;
	float total_secs = (float)total/1000000000.0f;
	float ave_latency  = (float)sum_ull/(float)nbquerys/1000000000.0f;
	float rate = (float)nbquerys/total_secs;
	fprintf(stdout,"ave latency, %f secs\n", ave_latency);
	fprintf(stdout,"rate: %f querys/sec\n", rate);
	fprintf(stdout,"total time, %f secs\n", total_secs);

    } else {
	fprintf(stdout,"unrecognized cmd, %u\n", cmd);
    }

    zmq_close(prepskt);
    zmq_term(ctx);
    return 0;
}
