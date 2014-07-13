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
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#ifdef _WIN32
#define BUILD_EXE
#endif

#include "phash_audio.h"
#include "audiodata.h"

int main (int argc, char **argv){
  const int sr = 6000;
  const float nbsecs = 0.0f;

  if (argc < 2){
    printf("not enough input args\n");
    return -1;
  }

  const char *dirname = argv[1];
  printf("dirname: %s\n", dirname);


  unsigned int nbfiles;
  char **files = readfilenames(dirname, &nbfiles);
    
  printf("nbfiles %u\n", nbfiles);

  unsigned int buflen = 0;

  AudioHashStInfo *hash_st = NULL;

  clock_t total = 0;
  unsigned int i,j;
  int err;
  for (i=0;i<nbfiles;i++){
    printf("file[%u]: %s\n", i, files[i]);
    
    float *buf = readaudio(files[i], sr, NULL, &buflen, nbsecs, NULL, &err);
    if (buf == NULL){
      printf("unable to read audio: error number %d\n",err);
      break;
    }
    printf("buf %p, len %u\n", buf, buflen);

    clock_t start = clock();
    uint32_t *hash = NULL;
    unsigned int nbframes;
    if (audiohash(buf, &hash, NULL, NULL, NULL, &nbframes, NULL, NULL, buflen, 0, sr,&hash_st) < 0){
      printf("unable to get hash \n");
      break;
    }

    clock_t end = clock();
    clock_t dur = end - start;
    total += dur;
    double secs = (double)dur/(double)CLOCKS_PER_SEC;
    printf("hash %p, nbframes %u, %f seconds\n", hash, nbframes,secs);

    ph_free(hash);
    ph_free(buf);
  }

  ph_hashst_free(hash_st);

  double average_seconds = (double)total/(double)nbfiles/(double)CLOCKS_PER_SEC;
  printf("ave hash time %f seconds\n", average_seconds);
  for (i = 0;i<nbfiles;i++){
    free(files[i]);
  }
  free(files);

  return 0;
}
