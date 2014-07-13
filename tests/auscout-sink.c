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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <zmq.h>
#include <pthread.h>
#include "serialize.h"
#include "zmqhelper.h"

static int table_number = 0;

static const char *init_str = "INIT";


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

int register_table(void *ctx, const char *address){
    void *skt = zmq_socket(ctx, ZMQ_REQ);
    assert(skt);
    assert(zmq_connect(skt, address) == 0);

    char *str = strdup(init_str);
    send_msg_data(skt, str, strlen(str)+1, free_fn, NULL);

    int64_t more;
    size_t msg_size, more_size = sizeof(int64_t);
    void *data;
    recieve_msg(skt, &msg_size, &more, &more_size, &data);
    if (msg_size != sizeof(uint8_t)){
	return -1;
    }
    memcpy(&table_number, data, sizeof(uint8_t));
    free(data);
    if (more) flushall_msg_parts(skt);
    assert(zmq_close(skt) == 0);

    return 0;
}

int main(int argc, char **argv){
    char addr[32];
    if (argc < 4){
	fprintf(stdout,"not enough args\n");
	fprintf(stdout,"usage: prog <auscoutd address>  <port> <port>\n");
	fprintf(stdout,"address - mock auscoud server address, e.g. \"localhost\"\n");
	fprintf(stdout,"port    - mock auscoutd server port e.g. 4005\n");
	fprintf(stdout,"port    - port to recieve init from auscout driver, e.g. 4009\n");
	return 0;
    }

    /* progname <auscoutd address> <auscoutd port> <driver port>*/
    const char *auscoutd_address = argv[1];
    const int port = atoi(argv[2]);
    const int driver_port = atoi(argv[3]);

    int subscr_port  = port + 2;
    int result_port = port + 3;
    int table_port  = port + 1;

    snprintf(addr, 32, "tcp://%s:%d", auscoutd_address, table_port);
    fprintf(stdout,"register as table with %s\n", addr);

    void *ctx = zmq_init(1);
    assert(ctx);
    assert(register_table(ctx, addr) == 0);

    /* skt to recieve number of files to be sent from auscout driver program */
    snprintf(addr, 32, "tcp://*:%d", driver_port);
    void *prepskt = socket_bind(ctx, ZMQ_PAIR, addr);
    assert(prepskt);

    snprintf(addr, 32, "tcp://%s:%d", auscoutd_address, subscr_port);
    void *subscr_skt = socket_connect(ctx, ZMQ_SUB, addr);
    assert(subscr_skt);
    assert(zmq_setsockopt(subscr_skt, ZMQ_SUBSCRIBE, "phash.", 6) == 0);

    snprintf(addr, 32, "tcp://%s:%d", auscoutd_address, result_port);
    void *result_skt = socket_connect(ctx, ZMQ_REQ, addr);
    assert(result_skt);

    struct timespec seed_ts;
    clock_gettime(CLOCK_REALTIME, &seed_ts);
    srand(seed_ts.tv_nsec);

    uint8_t cmd, thrdnum, tblnum;
    struct timespec start_ts, end_ts, diff_ts;
    uint32_t uid, nbframes, count = 0, nbquerys;
    void *data;
    int64_t more;
    size_t msg_size, more_size = sizeof(int64_t);
    float cs = 0.05555f;
    fprintf(stdout,"\nready to recieve subscription messages from auscoutd ...\n\n");
    while (1){

	recieve_msg(prepskt, &msg_size, &more, &more_size, &data);
	assert(msg_size == sizeof(uint32_t));
	memcpy(&nbquerys, data, sizeof(uint32_t));
	free(data);
	fprintf(stdout,"expecting %d querys\n\n", nbquerys);

	clock_gettime(CLOCK_MONOTONIC, &start_ts);
	count = 0;
	do {
	    /* recieve topic */
	    recieve_msg(subscr_skt, &msg_size, &more, &more_size, &data);
	    free(data);

	    /* recieve cmd */
	    recieve_msg(subscr_skt, &msg_size, &more, &more_size, &data);
	    memcpy(&cmd, data, sizeof(uint8_t));
	    free(data);
	
	    recieve_msg(subscr_skt, &msg_size, &more, &more_size, &data);
	    memcpy(&nbframes, data, sizeof(uint32_t));
	    nbframes = nettohost32(nbframes);
	    free(data);

	    /* recieve hash */
	    recieve_msg(subscr_skt, &msg_size, &more, &more_size, &data);
	    free(data);

	    recieve_msg(subscr_skt, &msg_size, &more, &more_size, &data);
	    memcpy(&thrdnum, data, sizeof(uint8_t));
	    free(data);

	    recieve_msg(subscr_skt, &msg_size, &more, &more_size, &data);
	    memcpy(&uid, data, sizeof(uint32_t));
	    uid = nettohost32(uid);
	    free(data);

	    if (cmd == 1){
		/* recieved lookup */
		uid = 0;  /* send back random integer id*/
		cs =  0.5555;
		fprintf(stdout,"(%d) lookup: %u frames\n",count,nbframes);
		fprintf(stdout,"        reply : thrdnum %u | uid %u | cs %f\n\n",thrdnum,uid,cs);

		uid = hosttonet32(uid);
		cs = hosttonetf(cs);
		sendmore_msg_vsm(result_skt, &thrdnum, sizeof(uint8_t));
		sendmore_msg_vsm(result_skt, &uid, sizeof(uint32_t));
		send_msg_vsm(result_skt, &cs, sizeof(float));
		
		recieve_msg(result_skt, &msg_size, &more, &more_size, &data);
		free(data);
	    } else if (cmd == 2){
		tblnum = thrdnum;
		
		/* recieved submission */
		fprintf(stdout,"(%d) submit: %u frames | %u table\n",count,nbframes,tblnum);
		fprintf(stdout,"         no reply\n\n");
	    }
	    fprintf(stdout,"**************************************\n");


	} while (++count < nbquerys);
	
	clock_gettime(CLOCK_MONOTONIC, &end_ts);
	diff_ts = diff_timespec(start_ts, end_ts);

	unsigned long long total_ull = 1000000000*diff_ts.tv_sec + diff_ts.tv_nsec;
	float total_secs = (float)total_ull/1000000000.0f;
	float rate = (float)nbquerys/total_secs;

	fprintf(stdout,"recieved %d querys in %f secs - %f querys/sec\n",\
                                                            nbquerys, total_secs, rate);

    }

    zmq_close(subscr_skt);
    zmq_close(result_skt);
    zmq_close(prepskt);
    zmq_term(ctx);

    return 0;
}
