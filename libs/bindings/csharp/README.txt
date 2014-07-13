C# bindings to pHashAudio.dll and AudioData.dll
------------------------------------------------------

This project contain the files for building the c#
bindings to pHashAudio.dll and AudioData.dll



INSTALLATION
--------------------------------------------------------------
You will need to install cmake. Make sure the cmake executable
is on your PATH variable.

To build the project, simply open a windows command shell:
(Run vcvars32.bat script in the shell first.)

   cmake -g "NMake Makefiles" .
   nmake all

The files produced should be pHashAudio-ni.dll for 
managed code interface to native bindings and a TestNI.exe
executable to test the bindings.  Run this with the
name of an audio file to test the read and hash functions.

Note: The AudioData.dll and pHashAudio.dll must be in your
PATH env variable.

Test the bindings out by running TestNI.exe executable.  It 
takes the name of an audio file as argument.  Make sure
the dir in which the dependency dll's are in is on the 
PATH env variable.  


DEPENDENCIES
------------------------------------------------------

.NET Framework v4.0
