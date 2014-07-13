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
#include <assert.h>
#include <math.h>
#include "phash_audio.h"


#define TESTDIR "./testdir"
#define TESTFILE "audioindextestfile"

#define PI 3.141592654

static const int sr = 8000;

void test_audiohash(){
  const int duration = 30;
  unsigned int i, j, nbfreqs = 100, step, nbframes, nbcoeffs, P = 3;
  unsigned int nbsamples = duration*sr;
  float freq;

  /* create a sample signal */ 
  float *sig = (float*)malloc(nbsamples*sizeof(float));
  assert(sig);

  for (i=0;i<nbsamples;i++){
    sig[i] = 0.0;
    freq = 20;
    step = 50;
    for (j=0;j<nbfreqs;j++){
      sig[i] += cos(2*PI*freq*i/sr);
      freq += step;
      step *= 2;
    }
    sig[i]/= nbfreqs;
  }
   
  uint32_t *phash = NULL;
  double **coeffs = NULL;
  uint8_t **toggles = NULL;
  double minB, maxB;
 
  AudioHashStInfo *hash_st = NULL;

  int res = audiohash(sig, &phash, &coeffs, &toggles, &nbcoeffs, &nbframes, &minB, &maxB,\
		      nbsamples, P, sr, &hash_st);
  assert(res==0);
  assert(phash);
  assert(coeffs);
  assert(toggles);
  assert(nbframes == 3719);
  assert(nbcoeffs == 33);

  /* cleanup */ 
  free(sig);
  free(phash);
  for (i=0;i<nbframes;i++){
    free(coeffs[i]);
    free(toggles[i]);
  }
  free(hash_st);
  free(coeffs);
  free(toggles);

}
void generate_hashes(uint32_t ***hashes, unsigned int nbhashes,unsigned int hashlength){
  unsigned int i,j;
  (*hashes) = (uint32_t**)malloc(nbhashes*sizeof(uint32_t*));
  for (i=0;i<nbhashes;i++){
    (*hashes)[i] = (uint32_t*)malloc(hashlength*sizeof(uint32_t));
    for (j=0;j<hashlength;j++){
      (*hashes)[i][j] = rand();
    }
  }
}

void simple_test(){

  const unsigned int nbbuckets = 1024; /* should be power of 2 */ 
  const unsigned int hashlength = 22000;
  const unsigned int nbhashes =  100;
  unsigned int i;
  int res;

  fprintf(stdout,"generate %u hashes, each %u in length\n", nbhashes, hashlength);
  uint32_t **hashes = NULL;
  generate_hashes(&hashes, nbhashes, hashlength);

  AudioIndex index = open_audioindex(TESTFILE, 1, nbbuckets);
  assert(index);

  fprintf(stdout,"insert hashes ...\n");
  for (i=0;i<nbhashes;i++){
    res = insert_into_audioindex(index, i, hashes[i], hashlength);
    assert(res == 0);
  }

  uint32_t id;
  float cs;
  fprintf(stdout,"retrieve hashes ...\n");
  for (i=0;i<nbhashes;i++){
    res = lookupaudiohash(index, hashes[i], NULL, hashlength, 0, 256,0.04, &id, &cs);
    assert(res == 0);
  }
  fprintf(stdout,"done\n");
  res = close_audioindex(index, 1);
  assert(res == 0);

  for (i=0;i<nbhashes;i++){
    free(hashes[i]);
  }
  free(hashes);
 
}

void io_test(){

  const unsigned int nbhashes = 1000, hashlength = 5000;
  const int nbbuckets = 1024;
  uint32_t **hashes = NULL;
  generate_hashes(&hashes, nbhashes, hashlength);

  AudioIndex index = open_audioindex(TESTFILE, 1, nbbuckets);
  assert(index);

  int res;
  uint32_t i;
  for (i=0;i<nbhashes;i++){
    res = insert_into_audioindex(index, i, hashes[i], hashlength);
    assert(res == 0);
  }
  int total_bkts, total_entries; 
  stat_audioindex(index, &total_bkts, &total_entries);
  assert(total_bkts >= 1024);  
  assert(total_entries > 0);

  res = flush_audioindex(index,  TESTFILE);
  assert(res == 0);
  
  res = close_audioindex(index, 1);
  assert(res == 0);

  index = open_audioindex(TESTFILE, 0, 0);
  assert(index != NULL);

  int bkts, entries;
  stat_audioindex(index, &bkts, &entries);
  assert(bkts == total_bkts);
  assert(entries == total_entries);

  uint32_t id;
  float cs;
  for (i = 0;i < nbhashes;i+=10){
    res = lookupaudiohash(index, hashes[i], NULL, hashlength, 0, 256, 0.03, &id, &cs);
    assert(res==0);
  }

  res = close_audioindex(index, 0);
  assert(res == 0);


  /* cleanup */ 
  for (i=0;i<nbhashes;i++){
    free(hashes[i]);
  }
  free(hashes);

}

int main(int argc, char **argv){


  printf("test read filenames in dir\n");
  
  unsigned int nbfiles = 0;
  char **files = readfilenames(TESTDIR, &nbfiles);
  assert(nbfiles == 3);
  
  printf("test audio hash\n");
  test_audiohash();
  printf("simple test\n");
  simple_test();
  printf("io test\n");
  io_test();
  printf("done\n");

  return 0;
}
