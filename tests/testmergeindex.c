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
#include <phash_audio.h>

int main(int argc, char **argv){
    if (argc < 3){
	printf("not enough input args\n");
	printf("usage: %s dstindexfile srcindexfile\n", argv[0]);
	exit(1);
    }
    const char *dstfile = argv[1];
    const char *srcfile = argv[2];

    printf("merging %s into %s\n", srcfile, dstfile);
    int err = merge_audioindex(dstfile, srcfile);
    if (err < 0){
	printf("could not merge successfully: %d\n", err);
    } else {
	printf("merged %d \n", err);
    }
    printf("done\n");

    return 0;
}
