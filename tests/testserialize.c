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
#include "serialize.h"


int main(int argc, char **argv){

    uint32_t s = 0x01020304;
    uint32_t t, u;

    t = hosttonet32(s);
    u = nettohost32(t);

    printf("s = %x, t = %x, u = %x \n", s, t, u);
    
    uint32_t nb = 2781;
    uint32_t nb2 = hosttonet32(nb);
    uint32_t nb3 = nettohost32(nb2);

    printf("nb = %u => %u => %u\n", nb, nb2, nb3);
    printf("nb = %x => %x => %x\n", nb, nb2, nb3);
    uint32_t nbframes = 2573;
    uint32_t nbframes2 = 2781;
    printf(" 2573 = %x\n", nbframes);
    printf(" 2781 = %x\n", nbframes2);


    return 0;
}
