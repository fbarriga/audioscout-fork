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
#include <assert.h>
#include "audiodata.h"


static const char *testfile = "./testdir/sample.mp3";
static const char *amrtestfile = "./testdir/amr-1.amr";
static const char *amrtestfile2 = "./testdir/amr-2.amr";

int main(int argc, char **argv){
  float *sigbuf = (float*)malloc(1<<28);
  const unsigned int buflen = (1<<28)/sizeof(float);

  AudioMetaData mdata;
  float *buf = NULL;
  unsigned int len;

  float nbsecs = 30.0f; /* full audio */ 
  int sr = 5512;
  int error, i;

  len = buflen;

  printf("testing  %s @ sr %d, for %f seconds...\n", testfile, sr, nbsecs);
  buf = readaudio(testfile, sr, sigbuf, &len, nbsecs, &mdata, &error);
  assert(buf);
  assert(len == 165359 || len == 165360);
  assert(mdata.title2);
  assert(strchr(mdata.title2, 'B') < mdata.title2 + strlen(mdata.title2)) ;
  assert(mdata.composer);
  assert(strchr(mdata.composer, 'T') < mdata.composer + strlen(mdata.composer));
  printf("ok\n");

  free_mdata(&mdata);
  if (buf != sigbuf) free(buf);


  buf = NULL;
  sr = 8000;
  nbsecs = 60.0f;
  len = buflen;

  printf("testing %s @ sr = %d, for %f seconds...\n", testfile, sr, nbsecs);
  buf = readaudio(testfile, sr, sigbuf, &len, nbsecs, &mdata, &error);
  assert(buf);
  assert(len == 479999 || len == 480000);
  printf("ok\n");

  free_mdata(&mdata);
  if (buf != sigbuf) free(buf);


  buf = NULL;
  sr = 11025;
  nbsecs = 0.0f;
  len = buflen;

  printf("testing %s @ sr = %d for %f seconds...\n", testfile, sr, nbsecs);
  buf = readaudio(testfile, sr, sigbuf, &len, nbsecs, &mdata, &error);
  assert(buf);
  assert(len == 671771);
  printf("ok\n");
  
  free_mdata(&mdata);
  if (buf != sigbuf) free(buf);

  buf = NULL;
  sr = 6000;
  nbsecs = 0.0f;
  len = buflen;

  printf("testing %s @ sr = %d for %f seconds...\n", amrtestfile, sr, nbsecs);
  buf = readaudio(amrtestfile, sr, sigbuf, &len, nbsecs, &mdata, &error);
  assert(buf);
  assert(len == 38520);
  printf("samples %d\n", len);
  printf("ok\n");

  free_mdata(&mdata);
  if (buf != sigbuf) free(buf);

  buf = NULL;
  sr = 8000;
  nbsecs = 0.0f;
  len = buflen;
  printf("testing %s @ sr = %d for %f seconds...\n", amrtestfile2, sr, nbsecs);
  buf = readaudio(amrtestfile2, sr, sigbuf, &len, nbsecs, &mdata, &error);
  assert(buf);
  assert(len == 58400);
  printf("samples %d\n", len);
  printf("ok\n");

  free_mdata(&mdata);
  if (buf != sigbuf) free(buf);

  printf("done\n");
  free(sigbuf);

  return 0;
}
