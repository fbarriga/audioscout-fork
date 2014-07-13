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
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "./table-4.3.0phmodified/table.h"
#include "fft.h"
#include "phash_audio.h"
#include <stdio.h>

#ifdef __unix__
#include <dirent.h>
#include <unistd.h>
#endif



#define TOGGLE_BIT(word,b)     (0x80000000 >> b)^word

static const unsigned int nfilts = 33;
static const double BarkWidth = 1.06;

static const double BarkFreqs[33] = {   50.0,  75.0,  100.0,  125.0,  150.0,   200.0,   250.0,   300.0, 
                                       350.0, 400.0,  450.0,  510.0,  570.0,   635.0,   700.0,   770.0, 
                                       840.0, 920.0, 1000.0, 1085.0, 1170.0,  1270.0,  1370.0,  1485.0, 
                                      1600.0,1725.0, 1850.0, 2000.0, 2150.0,  2325.0,  2500.0,  2700.0, 
                                      2900.0
                                     };

#ifndef JUST_AUDIOHASH

PHASH_EXPORT
AudioIndex open_audioindex(const char *idx_file, int add, int nbbuckets){

    int error;
    AudioIndex audio_index = NULL;
    if (add){
	audio_index = (AudioIndex)table_read(idx_file, &error);
	if (error != TABLE_ERROR_NONE){
	    audio_index = table_alloc(nbbuckets, &error);
	    if (error != TABLE_ERROR_NONE){
		return NULL;
	    }
	}
	/*adjusting table is NOT concurrent! */
	/*table_attr(audio_index, TABLE_FLAG_AUTO_ADJUST); */
    } else {
	audio_index = table_mmap(idx_file, &error);
	if (error != TABLE_ERROR_NONE){
	    audio_index = table_alloc(nbbuckets, &error);
	    if (error != TABLE_ERROR_NONE){
		return NULL;
	    }
	    table_write(audio_index, idx_file, 0755);
	    table_free(audio_index);
	    audio_index = table_mmap(idx_file, &error);
	    if (error != TABLE_ERROR_NONE){
		return NULL;
	    }
	}
    }

    return audio_index;
}

PHASH_EXPORT
int merge_audioindex(const char *dst_idxfile, const char *src_idxfile){
    /* merge table in src_idxfile into dst_idxfile - clear source */ 
    int ret = 0, err, nb_bkts, nb_entries, key_size, data_size;
    struct stat src_info;
    table_t *srctbl, *dsttbl;
    void *pkey, *pdata;
    table_linear_t linear_st;

    if (!dst_idxfile || !src_idxfile) return -1;
    stat(src_idxfile, &src_info);

    if (!stat(src_idxfile, &src_info) && src_info.st_size > 0) {
	/* open source  table */
	srctbl = open_audioindex(src_idxfile, 1, 0);
	if (srctbl == NULL) return -3;
	table_attr(srctbl, 0);

	err = table_info(srctbl, &nb_bkts, &nb_entries);
	
	if (nb_entries > 0){
	    /* open destination table */
	    dsttbl = open_audioindex(dst_idxfile, 1, 0);
	    if (dsttbl == NULL) {
		table_free(srctbl);
		return -2;
	    }
	    table_attr(dsttbl, 0);

	    /* do merge */
	    err = table_first_r(srctbl, &linear_st, &pkey, &key_size, &pdata, &data_size);
	    while (err == TABLE_ERROR_NONE){
		err = table_insert_kd(dsttbl, pkey, key_size, pdata, data_size, NULL, NULL, 0);
		if (err != TABLE_ERROR_NONE && err != TABLE_ERROR_OVERWRITE){
		    ret = -4;
		    break;
		}
		err = table_next_r(srctbl, &linear_st, &pkey, &key_size, &pdata,&data_size);
	    }
	    table_clear(srctbl);
	    table_write(srctbl, src_idxfile, 0755);
	    table_write(dsttbl, dst_idxfile, 0755);
	    table_free(dsttbl);
	}
	table_free(srctbl);
    } else {
	ret = 1;
    }

    return ret;
}

