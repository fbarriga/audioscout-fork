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
#include <wctype.h>
#include <wchar.h>
#include <unistd.h>
#include <stdint.h>
#include <zmq.h>
#include "audiodata.h"
#include "phash_audio.h"
#include "serialize.h"
#include "zmqhelper.h"


int main(int argc, char **argv){
    if (argc < 6){
	printf("not enough input args\n");
	printf("usage: %s dir server_addr command nbsecs P\n", argv[0]);
	printf("   dir - name of direcotry from which you want to take files from\n");
	printf("   server_addr - address of auscoutd server - e.g. tcp://localhost:4005\n");
	printf("   command     - command to auscoutd,  1 for query, 2 for submit\n");
	printf("   nbsecs      - number of secs from files, starting from beginning of file \n");
	printf("   P           - number permutations to consider in hash lookups \n");
	exit(1);
    }
    wchar_t wcs_str[512];
    const char *dir_name = argv[1];
    const char *server_address = argv[2];             /*tcp://localhost:5000 */
    const uint8_t command = (uint8_t)atoi(argv[3]);    /* cmd = 1 query, cmd = 2 submissions */
    const float nbsecs = atof(argv[4]);
    const unsigned int P = atoi(argv[5]);

    if (nbsecs < 0.0f){
	fprintf(stdout,"bad nbsecs parameter - cannot be less than 0\n");
	exit(1);
    }

    if (command < 1 || command > 2){
	fprintf(stdout,"bad commmand parameter - only 1 or 2\n");
	exit(1);
    }

    if (fwide(stdout, 0)!=0 && fwide(stdout, 1) <= 0){
	fprintf(stdout,"ERROR: unable to set wchar support.\n");
	exit(1);
    }
    mbstowcs(wcs_str, dir_name, 256);
    unsigned int nbfiles;
    char **files = readfilenames(dir_name, &nbfiles);
    if (files == NULL){
	fwprintf(stdout,L"unable to read file names in %ls\n", wcs_str);
	exit(1);
    }

    fwprintf(stdout,L"cmd = %d,  %u files in %ls\n", command, nbfiles, wcs_str);

    const int sr = 6000;
    AudioMetaData mdata;

    float *sigbuf = (float*)malloc(1<<26);
    const unsigned int buflen = (1<<26)/sizeof(float);
    if (!sigbuf){
	fwprintf(stdout,L"mem alloc error\n");
	exit(1);
    }

    AudioHashStInfo *hash_st = NULL;

    void *ctx = zmq_init(1);
    if (ctx == NULL){
	fwprintf(stdout,L"unable to init zeromq\n");
	exit(1);
    }

    void *skt = zmq_socket(ctx, ZMQ_REQ);
    if (skt == NULL){
	fwprintf(stdout,L"unable to create zmq socket\n");
	exit(1);
    }

    printf("connect skt to %s\n", server_address);
    int rc = zmq_connect(skt, server_address);
    if (rc != 0){
	mbstowcs(wcs_str, server_address, 256);
	fwprintf(stdout,L"unable to connect to %ls\n", server_address);
	exit(1);
    }

    uint8_t cmd = command;
    char mdata_inline[512];
    uint32_t nbframes = 0, snbframes = 0, uid = 0, *hash = NULL;
    int error;
    unsigned int i, j, k;
    char *result_str;
    uint8_t *data;
    uint8_t **toggles = NULL;
    for (i=0;i<nbfiles;i++){
	mdata_inline[0] = '\n';
	unsigned int tmpbuflen = buflen;
	char *name = strrchr(files[i], '/') + 1;

	mbstowcs(wcs_str, name, 512);
	fwprintf(stdout,L"(%d) %ls\n", i, wcs_str);

	float *buf = readaudio(files[i], sr, sigbuf, &tmpbuflen, nbsecs, &mdata, &error);
	if (buf == NULL){
	    fwprintf(stdout,L"unable to read file - error %d\n\n",error);
	    continue;
	}

	if (audiohash(buf, &hash, NULL, &toggles, NULL, &nbframes, NULL, NULL,\
                         tmpbuflen, P, sr, &hash_st) < 0){
	    fwprintf(stdout,L"unable to get hash\n\n");
	    if (buf != sigbuf) ph_free(buf);
	    continue;
	}

	for (j=0;j<nbframes;j++){
	    hash[j] = hosttonet32(hash[j]);
	}
	fwprintf(stdout,L"    %d hash frames\n\n", nbframes);

	if (buf != sigbuf)ph_free(buf);

	uint8_t perms = (uint8_t)P;
	int64_t more;
	size_t msg_size, more_size = sizeof(int64_t);
	snbframes = hosttonet32(nbframes);
	if (cmd == 1) { /* query */
	    /* send query */ 
	    fwprintf(stdout,L"Sending query ...\n\n");

	    sendmore_msg_vsm(skt, &cmd, sizeof(uint8_t));
	    sendmore_msg_vsm(skt, &snbframes, sizeof(uint32_t));
	    sendmore_msg_data(skt, hash, nbframes*sizeof(uint32_t), free_fn, NULL);
	    if (P > 0 && toggles){
		sendmore_msg_vsm(skt, &perms, sizeof(uint8_t));
		for (k = 0;k < nbframes;k++){
		    sendmore_msg_data(skt, toggles[k], P*sizeof(uint8_t), free_fn, NULL);
		}
	    }
	    send_empty_msg(skt);

	    /* recieve response */ 
	    recieve_msg(skt, &msg_size, &more, &more_size, (void**)&result_str);

	    mbstowcs(wcs_str, result_str, 512);
	    fwprintf(stdout,L"Recieved: %ls\n\n", wcs_str);
	    free(result_str);
	} else if (cmd == 2){ /* submission */ 
	    if (metadata_to_inlinestr(&mdata, mdata_inline, 512) < 0){
		fwprintf(stdout,L"unable to parse mdata struct\n\n");
		continue;
	    }

	    mbstowcs(wcs_str, mdata_inline, 512);
	    fwprintf(stdout,L"Sending metadata: %ls\n\n", wcs_str);
 
            /* send */ 
	    sendmore_msg_vsm(skt, &cmd, sizeof(uint8_t));
	    sendmore_msg_vsm(skt, &snbframes, sizeof(uint32_t));
	    sendmore_msg_data(skt, hash, nbframes*sizeof(uint32_t), free_fn, NULL);
	    send_msg_data(skt, mdata_inline, strlen(mdata_inline)+1, NULL, NULL);

	    /* recieve response */
	    uid = 0;
	    recieve_msg(skt, &msg_size, &more, &more_size, (void**)&data);
	    if (msg_size == sizeof(uint32_t)){
		memcpy(&uid, data, sizeof(uint32_t));
	    } 
	    uid = nettohost32(uid);

	    
	    fwprintf(stdout,L"Recieved: id = %u\n", uid);
	}

	if (command == 1){
	    fwprintf(stdout,L"***********Hit Enter for next**********************\n\n");
	    getchar();
	} else {
	    fwprintf(stdout,L"***************************************************\n\n");
	    sleep(3);
	}
	free_mdata(&mdata);
    }
    
    /* cleanup */
    ph_hashst_free(hash_st);
    free(sigbuf);
    zmq_close(skt);
    zmq_term(ctx);

    for (i=0;i<nbfiles;i++){
	free(files[i]);
    }
    free(files);

    return 0;
}
