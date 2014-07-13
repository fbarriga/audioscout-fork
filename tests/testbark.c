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
#include <math.h>
#include <plot.h>

static const unsigned int nfilts = 33;
static const double barkfreqs[33] = { 25.0, 50.0, 100.0, 150.0, 200.0, 250.0, 300.0, 350.0,\
                                      400.0, 450.0, 510.0, 570.0, 635.0, 700.0, 770.0,\
				      840.0, 920.0, 1000.0, 1085.0, 1170.0, 1270.0, 1370.0,\
                                      1485.0, 1600.0, 1725.0, 1850.0, 2000.0, 2150.0, 2325.0,\
                                      2500.0, 2700.0, 2900.0, 3000.0 };

static const char* colors[16] = {"red","blue", "green", "black", "gray", "navy", "yellow",\
				 "DarkGreen", "khaki", "brown", "beige", "DarkSalmon","firebrick",\
                                 "peru", "DeepPink", "magenta"};

int main(int argc, char **argv ){

    const double minfreq = 0.0;
    const double maxfreq = 3000.0;
    const unsigned int nfft = 2048;
    const unsigned int nfft_half = nfft/2;
    double barkwidth = 1.06;
    if (argc > 1) barkwidth = atof(argv[1]);
    printf("barkwidth = %f\n", barkwidth);


    double wts[nfilts][nfft_half];

    double binbarks[nfft_half];
    double temp;
    unsigned int i,j;
    for (i = 0; i< nfft_half;i++){
	temp = i*maxfreq/nfft_half/600.0;
	binbarks[i] = 6*log(temp + sqrt(temp*temp + 1.0));
    }

    double lof, hif, m;
    for (i = 0;i < nfilts; i++){
	double f_bark_mid = barkfreqs[i]/600.0;
	f_bark_mid = 6*log(f_bark_mid + sqrt(f_bark_mid*f_bark_mid + 1.0));
	for (j = 0; j < nfft_half;j++){
	    double barkdiff = binbarks[j] - f_bark_mid;
	    lof = -2.5*(barkdiff/barkwidth - 0.5);
	    hif = barkdiff/barkwidth + 0.5;
	    m = lof < hif ? lof : hif;
	    m = (m < 0) ? m : 0;
	    m = pow(10,m);
	    wts[i][j] = m;
	}

    }

    pl_parampl("BITMAPSIZE", "650x200");
    int handle = pl_newpl("X", stdin, stdout, stderr);
    if (handle < 0){
	printf("unable to get handle to plotter\n");
	exit(1);
    }

    pl_selectpl(handle);
    if (pl_openpl() < 0){
	printf("unable to open plotter\n");
	exit(1);
    }
    pl_fspace(0.0,0.0,nfft_half, 1.10);
    pl_flinewidth(0.30);
   
    for (i=0;i<nfilts;i++){
	pl_flinewidth(0.30);
	pl_pencolorname(colors[i%16]);
	pl_fmove(0.0,0.0);
	double max = 0.0;
	double min = 1000000.0;
	for (j = 0;j<nfft_half;j++){
	    pl_fcont(j, wts[i][j]);
	}
    }
    pl_selectpl(0);
    pl_deletepl(handle);
	



    return 0;
}