PHASH_EXPORT
int close_audioindex(AudioIndex audioindex, int add){
    int error;
    if (add){
	error = table_free((table_t*)audioindex);
    } else {
	error = table_munmap((table_t*)audioindex);
    }
    if (error != TABLE_ERROR_NONE){
	return -1;
    }
    return 0;
}

PHASH_EXPORT
int insert_into_audioindex(AudioIndex audio_index, uint32_t id, uint32_t *hash, int nbframes){
	const char ovrwrt = 0;
	TableValue entry;
	int err, i;

	entry.id = id;
	for (i=0;i<nbframes;i++){
	    entry.pos = (uint32_t)i;
	    err = table_insert_kd((table_t*)audio_index,&(hash[i]),sizeof(uint32_t), \
                                    &entry,sizeof(TableValue),\
                                    NULL, NULL, ovrwrt);
	    if (err != TABLE_ERROR_NONE && err != TABLE_ERROR_OVERWRITE){
		return -1;
	    }
	}
	return 0;
}

PHASH_EXPORT
int stat_audioindex(AudioIndex audio_index, int *nbbuckets, int *nbentries){
    table_info((table_t*)audio_index, nbbuckets, nbentries);
    return 0;
}

PHASH_EXPORT
int flush_audioindex(AudioIndex audio_index, const char *filename){
    int error = table_write((table_t*)audio_index, filename, 00755);
    if (error != TABLE_ERROR_NONE){
	return -1;
    }
    return 0;
}

PHASH_EXPORT
int grow_audioindex(AudioIndex audio_index, const float load){
    int nbbuckets, nbentries, error;
    double current_load;

    stat_audioindex(audio_index, &nbbuckets, &nbentries);
    current_load = (double)nbentries/(double)nbbuckets;
    if (current_load > load) {
	error = table_adjust((table_t*)audio_index, 0); /* adjust to number of entries */ 
    }

    return 0;
}

#endif /* JUST_AUDIOHASH */ 

#ifndef _WIN32

PHASH_EXPORT
char** readfilenames(const char *dirname,unsigned int *nbfiles){

    struct dirent *dir_entry;
    DIR *dir = opendir(dirname);
    if (!dir){
        return NULL;
    }

    /*count files */
    /* counts all entries as file names except for . and .. */
    /* so far, no other way to test if an entry is a dir or a file */
    /* so, some names returned could still be directories  */ 
    *nbfiles = 0;
    while ((dir_entry = readdir(dir)) != NULL){
      if ( strcmp(dir_entry->d_name, ".") && strcmp(dir_entry->d_name,"..")){
	    (*nbfiles)++;
	}
    }

    /* alloc list of files */
    char **files = (char**)malloc((*nbfiles)*sizeof(char*));
    if (!files){
	closedir(dir);
	return NULL;
    }
    int index = 0;
    char path[FILENAME_MAX];
    path[0] = '\0';
    rewinddir(dir);
  
    /* read in files names */ 
    while ((dir_entry = readdir(dir)) != 0){
      if ( strcmp(dir_entry->d_name,".") && strcmp(dir_entry->d_name,"..")){
	    strncat(path, dirname, strlen(dirname));
	    strncat(path,SEPARATOR , 1);
	    strncat(path, dir_entry->d_name, strlen(dir_entry->d_name));
	    files[index++] = strdup(path);
	    path[0]='\0';
       }
    }

    closedir(dir);
    return files;
}

#endif /* _WIN32 */


PHASH_EXPORT
void ph_free(void * ptr){
  free(ptr);
}


PHASH_EXPORT
void ph_hashst_free(AudioHashStInfo *ptr){
  int i;
  if (ptr != NULL){
      for (i=0;i<nfilts;i++){
	  free(ptr->wts[i]);
      }
      free(ptr->wts);
      free(ptr->window);
      free(ptr);
  }
}

