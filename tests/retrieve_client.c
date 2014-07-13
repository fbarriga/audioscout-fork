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
#include <zmq.h>
#include "serialize.h"


int main(int argc, char **argv){

    if (argc < 3){
	printf("not enough input args\n");
	exit(1);
    }
    const char *addr = argv[1];
    const int N = atoi(argv[2]);

    void *ctx = zmq_init(1);
    if (ctx == NULL){
	printf("unable to init zmq\n");
	exit(1);
    }
    void *skt = zmq_socket(ctx, ZMQ_REQ);
    if (skt == NULL){
	printf("unable to obtain socket\n");
	exit(1);
    }
    int rc = zmq_connect(skt, addr);
    if (rc){
	printf("unable to connect socket to %s\n", addr);
	exit(2);
    }
    

    zmq_msg_t cmd_msg, uid_msg, mdata_msg;
    char *mdatastr;
    uint32_t cmd, uid = 0, suid;
    unsigned int i;
    for (i=0;i<N;i++){
	uid++;
	printf("looking up %u ...\n", uid);
	
	cmd = 2;
	cmd = hosttonet32(cmd);
	suid = hosttonet32(uid);

	zmq_msg_init_size(&cmd_msg, sizeof(uint32_t));
	memcpy(zmq_msg_data(&cmd_msg), &cmd, sizeof(uint32_t));
	zmq_send(skt, &cmd_msg, ZMQ_SNDMORE);

	zmq_msg_init_size(&uid_msg, sizeof(uint32_t));
	memcpy(zmq_msg_data(&uid_msg), &suid, sizeof(uint32_t));
	zmq_send(skt, &uid_msg, 0);

	zmq_msg_init(&mdata_msg);
	zmq_recv(skt, &mdata_msg, 0);
	
	mdatastr = (char*)zmq_msg_data(&mdata_msg);
	printf("result: %s\n", mdatastr);

	zmq_msg_close(&cmd_msg);
	zmq_msg_close(&uid_msg);
	zmq_msg_close(&mdata_msg);
    }

    return 0;
}
