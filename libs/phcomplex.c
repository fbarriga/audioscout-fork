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


#include "phcomplex.h"

PHComplex polar_to_complex(const double r, const double theta){
    PHComplex result;
    result.re = r*cos(theta);
	result.im = r*sin(theta);
    return result;
}
PHComplex add_complex(const PHComplex a, const PHComplex b){
    PHComplex result;
    result.re = a.re + b.re;
    result.im = a.im + b.im;
    return result;
}
PHComplex sub_complex(const PHComplex a, const PHComplex b){
	PHComplex result;
	result.re = a.re - b.re;
	result.im = a.im - b.im;
	return result;
}
PHComplex mult_complex(const PHComplex a, const PHComplex b){
	PHComplex result;
	result.re = (a.re*b.re) - (a.im*b.im);
    	result.im = (a.re*b.im) + (a.im*b.re);
	return result;
}
double complex_abs(const PHComplex a){
  return sqrt(a.re*a.re + a.im*a.im);
}