static double** GetWts(const sr, const int nfft_half){
    double **wts = (double**)malloc(nfilts*sizeof(double*));

    int i, j;
    float maxfreq = (float)(sr/2);
    double f_bark_mid, mdouble, bark_diff, lof, hif;
    double *binbarks = (double*)malloc(nfft_half*sizeof(double));
    if (binbarks == NULL) return NULL;

    for (i=0; i < nfft_half;i++){
	/* frequency of point on axis */
	double temp = i*maxfreq/(double)nfft_half;

	/* asinh implentation freq => bark number conversion */
	/* approx of: bark = 6*arcsinh(freq/600)             */
	temp /= 600.0;
	binbarks[i] = 6*log(temp + sqrt(temp*temp + 1.0));
    }

    for (i=0;i < nfilts;i++){
      wts[i] = (double*)malloc(nfft_half*sizeof(double));
      if (wts[i] == NULL) return NULL;

      /*calculate wts for each filter */
      f_bark_mid = BarkFreqs[i]/600.0;
      f_bark_mid = 6*log(f_bark_mid + sqrt(f_bark_mid*f_bark_mid + 1.0));
      for (j=0;j < nfft_half ;j++){
	  bark_diff = binbarks[j] - f_bark_mid;
	  lof = -2.5*(bark_diff/BarkWidth - 0.5);
	  hif = bark_diff/BarkWidth + 0.5;
	  mdouble = lof < hif ? lof : hif;
	  mdouble = (mdouble < 0) ? mdouble : 0; 
	  mdouble = pow(10,mdouble);
	  wts[i][j] = mdouble;
      }
    }

    free(binbarks);
    return wts;
}

static double* GetHammingWindow(int length){
  double *window = (double*)malloc(length*sizeof(double));

  int i;
  for (i = 0;i < length;i++){
      window[i] = 0.54 - 0.46*cos(2*PI*i/(length-1));
  }

  return window;
}

static void sort_barkdiffs(double *barkdiffs,uint8_t *bits,unsigned int length){
    int i, minpos;
    for (i=0;i<(int)length;i++){
	minpos = i;
	int j;
	for (j=i+1;j<(int)length;j++){
	    if (barkdiffs[j] < barkdiffs[minpos])
		minpos = j;
	}
	if (i != minpos){
	    double tmpdiff = barkdiffs[i];
	    barkdiffs[i] = barkdiffs[minpos];
	    barkdiffs[minpos] = tmpdiff;

	    double tmpbit = bits[i];
	    bits[i] = bits[minpos];
	    bits[minpos] = tmpbit;
	}
    }
}

int getframelength(int sr, float duration){
    int count = 0, nbsamples = (int)(duration*(float)sr);
    while (nbsamples != 0){
	nbsamples >>= 1;
	count++;
    }
    count--;
    return (0x0001 << count);
}

