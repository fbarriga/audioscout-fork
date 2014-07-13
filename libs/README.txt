pHashAudio AudioData libraries
----------------------------------------------------------

libAudioData is a c library for reading audio files and 
converting them to a given sample rate.  It also reads
the the files metadata v1 and v2 tags.  Supported file formats
include: .wav, .ogg, .flac, .mp3 and anything read by libsndfile.

libpHashAudio is a c library for calculating audio hash values for 
a given signal.  

INSTALL
---------------------------------------------------------------
For linux builds,  use the standard generator.

cmake .
make all
make install 

WINDOWS
---------------------------------------------------------------

The libraries can be compiled on mingw or on msvc.  For msvc,
use the cmake build file with the "Nmake Makefiles" generator:
(Be sure to call vcvars32.bat script to set the environment variables.
Also, these compiler utilities, such as nmake, must be on your path.)

    cmake -g "Nmake Makefiles" .
    nmake
    nmake install

However, this is the default makefile generator on the windows 
shell.  You will probably wish to change the local install variable 
in the CMakeLists.txt file. Comments in the file indicate which that is.
This call should build the bindings as well.

Note: In order call the functions from the c# bindings, you compile with
the _UNICODE flag.  

The produced files of concern for dev are: pHashAudio.dll, AudioData.dll,
phash_audio.h audiodata.h, pHashAudio.lib, AudioData.lib.  


DEPENDENCIES
---------------------------------------------------------

zeromq 2.1 (www.zeromq.org)

libsndfile v1.0.25 (www.mega-nerd.com/libsndfile)

libsamplerate v0.1.8 (www.mega-nerd.com/SRC)

optional:

mpg123 v1.13.4 (www.mpg123.de) 

libogg 1.3.0  (xiph.org/downloads) 
libvorbis 1.3.2
libflac 1.2.1
