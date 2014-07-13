#!/bin/bash

if [ $# -lt 1 ]; then
	echo "$0 <arch> [base install path]"
	echo "arch like powerpc-unknown-linux-gnu"
	exit
fi

BASE_PATH=$2

if [ -z $BASE_PATH ]; then
	BASE_PATH="$HOME/AudioScout-$1"
fi

if [ ! -d $BASE_PATH ]; then
	mkdir -p $BASE_PATH
fi

deps_list=( e2fsprogs-1.41.12 libogg-1.2.1 libvorbis-1.3.2 flac-1.2.1 libsndfile-1.0.23 \
		libsamplerate-0.1.7 sqlite-3.7.3 mpg123-1.12.5 zeromq-2.0.10 )

declare -A dep_dirs

CC="$HOME/x-tools/$1/bin/$1-gcc"
SAVE_PATH="$PATH"
PATH="$HOME/x-tools/$1/bin:$PATH"

dep_dirs[mpg123-1.12.5]=""
dep_dirs[e2fsprogs-1.41.12]=""
dep_dirs[libogg-1.2.1]=""
dep_dirs[libvorbis-1.3.2]="OGG_CFLAGS=-I$BASE_PATH/include OGG_LIBS='-L$BASE_PATH/lib -logg'"
dep_dirs[flac-1.2.1]=""
dep_dirs[libsndfile-1.0.23]="FLAC_CFLAGS=-I$BASE_PATH/include FLAC_LIBS='-L$BASE_PATH/lib -lFLAC' \
	OGG_CFLAGS=-I$BASE_PATH/include OGG_LIBS='-L$BASE_PATH/lib -logg' VORBIS_CFLAGS=-I$BASE_PATH/include \
  	VORBIS_LIBS='-L$BASE_PATH/lib -lvorbis -lvorbisfile' VORBISENC_CFLAGS=-I$BASE_PATH/include \
	VORBISENC_LIBS='-L$BASE_PATH/lib -lvorbisenc'"

dep_dirs[libsamplerate-0.1.7]="SNDFILE_CFLAGS=-I$BASE_PATH/include SNDFILE_LIBS='-L$BASE_PATH/lib -lsndfile'"
dep_dirs[sqlite-3.7.3]="CFLAGS=-I$BASE_PATH/include LDFLAGS=-L$BASE_PATH/lib"
dep_dirs[zeromq-2.0.10]="CFLAGS=-I$BASE_PATH/include LDFLAGS=-L$BASE_PATH/lib CPPFLAGS=-I$BASE_PATH/include"

declare -A dep_flags

dep_flags[e2fsprogs-1.41.12]="--disable-debugfs --disable-imager --disable-resizer --disable-uuidd \
				--disable-testio-debug --disable-nls"
dep_flags[libogg-1.2.1]=""
dep_flags[libvorbis-1.3.2]="--disable-oggtest"
dep_flags[flac-1.2.1]="--disable-cpplibs --disable-xmms-plugin --with-ogg-libraries=$BASE_PATH/lib \
			--with-ogg-includes=$BASE_PATH/include"
dep_flags[libsndfile-1.0.23]="--disable-sqlite"
dep_flags[libsamplerate-0.1.7]="--disable-fftw"
dep_flags[sqlite-3.7.3]=""
dep_flags[mpg123-1.12.1]="--enable-static"
dep_flags[zeromq-2.0.10]=""

HASHES=${#deps_list[@]}

COUNT=1

for x in ${deps_list[@]}
do
	cd "$HOME/$x"
	echo
	echo "************** Building $x **************"
	echo "CC=$CC ${dep_dirs[$x]} ./configure --prefix=$BASE_PATH --host=$1 \
		${dep_flags[$x]}"
	echo
	sleep 5
	let HASHMARKS=$COUNT*$HASHES
	echo -n "Progress: ["
	for (( i=1; i<= $COUNT; i++ ))
	do
		echo -n "#"
	done
	for (( i=$HASHES; i > $COUNT; i-- ))
	do
		echo -n " "
	done
	echo "] $COUNT/$HASHES"
	make distclean > /dev/null 2>&1
	eval CC="$CC" "${dep_dirs[$x]}" ./configure --prefix=$BASE_PATH --host=$1 \
		${dep_flags[$x]} > /dev/null 2>&1
	if [ ${x:0:9} = "e2fsprogs" ]; then
		cd lib/uuid
	fi
	make > /dev/null 2>&1
	make install > /dev/null 2>&1
	let COUNT+=1
done

PATH="$SAVE_PATH"

echo
echo
echo "************** AudioScout Build complete! **************"
echo "Files installed in $BASE_PATH"
echo
echo