int audiohash(float *buf, uint32_t **hash, double ***coeffs, uint8_t ***toggles, 
              unsigned int *nbcoeffs, unsigned int *nbframes, double *minB, double *maxB, 
              unsigned int buflen, unsigned int P, int sr, AudioHashStInfo **hash_st){
    const float dur = 0.40f;
    int framelength, nfft, nfft_half;

    if (buf == NULL || nbframes == NULL || hash == NULL || buflen == 0 || hash_st == NULL || sr < 6000) return -1;

    int i,j;
    if (*hash_st == NULL){
	*hash_st = (AudioHashStInfo*)malloc(sizeof(AudioHashStInfo));
	framelength = getframelength(sr, dur);
	(*hash_st)->framelength = framelength;
	(*hash_st)->window = GetHammingWindow(framelength);
	(*hash_st)->wts = GetWts(sr, framelength/2);
    }
    
    framelength = (*hash_st)->framelength;
    nfft = framelength;
    nfft_half = framelength/2;
    double *window = (*hash_st)->window;
    double **wts   = (*hash_st)->wts;
    double *frame = (double*)malloc(framelength*sizeof(double));
    PHComplex *pF = (PHComplex*)malloc(nfft*sizeof(PHComplex));
    double *magnF = (double*)malloc(nfft_half*sizeof(double));
    double *barkdiffs = (double*)malloc((nfilts-1)*sizeof(double));

    int start = 0;
    int end = start + framelength - 1;
    int overlap = 31*framelength/32;
    int advance = framelength - overlap;
    int totalframes = (int)(floor(buflen/advance) - floor(framelength/advance) + 1);
    int nbhashes = totalframes - 2;
    *nbframes = nbhashes;

    *hash = (uint32_t*)calloc(nbhashes,sizeof(uint32_t));
    uint8_t *tmptoggles = NULL;
    if (P > 0 && toggles){
	 tmptoggles = (uint8_t*)malloc((nfilts-1)*sizeof(uint8_t));
	 *toggles = (uint8_t**)malloc(nbhashes*sizeof(uint8_t*));
    }

    double **barkcoeffs = (double**)malloc(totalframes*sizeof(double*));
    for (i = 0;i < totalframes;i++){
	barkcoeffs[i] = (double*)malloc(nfilts*sizeof(double));
    }

    if (coeffs && nbcoeffs) { 
	*coeffs =  barkcoeffs;
	*nbcoeffs = nfilts;
    }

    int index = 0;
    double maxF, minbark = 10000000000000.0, maxbark = 0.0;
    while (end < buflen){
	maxF = 0.0;
	for (i = 0;i < framelength;i++){
	    frame[i] = window[i]*buf[start+i];
	}

	fft(frame, framelength, pF);
	
	for (i = 0;i < nfft_half;i++){
	    magnF[i] = complex_abs(pF[i]);
	    /* channel normalization ??? */
	    /*  magnF[i] = log(magnF[i]; */
	    if (magnF[i] > maxF){
		maxF = magnF[i];
	    }
	}
	
	/* channel normalization ??? */
	/*
	for (i = 0;i < nfft_half;i++){
	    magnF[i] = exp(magnF[i]/maxF) + 1;
	}
	*/

	/* critical band integration */
	for (i = 0;i < nfilts;i++){
	    barkcoeffs[index][i] = 0.0;
	    for (j = 0;j < nfft_half;j++){
		barkcoeffs[index][i] += wts[i][j]*magnF[j];
	    }
	    if (barkcoeffs[index][i] > maxbark){
		maxbark = barkcoeffs[index][i];
	    }
	    if (barkcoeffs[index][i] < minbark){
		minbark = barkcoeffs[index][i];
	    }
	}

	index += 1;
	start += advance;
	end += advance;
    }

    index = 0;
    for (i = 1;i < totalframes - 1;i++){
	uint32_t hashvalue = 0;
	int m;
	for (m=0;m < nfilts-1;m++){
	    double diff = (barkcoeffs[i+1][m] - barkcoeffs[i+1][m+1]) - (barkcoeffs[i-1][m] - barkcoeffs[i-1][m+1]);
	    if (tmptoggles) tmptoggles[m] = (uint8_t)m;
	    hashvalue <<= 1;
	    if (diff > 0){
		hashvalue |= 0x00000001;
	    }
	    barkdiffs[m] = abs(diff);
	}

	if (P > 0 && toggles){
	    sort_barkdiffs(barkdiffs, tmptoggles, nfilts-1);
	    (*toggles)[index] = (uint8_t*)malloc(P*sizeof(uint8_t));
	    for (m = 0;m < P;m++){
		(*toggles)[index][m] = tmptoggles[m];
	    }
	}
	(*hash)[index++] = hashvalue;
    }

    if (coeffs == NULL){
	for (i = 0;i < totalframes;i++){
	    free(barkcoeffs[i]);
	}
	free(barkcoeffs);
    }

    free(tmptoggles);
    free(barkdiffs);
    free(frame);
    free(magnF);
    free(pF);
    
    return 0;
}

#ifndef JUST_AUDIOHASH

static int GetCandidates2(uint32_t hashvalue, uint8_t *toggles, const unsigned int P, uint32_t **pcands, int *nbcandidates){
    const int NbCandidates = 32;
    *nbcandidates = NbCandidates + 1;
    *pcands = (uint32_t*)malloc((NbCandidates+1)*sizeof(uint32_t));
    (*pcands)[0] = hashvalue;
    
    uint32_t mask = 0x80000000;
    int i;
    for (i = 0;i < NbCandidates;i++){
	uint32_t currentValue = hashvalue;
	currentValue = TOGGLE_BIT(currentValue, i);
	(*pcands)[i+1] = currentValue;
    }

    return 0;
}

