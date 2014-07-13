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
#include "audiodata.h"
#include "phash_audio.h" 


static const char delim = 30; /* RS */
static const char space = 32; /* space */


#define APPEND_DELIM(x) strncat(x, &space, 1); strncat(x, &delim, 1); strncat(x, &space, 1)
#define APPEND_SPACE(x) strncat(x, &space, 1)

int metadata_to_inlinestr(AudioMetaData *mdata, char *str, int len){
    if (str == NULL || len < 256 || mdata == NULL) return -1;

    /* composer(str) | title(str) | perf(str) | date(str) |  album(str) | genre(str)|*/
    /*   year (int)  | dur (int)  | part(int) */ 
    char year[5];
    snprintf(year, 5, "%d", mdata->year);
    char dur[5];
    snprintf(dur, 5, "%d", mdata->duration);
    char part[5];
    snprintf(part, 5, "%d", mdata->partofset);

    str[0] = '\0';
    if (mdata->composer) strncat(str, mdata->composer, len);

    APPEND_DELIM(str);

    if (mdata->title1) strncat(str, mdata->title1, len);
    APPEND_SPACE(str);
    if (mdata->title2) strncat(str, mdata->title2, len);
    APPEND_SPACE(str);
    if (mdata->title3) strncat(str, mdata->title3, len);

    APPEND_DELIM(str);

    if (mdata->tpe1) strncat(str, mdata->tpe1, len);
    APPEND_SPACE(str);
    if (mdata->tpe2) strncat(str, mdata->tpe2, len);
    APPEND_SPACE(str);
    if (mdata->tpe3) strncat(str, mdata->tpe3, len);
    APPEND_SPACE(str);
    if (mdata->tpe4) strncat(str, mdata->tpe4, len);

    APPEND_DELIM(str);

    if (mdata->date) strncat(str, mdata->date, len);

    APPEND_DELIM(str);

    if (mdata->album) strncat(str, mdata->album, len);

    APPEND_DELIM(str);

    if (mdata->genre) strncat(str, mdata->genre, len);

    APPEND_DELIM(str);

    strncat(str, year, len);

    APPEND_DELIM(str);

    strncat(str, dur,len);

    APPEND_DELIM(str);

    strncat(str, part, len);

    return 0;
}

int main(int argc, char **argv){
    const int sr = 6000;
    const float nbsecs = 5.0;

    if (argc < 3){
	printf("not enough input args\n");
	exit(1);
    }
   
    const char *dirname = argv[1];
    const char *addr = argv[2];

    unsigned int nbfiles;
    char **files = readfilenames(dirname, &nbfiles);
    if (files == NULL){
	printf("unable to read filenames from %s\n", dirname);
	exit(1);
    }
    printf("read %u files in %s and put into db at %s\n", nbfiles, dirname, addr);

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
    
    float *sigbuf = (float*)malloc(1<<18);
    unsigned int buflen = (1<<18)/sizeof(float);

    char *mdata_str = (char*)malloc(512);
    int len = 512;

    AudioMetaData mdata;
    zmq_msg_t cmd_msg, mdata_msg;
    zmq_msg_t uid_msg;
    int err;
    uint32_t cmd, uid;
    unsigned int i;
    for (i=0;i<nbfiles;i++){
	printf("file[%u] = %s\n", i, files[i]);
	unsigned int tmpbuflen = buflen;
	float *buf = readaudio(files[i], sr, sigbuf, &tmpbuflen, nbsecs, &mdata, &err);
	if (buf == NULL){
	    printf("unable to read audio\n");
	    continue;
	}
	/* send cmd msg */ 
	cmd = 1;
	cmd = hosttonet32(cmd);
	zmq_msg_init_size(&cmd_msg, sizeof(uint32_t));
	memcpy(zmq_msg_data(&cmd_msg), &cmd, sizeof(uint32_t));
	zmq_send(skt, &cmd_msg, ZMQ_SNDMORE);

	if (metadata_to_inlinestr(&mdata, mdata_str, len) < 0){
	    printf("error in parsing metadata into inline string\n");
	    continue;
	}
	printf("metadata: %s\n", mdata_str);

	zmq_msg_init_data(&mdata_msg, mdata_str, len, NULL, NULL);
	zmq_send(skt, &mdata_msg, 0);

	/* wait for response */ 
	zmq_msg_init(&uid_msg);
	zmq_recv(skt, &uid_msg, 0);
	memcpy(&uid, zmq_msg_data(&uid_msg), sizeof(uint32_t));
	uid = nettohost32(uid);
	printf("uid is %u\n", uid);

	zmq_msg_close(&cmd_msg);
	zmq_msg_close(&mdata_msg);
	zmq_msg_close(&uid_msg);

	if (buf != sigbuf) free(buf);
	free_mdata(&mdata);
    }

    printf("done\n");
    free(sigbuf);
    for (i = 0;i<nbfiles;i++){
	free(files[i]);
    }
    free(files);


    return 0;
}