static int GetCandidates(uint32_t hashvalue, uint8_t *toggles, const unsigned int P, uint32_t **pcands, int *nbcandidates){
    int n = 1 << P;
    n = (n > 0) ? n : 1;
    *nbcandidates = n;

    *pcands = (uint32_t*)malloc(n*sizeof(uint32_t));

    (*pcands)[0] = hashvalue;
    int index;
    for (index = 1;index < n;index++){
	uint32_t currentvalue = hashvalue;
	int perms = index;
	int bitnum = 0;
	while (perms != 0) {
	    if (perms & 0x00000001){
		currentvalue = TOGGLE_BIT(currentvalue, toggles[bitnum]);
	    }
	    bitnum++;
	    perms >>= 1;
	}

	(*pcands)[index] = currentvalue;
    }

    return 0;
}

PHASH_EXPORT
int lookupaudiohash(AudioIndex index_table,uint32_t *hash,uint8_t **toggles, int nbframes,\
                    int P, int blocksize,float threshold, uint32_t *id, float *cs){

    int max_results = 3*blocksize, nbresults = 0, max_cnt = 0, max_pos = 0, total = 0, error = 0;
    int i,j,k,m, nbcandidates, already_added;
    uint32_t *results = (uint32_t*)malloc(max_results*sizeof(uint32_t));
    uint32_t *last_positions = (uint32_t*)malloc(max_results*sizeof(uint32_t));
    uint32_t *subhash, *candidates;
    uint8_t *curr_toggles;
    int *result_cnts = (int*)malloc(max_results*sizeof(int));
    float lvl;
    TableValue *lookup_val;

    if (results == NULL || last_positions == NULL || result_cnts == NULL){
	return -1;
    }
    *id = 0;
    *cs = 0.0;
    for (i=0;i<nbframes-blocksize+1;i+=blocksize){
	subhash = hash+i;
	for (j=0;j<blocksize;j++){
	    curr_toggles = (toggles) ? toggles[i+j] : NULL;

	    /* expand hash candidates */
	    GetCandidates(subhash[j], curr_toggles, P, &candidates, &nbcandidates); 
	    /* GetCandidates2(subhash[j], curr_toggles, P, &candidates, &nbcandidates); */ 

	    for (k = 0;k < nbcandidates; k++){
		lookup_val = NULL;
		error = table_retrieve((table_t*)index_table, &candidates[k],sizeof(uint32_t),\
                                           (void*)&lookup_val,NULL); 
		if (lookup_val != NULL){
		    already_added = 0;
		    for (m=0;m<nbresults;m++){
			if (results[m] == lookup_val->id &&\
			    lookup_val->pos > last_positions[m] &&\
                            lookup_val->pos <= last_positions[m]+ 2*blocksize ){
			    result_cnts[m]++;
			    last_positions[m] = lookup_val->pos;
			    total++;
			    if (result_cnts[m] > max_cnt){
				max_cnt = result_cnts[m];
				max_pos = m;
			    }
			    already_added = 1;
			    break;
			}
		    }
		    if (!already_added && nbresults < max_results){
			results[nbresults] = (uint32_t)lookup_val->id;
			last_positions[nbresults] = (uint32_t)lookup_val->pos;
			result_cnts[nbresults] = 1;
			total++;
			if (result_cnts[nbresults] > max_cnt){
			    max_cnt = result_cnts[nbresults];
			    max_pos = nbresults;
			}
			nbresults++;
		    }
		}
	    }

	    free(candidates);

	}
	lvl = (float)max_cnt/(float)blocksize;
	if (lvl >= threshold){
	    break;
	}
    }

    if (nbresults > 0 && lvl >= threshold){
	*id = results[max_pos];
	*cs = lvl;
    } 
    
    free(results);
    free(result_cnts);
    free(last_positions);

    return 0;
}

#endif /* JUST_AUDIOHASH*/
